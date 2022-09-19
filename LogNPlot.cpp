#include "LogNPlot.h"

LogNPlot::LogNPlot(const char* _parameterNames[], const int _numberOfParameters, const char* _ssid, const char* _password, const char* _dataloggerName) : server(80) {
  ssid = _ssid;
  password = _password;
  parameterNames = _parameterNames;
  numberOfParameters = _numberOfParameters;
  dataloggerName = _dataloggerName;
  sumOfValues.resize(_numberOfParameters);
  sumOfSquares.resize(_numberOfParameters);
  numberOfSamples.resize(_numberOfParameters);
  std::fill(sumOfValues.begin(), sumOfValues.end(), 0.0);
  std::fill(sumOfSquares.begin(), sumOfSquares.end(), 0.0);
  std::fill(numberOfSamples.begin(), numberOfSamples.end(), 0);
}

bool LogNPlot::begin(){
  // Initialize SPIFFS
  if(!SPIFFS.begin()){
#ifndef LOGNPLOT_SILENT
    Serial.println("An Error has occurred while mounting SPIFFS");
#endif
    return false;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
#ifndef LOGNPLOT_SILENT
    Serial.println("Connecting to WiFi..");
#endif
  }

  if(!MDNS.begin(dataloggerName)) {
#ifndef LOGNPLOT_SILENT
     Serial.println("Error starting mDNS");
#endif
     return false;
  }

  // Print ESP32 Local IP Address
#ifndef LOGNPLOT_SILENT
  Serial.println(WiFi.localIP());
#endif

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  for(int j=0; j<numberOfParameters; j++){
    server.on(parameterNames[j], HTTP_GET, [&,j](AsyncWebServerRequest *request){
      request->send(200, "text/plain", payload(j));
    });
  }
  server.on("/slow", HTTP_GET, [&] (AsyncWebServerRequest *request) {
    NewSamplingPeriod = 5;
    NewMeasurementPeriod = 600;
    request->send(200, "text/plain", "Set to slow mode.");
  });
  server.on("/quick", HTTP_GET, [&] (AsyncWebServerRequest *request) {
    NewSamplingPeriod = 2;
    NewMeasurementPeriod = 60;
    request->send(200, "text/plain", "Set to quick mode. (Mostly for debugging.)");
  });
  server.on("/list", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", list_filenames());
  });
  server.onNotFound(notFound);

  delay(2000);
  configTime(Offset_sec, 0, ntpServer);
  delay(1000);
  time(&last_datetime);
#ifndef LOGNPLOT_SILENT
  Serial.println("Time of internal RTC set to \"" + time_t2datetime(last_datetime) + "\".");
#endif
  
  // Start server
  server.begin();
  return true;
}

double LogNPlot::avg(double sum, int N){
  return sum/N;
}

double LogNPlot::stdev(double sum,double square, int N){
  return sqrt((square-pow(sum,2)/N)/(N-1));
}

bool LogNPlot::getTimeRightNow(time_t _set_time)
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return 0;
  }
  _set_time = mktime(&timeinfo);
  return 1;
}

String LogNPlot::time_t2datetime(time_t trn_t)
{
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&trn_t));
  return buffer;
}

String LogNPlot::time_t2date(time_t trn_t)
{
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&trn_t));
  return buffer;
}

