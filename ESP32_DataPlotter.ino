#include "LogNPlot.h"

/*BEGIN USER CODE - USER LIBRARIES*/
/*END USER CODE - USER LIBRARIES*/

/*BEGIN USER CODE - GLOBAL DECLARATIONS*/
/*END USER CODE - GLOBAL DECLARATIONS*/

/*BEGIN USER CODE - WIFI (Replace with your network credentials)*/
const char* ssid = "REGOSH";
const char* password = "Libre2022";
const char* DataPlotterName = "dataplotter";
/*END USER CODE - WIFI*/

/*BEGIN USER CODE - NAMES OF PARAMETERS*/
const int NumberOfParameters = 4;
const char* ParameterNames[NumberOfParameters] = {"/voltagePinA", "/voltagePinB", "/voltagePinC", "/hallSensorESP32"};
/*END USER CODE - NAMES OF PARAMETERS*/

LogNPlot logger(ParameterNames, NumberOfParameters, ssid, password, DataPlotterName);

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  serialIncFlush();
  serialOutFlush();
  /*BEGIN USER CODE - SETUP*/
  pinMode(32,INPUT);
  pinMode(33,INPUT);
  pinMode(34,INPUT);
  /*END USER CODE - SETUP*/
  logger.begin();
}
 
void loop(){
  logger.checkConnection();
  for(int i=0; i < logger.MeasurementPeriod/(2*logger.SamplingPeriod); i++){
    measure_all();
  }
  time_t timeRightNow;
  time(&timeRightNow);
  for(int i=0; i < logger.MeasurementPeriod/(2*logger.SamplingPeriod); i++){
    measure_all();
  }
  logger.saveMeasurement(timeRightNow);
}

void serialOutFlush()
{
  Serial.flush();
}

void serialIncFlush()
{
  while(Serial.available()) Serial.read();
}

void measure_all(){
  long t1 = millis();
  /*BEGIN USER CODE - Measuring a sample.*/
  int adc1, adc2, adc3;
  adc1 = analogRead(32);
  adc2 = analogRead(33);
  adc3 = analogRead(34);
  logger.addSampleAtPosition(adc1*0.000806,0);
  logger.addSampleAtPosition(adc2*0.000806,1);
  logger.addSampleAtPosition(adc3*0.000806,2);
  Serial.println("ADC: " + String(adc1) + " , " + String(adc2) + " , " + String(adc3));
  int hallValue;
  hallValue = hallRead();
  logger.addSampleAtPosition(hallValue,3);
  Serial.println("Hall Sensor: " + String(hallValue));
  /*END USER CODE - Measuring a sample.*/
  long t2 = millis();
  if(logger.SamplingPeriod*1000-(millis()-t1) > 0) delay(logger.SamplingPeriod*1000-(t2-t1));
}
