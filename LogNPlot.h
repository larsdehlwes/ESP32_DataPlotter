#include <ESPmDNS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <time.h>
#include <vector> 

#define SECS_PER_DAY 86400UL

class LogNPlot 
{
public:
  LogNPlot(const char* _parameterNames[], const int _numberOfParameters, const char* _ssid, const char* _password, const char* _dataloggerName);
  bool begin();
  void checkConnection();
  void saveMeasurement(time_t _datetime);
  String time_t2datetime(time_t trn_t);
  String time_t2date(time_t trn_t);
  bool getTimeRightNow(time_t _set_time);
  void addSampleAtPosition(double value, int position);
  int SamplingPeriod = 2;
  int MeasurementPeriod = 60;
  int NewSamplingPeriod = 2;
  int NewMeasurementPeriod = 60;
  time_t last_datetime;
protected:
  double avg(double sum, int N);
  double stdev(double sum,double square, int N);
  void loadData(fs::FS &fs, time_t until_this_datetime, int parameter);
  char* payload(int parameter);
  void writeFile(fs::FS &fs, const char * path, const char * message);
  void appendFile(fs::FS &fs, const char * path, const char * message);
  static void notFound(AsyncWebServerRequest *request) {
  if (request->url().endsWith(F(".csv"))) {
    Serial.println("Downloading " + request->url());
    request->send(SPIFFS, request->url(), String(), true);
  } else {
    request->send_P(404, PSTR("text/plain"), PSTR("Not found"));
  }
}
  String list_filenames();
private:
  const char* ssid;
  const char* password;
  const char* dataloggerName;
  const char** parameterNames;
  int numberOfParameters;
  const char* ntpServer = "south-america.pool.ntp.org";
  const long Offset_sec = -10800;
  AsyncWebServer server;
  char payload_buffer[10000];
  std::vector<time_t> datetime;
  std::vector<float> AVG;
  std::vector<float> STD;
  std::vector<int> numberOfSamples;
  std::vector<double> sumOfValues;
  std::vector<double> sumOfSquares;
};