void LogNPlot::loadData(fs::FS &fs, time_t until_this_datetime, int parameter) {
  File this_day = fs.open("/Data_"+time_t2date(until_this_datetime)+".csv", FILE_READ);
  File preceding_day = fs.open("/Data_"+time_t2date(until_this_datetime-SECS_PER_DAY)+".csv", FILE_READ);
  datetime.clear();
  AVG.clear();
  STD.clear();
  bool firstLine = true;
  while (preceding_day.available()) {
    int c;
    if (firstLine) {
      c = preceding_day.read();
      if (c == '\n') {
        firstLine = false;
      }
    } 
    else {
      String proto = preceding_day.readStringUntil(',');
      proto.trim();
      struct tm read_time;
      strptime(proto.c_str(),"%Y-%m-%d %H:%M:%S",&read_time);
      datetime.push_back(mktime(&read_time));
      if(parameter > 0)
      {
       for(int i = 0; i < parameter; i++)
        {
          preceding_day.readStringUntil(',');
          preceding_day.readStringUntil(',');
        }
      }
      char peek_buffer[16];
      preceding_day.readBytesUntil(',', peek_buffer, 16);
      if(strcmp(peek_buffer, "nan")  == 0){
        datetime.pop_back();
        preceding_day.readStringUntil('\n');
      }
      else{
        AVG.push_back(atof(peek_buffer));
        STD.push_back(preceding_day.parseFloat());
        preceding_day.readStringUntil('\n');
      }
      memset(peek_buffer, 0, sizeof(peek_buffer));
    }
  }
  preceding_day.close();
  firstLine = true;
  while (this_day.available()) {
    int c;
    if (firstLine) {
      c = this_day.read();
      if (c == '\n') {
        firstLine = false;
      }
    } else {
      String proto = this_day.readStringUntil(',');
      proto.trim();
      struct tm read_time;
      strptime(proto.c_str(),"%Y-%m-%d %H:%M:%S",&read_time);
      datetime.push_back(mktime(&read_time));
      if(parameter > 0)
      {
        for(int i = 0; i < parameter; i++)
        {
          this_day.readStringUntil(',');
          this_day.readStringUntil(',');
        }
      }
      char peek_buffer[16];
      this_day.readBytesUntil(',', peek_buffer, 16);
      if(strcmp(peek_buffer, "nan")  == 0){
        datetime.pop_back();
        this_day.readStringUntil('\n');
      }
      else{
        AVG.push_back(atof(peek_buffer));
        STD.push_back(this_day.parseFloat());
        this_day.readStringUntil('\n');
      }
      memset(peek_buffer, 0, sizeof(peek_buffer));
    }
  }
  this_day.close();
}

char* LogNPlot::payload(int parameter)
{ 
  loadData(SPIFFS,last_datetime,parameter);
  strcpy(payload_buffer,"{\"Data\":[");
  int number = min((int) AVG.size(),6*24);
  int offset = AVG.size()-number;
  int digits = 3;
  for(int i = 0; i < number; i++)
  {
    if(i > 0){
      strcat(payload_buffer,",");
    }
    String temp = "[" + String(offset+i) + ",\"" + time_t2datetime(datetime[offset+i]) + "\",\"" + String(AVG[offset+i],digits) + "\",\"" + String(STD[offset+i],digits) + "\"]";
    strcat(payload_buffer,temp.c_str());
  }
  strcat(payload_buffer,"]}");
  return payload_buffer;
}

void LogNPlot::writeFile(fs::FS &fs, const char * path, const char * message) {
#ifndef LOGNPLOT_SILENT
  Serial.printf("Writing file: %s\n", path);
#endif

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
#ifndef LOGNPLOT_SILENT
    Serial.println("Failed to open file for writing");
#endif
    return;
  }
  if(file.print(message)) {
#ifndef LOGNPLOT_SILENT
    Serial.println("File written");
#endif
  } else {
#ifndef LOGNPLOT_SILENT
    Serial.println("Write failed");
#endif
  }
  file.close();
}

void LogNPlot::appendFile(fs::FS &fs, const char * path, const char * message) {
#ifndef LOGNPLOT_SILENT
  Serial.printf("Appending to file: %s\n", path);
#endif

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
#ifndef LOGNPLOT_SILENT
    Serial.println("Failed to open file for appending");
#endif
    return;
  }
  if(file.print(message)) {
#ifndef LOGNPLOT_SILENT
    Serial.print("Message appended:");
    Serial.println(message);
#endif
  } else {
#ifndef LOGNPLOT_SILENT
    Serial.println("Append failed");
#endif
  }
  file.close();
}

