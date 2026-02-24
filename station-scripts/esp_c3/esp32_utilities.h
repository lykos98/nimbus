#pragma once
#include "secrets.h"
#include "esp_wifi_types.h"
#include "pins_arduino.h"
#include "esp32-hal-gpio.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h> 
#include <ArduinoMqttClient.h>
#include <Wire.h>

#ifndef RESET_PIN
  #define RESET_PIN GPIO_NUM_4
#endif

#ifndef SCL_PIN
  #define SCL_PIN GPIO_NUM_9
#endif

#ifndef SDA_PIN
  #define SDA_PIN GPIO_NUM_8
#endif

#define AS5600_ADDRESS 0x36 // I2C address of the AS5600
                            
#define LOG_BUFFER_SIZE 512
#define LOGF(fmt, ...) do { \
    char _log_buf[LOG_BUFFER_SIZE]; \
    snprintf(_log_buf, LOG_BUFFER_SIZE, fmt, ##__VA_ARGS__); \
    Serial.printf("[%-8lu] %s\n", (unsigned long)millis(), _log_buf); \
} while (0)
//#define DEBUG

int read_angle_as5600_raw()
{
  Wire.end();
  Wire.begin(SDA_PIN, SCL_PIN);

  auto& w = Wire;
  w.beginTransmission(AS5600_ADDRESS);
  w.write(0x0C); // Angle register address
  w.endTransmission();

  w.requestFrom(AS5600_ADDRESS, 2); // Request 2 bytes of data
  delay(100);
  // Read angle data
  int angle;
  float angleDegrees = 0.;
  if (w.available() >= 2) {
    byte highByte = w.read();
    byte lowByte = w.read();

    // Combine high and low bytes into a 12-bit angle value
    angle = (highByte << 8) | lowByte;

    // Convert angle to degrees (0-360)
    angleDegrees = (float)angle / 4096.0 * 360.0;
  }
  return angle;
}

/* ---- CERTIFICATE RETRIVAL AND BROKER CONFIGURATION ---- */


WiFiClientSecure wifiClient;
MqttClient mqttClient(wifiClient);

#define MAX_STR_LEN 40

void print_status()
{
  switch (WiFi.status()) 
    {
      case WL_CONNECTED:
        LOGF("WL_CONNECTED");
        break;
      case WL_NO_SHIELD:
        LOGF("WL_NO_SHIELD");
        break;
      case WL_IDLE_STATUS:
        LOGF("WL_IDLE_STATUS");
        break;
      case WL_NO_SSID_AVAIL:
        LOGF("WL_NO_SSID_AVAIL");
        break;
      case WL_CONNECT_FAILED:
        LOGF("WL_CONNECT_FAILED");
        break;
      case WL_SCAN_COMPLETED:
        LOGF("WL_SCAN_COMPLETED");
        break;
      case WL_CONNECTION_LOST:
        LOGF("WL_CONNECTION_LOST");
        break;
      case WL_DISCONNECTED:
        LOGF("WL_DISCONNECTED");
        break;
      default:
        break;
    }
    return;
}

void blink_n_times(int n = 5)
{
  pinMode(LED_BUILTIN, OUTPUT);
  for(int i = 0; i < n; ++i)
  {
    //digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    //digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
 
}

void connect_to_wifi() {
  WiFi.mode(WIFI_STA);
  //WiFi.setTxPower(WIFI_POWER_5dBm);

  int try_count = 0;
  
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  LOGF("Connecting to WiFi...");
  unsigned long start_attempt_time = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start_attempt_time < 60000) {
    delay(1000);
    LOGF("%s", String(WiFi.status()).c_str());
    print_status();
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOGF(" Failed!");
    esp_sleep_enable_timer_wakeup(5 * 1000000);
    esp_deep_sleep_start();
  } else {
    LOGF(" Connected!");
  }
}


/* ---- Utility for reading accureately voltage ---- */

float read_avg_v(int pin)
{
  /** 
   * this function accounts for a small ammount of calibration between 
   * read values and actual voltage into the pin it works under the 
   * assumption the voltmeter is a resistance in parallel
   * w.r.t the voltage measured, change R_INTERNAL_CALIBRATION to 
   * modify the internal resistance calibration. By default takes 
   * N_SAMPLES_ADC samples for each measurement and averages them
   **/
  #define N_SAMPLES_ADC 100
  #define R_VDIVIDER 68.1
  #define R_INTERNAL_CALIBRATION 535.917

  float bv = 0;
  for(int i = 0; i < N_SAMPLES_ADC; ++i)
  {
    //bv += (float)analogRead(0);
    bv += (float)analogReadMilliVolts(pin);
  }

  float vi = bv/(float)N_SAMPLES_ADC;

  float battery_voltage = ((float)vi);
  float R = R_VDIVIDER;
  float RX = R_INTERNAL_CALIBRATION;

  float i2 = vi/R;
  float ix = vi / RX;
  float i1 = ix + i2;
  float v1 = R*i1;
  float vtot = v1 + vi;
  battery_voltage = vtot / 1000.;

  return battery_voltage;
}
