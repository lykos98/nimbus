#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s2/ulp.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "soc/rtc_io_reg.h"
#include <rom/rtc.h>
#include "esp32_utilities.h"
#include "ulp_program.h"
#include "hal/brownout_hal.h"		// for adjusting brownout level



#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

#define ANEMOMETER_LEN 8e-2
#define SLEEP_TIME 60 * 10
#define M_PI 3.14159265358979323846

Adafruit_AHTX0  aht;
Adafruit_BMP280 bmp(&Wire); // I2C

int count = 0;
uint32_t anemometer_rotations = 0;

float read_angle_as5600(TwoWire& w)
{
  int calibration;
  preferences.begin("wifi-config", true);
  calibration = preferences.getInt("vane_cal");
  preferences.end();

  Serial.print("Calibration ");
  Serial.println(calibration);

  w.beginTransmission(AS5600_ADDRESS);
  w.write(0x0C); // Angle register address
  w.endTransmission();

  w.requestFrom(AS5600_ADDRESS, 2); // Request 2 bytes of data
  delay(100);
  // Read angle data
  float angleDegrees = 0.;
  if (w.available() >= 2) {
    byte highByte = w.read();
    byte lowByte = w.read();

    // Combine high and low bytes into a 12-bit angle value
    int angle = (highByte << 8) | lowByte;
    angle = (angle - calibration + 4096) % 4096;

    // Convert angle to degrees (0-360)
    angleDegrees = (float)angle / 4096.0 * 360.0;

    //Serial.print("Angle: ");
    //Serial.print(angleDegrees);
    //Serial.println(" degrees");
  }
  return angleDegrees;
}

float angle_to_direction(float angle, char* direction)
{
  angle = angle + 12.25;
  int dir_code = (int)(angle/22.5);
  return angle;
}

void print_reset_reason(esp_reset_reason_t reason)
{
  switch (reason)
  {
    case ESP_RST_UNKNOWN:
      Serial.println("ESP_RST_UNKNOWN");
      break;
    case ESP_RST_POWERON:
      Serial.println("ESP_RST_POWERON");
      break;
    case ESP_RST_EXT:
      Serial.println("ESP_RST_EXT");
      break;
    case ESP_RST_SW:
      Serial.println("ESP_RST_SW");
      break;
    case ESP_RST_PANIC:
      Serial.println("ESP_RST_PANIC");
      break;
    case ESP_RST_INT_WDT:
      Serial.println("ESP_RST_INT_WDT");
      break;
    case ESP_RST_TASK_WDT:
      Serial.println("ESP_RST_TASK_WDT");
      break;
    case ESP_RST_WDT:
      Serial.println("ESP_RST_WDT");
      break;
    case ESP_RST_DEEPSLEEP:
      Serial.println("ESP_RST_DEEPSLEEP");
      break;
    case ESP_RST_BROWNOUT:
      Serial.println("ESP_RST_BROWNOUT");
      break;
    case ESP_RST_SDIO:
      Serial.println("ESP_RST_SDIO");
      break;
    case ESP_RST_USB:
      Serial.println("ESP_RST_USB");
      break;
    case ESP_RST_JTAG:
      Serial.println("ESP_RST_JTAG");
      break;
    case ESP_RST_EFUSE:
      Serial.println("ESP_RST_EFUSE");
      break;
    case ESP_RST_PWR_GLITCH:
      Serial.println("ESP_RST_PWR_GLITCH");
      break;
    case ESP_RST_CPU_LOCKUP:
      Serial.println("ESP_RST_CPU_LOCKUP");
      break;
    default:
      break;    
  }
}

void bmp_sleep() {
  bmp.setSampling(Adafruit_BMP280::MODE_SLEEP);
  //write8(BMP280_REGISTER_CONTROL, 0x3C);
}