void LogNPlot::addSampleAtPosition(double value, int position){
  sumOfValues[position] += value;
  sumOfSquares[position] += pow(value,2);
  numberOfSamples[position]++;
}


String LogNPlot::list_filenames(){
  String temp;
  File root = SPIFFS.open("/");
 
  File file = root.openNextFile();
 
  while(file){
 
      temp+= file.name();
      temp+="\n";
 
      file = root.openNextFile();
  }
  return temp;
}


void LogNPlot::checkConnection(){
  if(WiFi.status() != WL_CONNECTED){
#ifndef LOGNPLOT_SILENT
    Serial.println("Reconnecting to WiFi...");
#endif
    WiFi.disconnect();
    WiFi.begin(ssid,password);
  }
  server.end();
  delay(1000);
  // remove old files in order not to fill up the SPIFFS memory
  int tBytes = SPIFFS.totalBytes();
  int uBytes = SPIFFS.usedBytes();
  float percentUsed = 100.0*((float) uBytes)/((float) tBytes);
#ifndef LOGNPLOT_SILENT
  Serial.println("SPIFFS "+String(percentUsed,2)+"% used.");
#endif
  std::vector<String> dataFilenames;
  if(percentUsed > 70.0){
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
#ifndef LOGNPLOT_SILENT
      Serial.print("Processing ");
      Serial.println(file.name());
#endif
      if(!strcmp(strrchr(file.name(), '\0') - 4, ".csv")){
        dataFilenames.push_back(file.name());
      }
      file = root.openNextFile();
    }
    std::sort(dataFilenames.begin(), dataFilenames.end(), [](const String & a, const String & b) -> bool
    {
        return a < b;
    });
  }
  int i=0;
  while(percentUsed > 70.0 && i<dataFilenames.size()){
    SPIFFS.remove(dataFilenames[i]);
#ifndef LOGNPLOT_SILENT
    Serial.print("Removed ");
    Serial.println(dataFilenames[i]);
#endif
    tBytes = SPIFFS.totalBytes();
    uBytes = SPIFFS.usedBytes();
    percentUsed = 100.0*((float) uBytes)/((float) tBytes);
#ifndef LOGNPLOT_SILENT
    Serial.println("SPIFFS "+String(percentUsed,2)+"% used.");
#endif
    i++;
  }
  server.begin();
}

void LogNPlot::saveMeasurement(time_t _datetime){
  String filename_now = "/Data_" + time_t2date(last_datetime) + ".csv";
  File file_test = SPIFFS.open(filename_now.c_str());
  bool file_exists = file_test;
  file_test.close();
  if(!file_exists) {
#ifndef LOGNPLOT_SILENT
    Serial.println("File doens't exist");
    Serial.println("Creating file \"" + filename_now + "\".");
#endif
    writeFile(SPIFFS, filename_now.c_str(), "Datetime");
    for(int j=0; j<numberOfParameters; j++){
      writeFile(SPIFFS, filename_now.c_str(), ",");
      writeFile(SPIFFS, filename_now.c_str(), parameterNames[j]);
      writeFile(SPIFFS, filename_now.c_str(), ",");
    }
  }
  String dataMessage = "\r\n" + time_t2datetime(_datetime);
  for(int i=0; i<numberOfParameters; i++){
    dataMessage+= "," + String(avg(sumOfValues[i],numberOfSamples[i]),3) + "," + String(stdev(sumOfValues[i],sumOfSquares[i],numberOfSamples[i]),3);
  }
  dataMessage+= ";";
  appendFile(SPIFFS, filename_now.c_str(), dataMessage.c_str());
  SamplingPeriod = NewSamplingPeriod;
  MeasurementPeriod = NewMeasurementPeriod;
  std::fill(sumOfValues.begin(), sumOfValues.end(), 0.0);
  std::fill(sumOfSquares.begin(), sumOfSquares.end(), 0.0);
  std::fill(numberOfSamples.begin(), numberOfSamples.end(), 0);
}
