#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s2/ulp.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "soc/rtc_io_reg.h"
#include <rom/rtc.h>
#include "hal/brownout_hal.h"		// for adjusting brownout level

#define SOLAR_PIN GPIO_NUM_17
#define BATTERY_PIN GPIO_NUM_16
#define AS5600_POWER_PIN GPIO_NUM_3
#define ANEMOMETER_PIN   GPIO_NUM_7

#define RESET_PIN GPIO_NUM_4

#define SCL_PIN GPIO_NUM_8
#define SDA_PIN GPIO_NUM_9

#define ANEMOMETER_LEN 0.13
#define SLEEP_TIME (60 * 10)
//#define SLEEP_TIME (30)
#define M_PI 3.14159265358979323846


#include "esp32_utilities.h"
#include "ulp_program.h"


#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

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

  LOGF("Calibration %d", calibration);
 

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
    int angle = (highByte << 8) | lowByte;
    //+ 4096 + 1024 to shift to the correct value nswe
    angle = (angle - calibration + 4096 + 1024) % 4096;
    angleDegrees = (float)angle / 4096.0 * 360.0;

  }
  return angleDegrees;
}

float angle_to_direction(float angle, char* direction)
{
  angle = angle + 12.25;
  int dir_code = (int)(angle/22.5);
  return angle;
}

void get_reset_reason(esp_reset_reason_t reason, char* str_reason)
{
  switch (reason)
  {
    case ESP_RST_UNKNOWN:
      strcpy(str_reason,"ESP_RST_UNKNOWN");
      break;
    case ESP_RST_POWERON:
      strcpy(str_reason,"ESP_RST_POWERON");
      break;
    case ESP_RST_EXT:
      strcpy(str_reason,"ESP_RST_EXT");
      break;
    case ESP_RST_SW:
      strcpy(str_reason,"ESP_RST_SW");
      break;
    case ESP_RST_PANIC:
      strcpy(str_reason,"ESP_RST_PANIC");
      break;
    case ESP_RST_INT_WDT:
      strcpy(str_reason,"ESP_RST_INT_WDT");
      break;
    case ESP_RST_TASK_WDT:
      strcpy(str_reason,"ESP_RST_TASK_WDT");
      break;
    case ESP_RST_WDT:
      strcpy(str_reason,"ESP_RST_WDT");
      break;
    case ESP_RST_DEEPSLEEP:
      strcpy(str_reason,"ESP_RST_DEEPSLEEP");
      break;
    case ESP_RST_BROWNOUT:
      strcpy(str_reason,"ESP_RST_BROWNOUT");
      break;
    case ESP_RST_SDIO:
      strcpy(str_reason,"ESP_RST_SDIO");
      break;
    case ESP_RST_USB:
      strcpy(str_reason,"ESP_RST_USB");
      break;
    case ESP_RST_JTAG:
      LOGF(str_reason,"ESP_RST_JTAG");
      break;
    case ESP_RST_EFUSE:
      LOGF(str_reason,"ESP_RST_EFUSE");
      break;
    case ESP_RST_PWR_GLITCH:
      LOGF(str_reason,"ESP_RST_PWR_GLITCH");
      break;
    case ESP_RST_CPU_LOCKUP:
      LOGF(str_reason,"ESP_RST_CPU_LOCKUP");
      break;
    default:
      break;    
  }
}

void bmp_sleep() {
  bmp.setSampling(Adafruit_BMP280::MODE_SLEEP);
}