void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  brownout_hal_config_t cfg = {
    .threshold = 7,			// 3.00v
    .enabled = false,
    .reset_enabled = false,		// if you want to use ".det" as a software signal, you must disable all brown-out shutdowns
    .flash_power_down = false,
    .rf_power_down = false,
  };
  brownout_hal_config(&cfg);
  pinMode(GPIO_NUM_3, OUTPUT);
  digitalWrite(GPIO_NUM_3, HIGH);

  Serial.begin(115200);
  delay(1000);
  

  Serial.println("\n\n---------------------------------------------------");
  Serial.println("RESET REASON");
  esp_reset_reason_t  reason = esp_reset_reason();
  print_reset_reason(reason);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  //Wire1.begin(AS5600_SDA_PIN, AS5600_SCL_PIN);

  check_reset_condition();
  connect_to_wifi();
  

 
  // Check if this is a wakeup from deep sleep
  if (reason == ESP_RST_DEEPSLEEP) {
    // If so, print the counter

    Serial.print("Number of turns ");
    Serial.println((uint32_t)(RTC_SLOW_MEM[COUNTER] & 0xFFFF) );
    Serial.print("Old state ");
    Serial.println((uint32_t)(RTC_SLOW_MEM[OLD_STATE] & 0xFFFF) );

    anemometer_rotations = RTC_SLOW_MEM[COUNTER] & 0xFFFF;

    RTC_SLOW_MEM[COUNTER] = 0;    
  } else {
    // First-time setup
    //ulp_wakeup_counter = 0;  // Reset the counter
    RTC_SLOW_MEM[COUNTER] = 0;
    RTC_SLOW_MEM[OLD_STATE] = 0;

    Serial.println("First boot, setting up ULP wakeup counter.");
  }
  // Initialize and start the ULP
  
  rtc_gpio_init(GPIO_SENSOR_PIN);
  rtc_gpio_set_direction(GPIO_SENSOR_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis(GPIO_SENSOR_PIN);     // enable the pull-up resistor
  rtc_gpio_pullup_en(GPIO_SENSOR_PIN);  // disable the pull-down resistor
  rtc_gpio_hold_en(GPIO_SENSOR_PIN);       // required to maintain pull-up
  


  check_certificate();
  String ca_cert = load_certificate();

  wifiClient.setCACert(ca_cert.c_str());
 
  
  connect_to_broker(mqttClient, broker, port);
  int tries = 0;
  while (aht.begin(&Wire) != true && tries < MAX_N_TRIES) {
    Serial.println(F("AHT20 not connected or fail to load calibration coefficient")); //(F()) save string to flash & keeps dynamic memory free
    delay(5000);
    ++tries;
  }
  char message[300];
 
  if(tries == MAX_N_TRIES)
  {
    esp_sleep_enable_timer_wakeup(5 * 1000 * 1000);
    esp_deep_sleep_start();
  }
  
  //if(!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)){
  if(!bmp.begin()){
      Serial.println("Cannot initialize bmp");
      esp_sleep_enable_timer_wakeup(5 * 1000 * 1000);
      esp_deep_sleep_start();
    }
  Serial.println("Sensors initialized");

  sensors_event_t aht_temp, aht_hum;

  aht.getEvent(&aht_hum, &aht_temp);
  float t = (bmp.readTemperature() + aht_temp.temperature ) /2.;
  //float t = aht_temp.temperature;

  float p = bmp.readPressure();
  float h = aht_hum.relative_humidity;
  Serial.println("Read from bmp & aht");

  float battery_voltage = 0.; //read_avg_v(0);
  Serial.println("Read battery V");

  float wind_speed = (float)anemometer_rotations * M_PI * (float)ANEMOMETER_LEN / (float)(SLEEP_TIME);

  float wind_angle = read_angle_as5600(Wire);
  Serial.println("Read from as5600");

  Serial.println("Sensors value read");

  sprintf(message, "{\"stationId\":\"%s\",\"airHumidity\":%.2f,\"airTemperature\":%.2f,\"airPressure\":%.2f,\"batteryV\":%.2f,\"RSSI\":%ld,\"windSpeed\":%.2f,\"windDirection\":%.2f}",
          station_id.c_str(),h,t,p, battery_voltage, WiFi.RSSI(), wind_speed, wind_angle); 
  Serial.println("Message built");
  mqttClient.beginMessage(topic);
  mqttClient.print(message);
  mqttClient.endMessage();

  delay(500);

  
  Serial.println(message);
  Serial.println("Sending via mqtt");   
  

  WiFi.disconnect();
  

  startULP();
  
  // Go to deep sleep
  esp_sleep_enable_ulp_wakeup();
  esp_sleep_enable_timer_wakeup(SLEEP_TIME *1000 * 1000);
  //esp_sleep_enable_timer_wakeup(30 *1000 * 1000);

  digitalWrite(GPIO_NUM_3, LOW);
  //aht.sleep();
  bmp.setSampling(Adafruit_BMP280::MODE_SLEEP);
  esp_deep_sleep_start();
  

 
}

void loop() {}