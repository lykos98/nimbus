//#define DEBUG
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ArduinoMqttClient.h>

#include "esp32_utilities.h"

Adafruit_BMP280 bmp; // I2C

//WiFiClient wifiClient;
WiFiClientSecure wifiClient;
MqttClient mqttClient(wifiClient);

//const char broker[] = "192.168.1.117";
const char broker[] = "lykos.cc";
//int        port     = 1883;
//int        port     = 1889;
int        port     = 8883;
const char topic[]  = "esp32/sensors";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  pinMode(0, INPUT);
  analogSetAttenuation(ADC_11db);
  
  Serial.begin(115200);
  Serial.println("nope");
 



  check_reset_condition();
  connect_to_wifi();

  check_certificate();
  String ca_cert = load_certificate();

  wifiClient.setCACert(ca_cert.c_str());
 
  
  connect_to_broker(mqttClient, broker, port);

  #ifdef DEBUG
    esp_sleep_enable_timer_wakeup(30 * 1000000);
  #else
    esp_sleep_enable_timer_wakeup(10 * 60 * 1000000);
  #endif



  char message[80];
 
  
  if(!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)){
      Serial.println("Cannot initialize bmp");
      esp_sleep_enable_timer_wakeup(5 * 1000000);
      esp_deep_sleep_start();
      

    }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                  Adafruit_BMP280::SAMPLING_X2,     
                  Adafruit_BMP280::SAMPLING_X16,    
                  Adafruit_BMP280::FILTER_X16,      
                  Adafruit_BMP280::STANDBY_MS_500); 


  float t = bmp.readTemperature();
  float p = bmp.readPressure();
  float h = 0;

  float battery_voltage = read_avg_v(0);

  sprintf(message, "{\"stationId\":\"%s\",\"airTemperature\":%.2f,\"airPressure\":%.2f,\"batteryV\":%.2f}",station_id.c_str(),t,p,battery_voltage);
  mqttClient.beginMessage(topic);
  mqttClient.print(message);
  mqttClient.endMessage();

  delay(500);

  
  Serial.println(message);
  Serial.println("Sending via mqtt");   
  

  WiFi.disconnect();
  esp_deep_sleep_start();

}


void loop() {
  
  //delay(3 * 60 * 1000);



}