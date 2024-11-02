#include "pins_arduino.h"
#include "esp32-hal-gpio.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>

const char* hash_url = "https://cert.lykos.cc/ca-cert-hash"; // URL for hash endpoint
const char* cert_url = "https://cert.lykos.cc/ca-cert";      // URL for full certificate

Preferences preferences;
WebServer server(80);  // Create a web server on port 80

void print_status()
{
  switch (WiFi.status()) 
    {
      case WL_CONNECTED:
        Serial.println("WL_CONNECTED");
        break;
      case WL_NO_SHIELD:
        Serial.println("WL_NO_SHIELD");
        break;
      case WL_IDLE_STATUS:
        Serial.println("WL_IDLE_STATUS");
        break;
      case WL_NO_SSID_AVAIL:
        Serial.println("WL_NO_SSID_AVAIL");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("WL_CONNECT_FAILED");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("WL_SCAN_COMPLETED");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("WL_CONNECTION_LOST");
        break;
      case WL_DISCONNECTED:
        Serial.println("WL_DISCONNECTED");
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
        Serial.println("Certificate retrieved from server:");
        Serial.println(certData);

        // Store the new certificate and its hash in flash memory
        preferences.begin("cert-storage", false);
        preferences.putString("ca-cert", certData);      // Store the certificate data
        preferences.putString("ca-cert-hash", new_cert_hash);  // Update the stored hash
        preferences.end();

        Serial.println("Stored certificate and hash updated.");
    } 
    else 
    {
        Serial.printf("Failed to retrieve certificate, HTTP code: %d\n", http_code);
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
        Serial.println(new_cert_hash);
        Serial.println("Hash retrieved from server:");
        Serial.println(new_cert_hash);

        // Retrieve the stored hash from flash memory
        preferences.begin("cert-storage", false);
        String stored_cert_hash = preferences.getString("ca-cert-hash", "");

        if (stored_cert_hash == new_cert_hash) {
            Serial.println("Certificate hash matches the stored version.");
        } else {
            Serial.println("Certificate hash has changed!");
            // Step 2: Retrieve the full certificate since the hash is different
            retrieve_and_store_certificate(new_cert_hash);
        }

        preferences.end();
    } 
    else 
    {
        Serial.printf("Failed to retrieve certificate hash, HTTP code: %d\n", http_code);
    }
    http.end();
}



String load_certificate() {
    preferences.begin("cert-storage", true);  // Open storage in read-only mode
    String stored_cert = preferences.getString("ca-cert", "");  // Retrieve the certificate or default to an empty string if not found
    preferences.end();  // Close preferences to free memory

    if (stored_cert.length() > 0) {
        Serial.println("Certificate loaded from preferences:");
        //Serial.println(stored_cert);
    } else {
        Serial.println("No certificate found in preferences.");
    }

    return stored_cert;
}


/* -------------------------------* 
| AUTO CONFIG for ssid and psswd  |
* --------------------------------*/


String ssid, password;
#define RESET_PIN 4

void start_config_portal() {
  Serial.println("Starting configuration portal...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("nimbus_config");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", 
      "<html>\n"
        "<body style=\"width:100vw; height:100vh; padding:auto\">\n"
            "<div style=\"margin:auto; width:fit-content\">\n"
                "<h1> Nimbus configurator </h1>\n"
                "<h3> Insert ssid and password </h3>\n"
                "<form action=\"/save\" method=\"post\">\n"
                    "<div style=\"padding:1em\">\n"
                        "SSID:<br> <input type=\"text\" name=\"ssid\"><br>\n"
                    "</div>\n"
                    "<div style=\"padding:1em\">\n"
                        "Password:<br> <input type=\"password\" name=\"password\"><br>\n"
                    "</div>\n"
                    "<input type=\"submit\" value=\"Save\">\n"
                "</form>\n"
            "</div>\n"
        "</body>\n"
    "</html>\n"
  );
    //server.send(200, "text/html", "ciao");
  });

  server.on("/save", HTTP_POST, []() {
    ssid = server.arg("ssid");
    password = server.arg("password");

    preferences.begin("wifi-config", false);  // Open preferences in read-write mode
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();

    server.send(200, "text/html", "<h1>Configuration Saved! Rebooting...</h1>");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  while(1)
  { 
    server.handleClient();
    delay(2);
  }
  
}


void check_reset_condition() {
  pinMode(RESET_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("Resetting WiFi credentials...");
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
  preferences.begin("wifi-config", false);
  ssid     = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();

  int try_count = 0;
  WiFi.begin(ssid.c_str(), password.c_str());

  if(ssid.length() == 0 || password.length() == 0) start_config_portal();

  Serial.print("Connecting to WiFi...");
  unsigned long start_attempt_time = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start_attempt_time < 60000) {
    delay(500);
    #ifdef DEBUG
      Serial.print(WiFi.status());
      print_status();
    #endif
    delay(500);
    blink_n_times();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" Failed!");
    esp_sleep_enable_timer_wakeup(5 * 1000000);
    esp_deep_sleep_start();
  } else {
    Serial.println(" Connected!");
  }
}

void connect_to_broker(MqttClient& mqttClient, const char* broker, const int port) {

  #define MAX_N_TRIES 50
  
  #ifdef DEBUG
    Serial.print("Attempting to connect to the MQTT broker: ");
    Serial.println(broker);
  #endif

  int try_count = 0;
  Serial.print("Connecting to broker...");
  unsigned long start_attempt_time = millis();

  while (!mqttClient.connect(broker, port) && try_count < MAX_N_TRIES) {
    delay(500);
    #ifdef DEBUG
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
    #endif
    delay(500);
    blink_n_times(3);
    try_count++;
  }

  if (try_count == MAX_N_TRIES) {
    Serial.println(" Failed!");
    esp_sleep_enable_timer_wakeup(5 * 1000000);
    esp_deep_sleep_start();
  } else {
    Serial.println(" Connected!");
  }
}

float read_avg_v(int pin)
{
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
