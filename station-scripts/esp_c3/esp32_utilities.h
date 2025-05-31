#pragma once

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

const char* hash_url = "https://cert.lykos.cc/ca-cert-hash"; // URL for hash endpoint
const char* cert_url = "https://cert.lykos.cc/ca-cert";      // URL for full certificate

Preferences preferences;
WebServer server(80);  // Create a web server on port 80

#define DNS_PORT 53
DNSServer dns_server;

WiFiClientSecure wifiClient;
MqttClient mqttClient(wifiClient);

//const char broker[] = "192.168.1.117";
const char broker[] = "lykos.cc";
int        port     = 8883;
const char topic[]  = "esp32/sensors";

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

void retrieve_and_store_certificate(String new_cert_hash) 
{
    HTTPClient http;
    http.begin(cert_url);
    int http_code = http.GET();

    if (http_code == 200) {
        String certData = http.getString();  // Get the full certificate
        LOGF("Certificate retrieved from server: %s", certData.c_str());

        // Store the new certificate and its hash in flash memory
        preferences.begin("cert-storage", false);
        preferences.putString("ca-cert", certData);      // Store the certificate data
        preferences.putString("ca-cert-hash", new_cert_hash);  // Update the stored hash
        preferences.end();

        LOGF("Stored certificate and hash updated.");
    } 
    else 
    {
        LOGF("Failed to retrieve certificate, HTTP code: %d", http_code);
    }
}

void check_certificate() 
{
    // Step 1: Get the certificate hash from the server
    HTTPClient http;
    http.begin(hash_url);
    int http_code = http.GET();

    if (http_code == 200) 
    {
        String new_cert_hash = http.getString();  // Get the hash from server
        LOGF("Hash retrieved from server: %s", new_cert_hash.c_str());


        // Retrieve the stored hash from flash memory
        preferences.begin("cert-storage", false);
        String stored_cert_hash = preferences.getString("ca-cert-hash", "");

        if (stored_cert_hash == new_cert_hash) {
            LOGF("Certificate hash matches the stored version.");
        } else {
            LOGF("Certificate hash has changed!");
            // Step 2: Retrieve the full certificate since the hash is different
            retrieve_and_store_certificate(new_cert_hash);
        }

        preferences.end();
    } 
    else 
    {
        LOGF("Failed to retrieve certificate hash, HTTP code: %d", http_code);
    }
    http.end();
}



String load_certificate() {
    preferences.begin("cert-storage", true);  // Open storage in read-only mode
    String stored_cert = preferences.getString("ca-cert", "");  // Retrieve the certificate or default to an empty string if not found
    preferences.end();  // Close preferences to free memory

    if (stored_cert.length() > 0) {
        LOGF("Certificate loaded from preferences:");
        //LOGF(stored_cert);
    } else {
        LOGF("No certificate found in preferences.");
    }

    return stored_cert;
}


/* ---- AUTO CONFIG for ssid and psswd ---- */


String ssid, password, station_id;