void setup() {
  char message[LOG_BUFFER_SIZE];

  /* disable brownout sensor */    
  brownout_hal_config_t cfg = {
    .threshold = 7,			// 3.00v
    .enabled = false,
    .reset_enabled = false,		// if you want to use ".det" as a software signal, you must disable all brown-out shutdowns
    .flash_power_down = false,
    .rf_power_down = false,
  };
  brownout_hal_config(&cfg);

  /* 
   * start up as5600 sensor
   * powered down for energy consumption 
   */

  pinMode(GPIO_NUM_3, OUTPUT);
  digitalWrite(GPIO_NUM_3, HIGH);


  /* 
   * Init serial and wait for initialization 
   * TODO: fine tune it to better have the
   * minimal number of seconds to sleep
   */
  Serial.begin(115200);
  delay(1000);
  

  LOGF("\n\n---------------------------------------------------");
  LOGF("RESET REASON");
  esp_reset_reason_t  reason = esp_reset_reason();
  char str_reason[20];
  get_reset_reason(reason, str_reason);
  LOGF("%s", str_reason);
  
  Wire.begin(SDA_PIN, SCL_PIN);

  /* 
   * check if initial config
   * SSID, password and possible configs need to be reset
   */
  check_reset_condition();
  connect_to_wifi();

  /* retrieve from memory the number of anemometer rotations */  
  if (reason == ESP_RST_DEEPSLEEP) {
    // If so, print the counter
    LOGF("Number of turns %u", (uint32_t)(RTC_SLOW_MEM[COUNTER] & 0xFFFF) );
    //Serial.print("Old state ");
    //LOGF((uint32_t)(RTC_SLOW_MEM[OLD_STATE] & 0xFFFF) );
    anemometer_rotations = RTC_SLOW_MEM[COUNTER] & 0xFFFF;
    RTC_SLOW_MEM[COUNTER] = 0;    
  } else {
    // First-time setup
    RTC_SLOW_MEM[COUNTER] = 0;
    RTC_SLOW_MEM[OLD_STATE] = 0;
    LOGF("First boot, setting up ULP wakeup counter.");
  }
  
  /* set the direction of the pin of the anemometer */

  rtc_gpio_init(ANEMOMETER_PIN);
  rtc_gpio_set_direction(ANEMOMETER_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis(ANEMOMETER_PIN);     // enable the pull-up resistor
  rtc_gpio_pullup_en(ANEMOMETER_PIN);  // disable the pull-down resistor
  rtc_gpio_hold_en(ANEMOMETER_PIN);       // required to maintain pull-up
  
  /* retrieve certificate and connect to the mqtt broker */
  check_certificate();
  String ca_cert = load_certificate();
  wifiClient.setCACert(ca_cert.c_str());
 
  connect_to_broker(mqttClient, broker, port);

  /*
   * Initialize aht 
   */
  int tries = 0;
  while (aht.begin(&Wire) != true && tries < MAX_N_TRIES)
  {
    LOGF("AHT20 not connected or fail to load calibration coefficient"); 
    delay(5000);
    ++tries;
  }
 
  if(tries == MAX_N_TRIES)
  {
    snprintf( message, LOG_BUFFER_SIZE,
              "{\"stationId\":\"%s\",\"error\":\"Cannot initialize AHT\"}", station_id.c_str());
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();
    delay(500);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000 * 1000);
    esp_deep_sleep_start();
  }
  
  /* 
   * initialize BMP
   */

  tries = 0;
  //if(!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)){
  while(!bmp.begin() && tries < MAX_N_TRIES)
  {
      delay(500);
      LOGF("Cannot initialize bmp");
      ++tries;
  }

  if(tries == MAX_N_TRIES)
  {
    snprintf( message, LOG_BUFFER_SIZE, 
              "{\"stationId\":\"%s\",\"error\":\"Cannot initialize BMP\"}", station_id.c_str());
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();
    delay(500);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000 * 1000);
    esp_deep_sleep_start();
  }

  LOGF("Sensors initialized");

  /* reading sensor values */

  sensors_event_t aht_temp, aht_hum;
  aht.getEvent(&aht_hum, &aht_temp);
  float t = (bmp.readTemperature() + aht_temp.temperature ) /2.;
  float p = bmp.readPressure();
  float h = aht_hum.relative_humidity;

  LOGF("Read from bmp & aht");

  /* reading battery and solar voltages */

  float battery_voltage = read_avg_v(BATTERY_PIN);
  float solar_voltage = read_avg_v(SOLAR_PIN);
  LOGF("Read battery V and solar V");

  /* reading wind speed and direction */
  float wind_speed = 3 * (float)anemometer_rotations * M_PI * (float)ANEMOMETER_LEN / (float)(SLEEP_TIME);
  float wind_angle = read_angle_as5600(Wire);
  LOGF("Read from as5600");

  LOGF("Sensors value read");

  /* composing mesage and sending */
  esp_err_t err = startULP();

  char ulp_err[10];
  if(err == ESP_OK){
    strcpy(ulp_err,"OK");
  }
  else {
    strcpy(ulp_err,"FAIL");
  }

  snprintf(message, LOG_BUFFER_SIZE,
            "{\"stationId\":\"%s\","
            "\"airHumidity\":%.2f,"
            "\"airTemperature\":%.2f,"
            "\"airPressure\":%.2f,"
            "\"solarV\":%.2f,"
            "\"batteryV\":%.2f,"
            "\"RSSI\":%ld,"
            "\"windSpeed\":%.2f,"
            "\"windDirection\":%.2f,"
            "\"ulpState\":\"%s\","
            "\"resetReason\":\"%s\"}",
          station_id.c_str(),h,t,p,solar_voltage, battery_voltage, WiFi.RSSI(), 
          wind_speed, wind_angle, ulp_err, str_reason); 
  LOGF("Message built");
  mqttClient.beginMessage(topic);
  mqttClient.print(message);
  mqttClient.endMessage();

  delay(500);
  
  LOGF(message);
  LOGF("Sending via mqtt");   

  
  /* disconnect from wifi and start the ULP */
  WiFi.disconnect();
  

  /* Go to deep sleep */
  esp_sleep_enable_ulp_wakeup();
  esp_sleep_enable_timer_wakeup(SLEEP_TIME *1000 * 1000);

  /* turn off a5600 and bmp */
  digitalWrite(GPIO_NUM_3, LOW);
  bmp.setSampling(Adafruit_BMP280::MODE_SLEEP);
  esp_deep_sleep_start();
 
}

void loop() {}