void handle_save_config()
{
    ssid       = server.arg("ssid");
    password   = server.arg("password");
    station_id = server.arg("station_id");

    if( ssid.length() <= MAX_SSID_LEN && 
        password.length() <= MAX_STR_LEN &&
        station_id.length() <= MAX_STR_LEN && station_id.length() >= 4)
    {
      preferences.begin("wifi-config", false);  // Open preferences in read-write mode
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.putString("station_id", station_id);
      preferences.end();
      server.send(200, "text/html", 
              R"rawliteral(
      <html>
          <head>
            <style>
              body { font-family: Arial, sans-serif; background: #f4f4f4; text-align: center; margin: 50px; }
              .container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); display: inline-block; }
              h1 { color: #333; }
              h2 { color: #007bff; font-size: 24px; }
              button { background: #007bff; color: white; border: none; padding: 12px 20px; font-size: 16px; cursor: pointer; border-radius: 5px; transition: 0.3s; }
              button:hover { background: #0056b3; }
              input { width: 80%; padding: 10px; margin: 10px; border: 1px solid #ccc; border-radius: 5px; }
            </style>
          </head>
          <body style="width:100vw; height:100vh; padding:auto">
              <div style="margin:auto; width:fit-content">
                  <h1> Config saved, reboot the board using the reset button </h1>
          </body>
      </html>)rawliteral");
    }
    else {
      server.send(200, "text/html", "<h1>Cannot save preferences, malformed strings</h1>");
    }
}

void handle_get_calibration()
{
  int angle = read_angle_as5600_raw();
  float angleDegrees = (float)angle / 4096.0 * 360.0;
  server.send(200, "text/plain", String(angleDegrees));
}

void handle_save_calibration()
{
    int angle = read_angle_as5600_raw();
    preferences.begin("wifi-config", false);  // Open preferences in read-write mode
    auto size = preferences.putInt("vane_cal", angle);
    preferences.end();

    LOGF("Writing vane calibration %d", angle);
    server.send(200, "text/plain", "nope");
}



void start_config_portal() {
  LOGF("Starting configuration portal...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("nimbus_config");

  IPAddress IP = WiFi.softAPIP();
  LOGF("AP IP address: %s",String(IP).c_str());
  
  dns_server.start(DNS_PORT, "*", IP);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", 
      R"rawliteral(<html>
        <head>
          <style>
            body { font-family: Arial, sans-serif; background: #f4f4f4; text-align: center; margin: 50px; }
            .container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); display: inline-block; }
            h1 { color: #333; }
            h2 { color: #007bff; font-size: 24px; }
            button { background: #007bff; color: white; border: none; padding: 12px 20px; font-size: 16px; cursor: pointer; border-radius: 5px; transition: 0.3s; }
            button:hover { background: #0056b3; }
            input { width: 80%; padding: 10px; margin: 10px; border: 1px solid #ccc; border-radius: 5px; }
          </style>
        </head>
        <script>
            function updateSensor() {
                fetch('/vane')
                .then(response => response.text())
                .then(data => { document.getElementById('sensorValue').innerText = data; });
                setTimeout(updateSensor, 500); // Fetch every 500ms
            }
            function saveCalibration() {
                let value = document.getElementById('sensorValue').innerText;
                fetch('/save_calibration')
                .then(response => response.text())
                .then(data => alert('Calibration Saved: ' + data));
            }
            window.onload = updateSensor;
        </script>
        <body style="width:100vw; height:100vh; padding:auto">
            <div style="margin:auto; width:fit-content">
                <h1> Nimbus configurator </h1>
                <h1>Current Sensor Value: <span id="sensorValue">0</span></h1>
                <button onclick="saveCalibration()">Save Calibration</button>
                <h1> Insert ssid and password </h1>
                <form action="/save_config" method="post">
                    <div>
                        Station ID:<br> <input type="text" name="station_id" maxlength=40><br>
                    </div>
                    <div>
                        SSID:<br> <input type="text" name="ssid" maxlength=40><br>
                    </div>
                    <div>
                        Password:<br> <input type="password" name="password" maxlength=40><br>
                    </div>
                    <button type="submit"> Save </button>
                </form>
            </div>
        </body>
    </html>)rawliteral"
  );
    //server.send(200, "text/html", "ciao");
  });

  server.on("/save_calibration", HTTP_GET, handle_save_calibration);
  server.on("/save_config", HTTP_POST, handle_save_config);
  server.on("/vane", HTTP_GET, handle_get_calibration);

  server.begin();
  while(1)
  { 
    dns_server.processNextRequest();
    server.handleClient();
    delay(2);
  }
  
}


void check_reset_condition() {
  pinMode(RESET_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_PIN) == LOW) {
    LOGF("Resetting WiFi credentials...");
    preferences.begin("wifi-config", false);  // Open preferences in read-write mode
    preferences.clear();  // Clear all keys in "wifi-config" namespace
    preferences.end();
    delay(10000);
    esp_restart();
  }
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
  preferences.begin("wifi-config", false);
  ssid       = preferences.getString("ssid", "");
  password   = preferences.getString("password", "");
  station_id = preferences.getString("station_id","");
  preferences.end();

  int try_count = 0;
  
  delay(100);
  if(ssid.length() == 0 || password.length() == 0) start_config_portal();
  WiFi.begin(ssid.c_str(), password.c_str());

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

void connect_to_broker(MqttClient& mqttClient, const char* broker, const int port) {

  #define MAX_N_TRIES 10
  
  LOGF("Attempting to connect to the MQTT broker: %s", broker);
  unsigned long start_attempt_time = millis();

  int try_count = 0;
  while (!mqttClient.connect(broker, port) && try_count < MAX_N_TRIES) {
    delay(500);
    #ifdef DEBUG
    #endif
    delay(500);
    try_count++;
  }

  if (try_count == MAX_N_TRIES) {
    LOGF("MQTT connection failed! Error code = %d", mqttClient.connectError());
    esp_sleep_enable_timer_wakeup(5 * 1000000);
    esp_deep_sleep_start();
  } else {
    LOGF("Broker Connected!");
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
