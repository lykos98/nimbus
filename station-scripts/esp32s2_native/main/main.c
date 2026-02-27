#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "hal/brownout_hal.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_periph.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"


#include "https_client.h"
#include "ulp_riscv.h"
#include "wifi.h"
#include "sensors.h"

#include "ulp_main.h"

#define ULP_TAG "ulp"
#define API_URL "https://hehe.lykos.cc/api/stations"

#if !defined(STATION_NAME)
    #error "Please define STATION_NAME in config.h"
#endif

#define CYCLE_SLEEP_TIME (10 * 60 * 1000 * 1000)
// #define CYCLE_SLEEP_TIME (15 * 1000 * 1000)
#define RESET_SLEEP_TIME (2 * 60 * 1000 * 1000)
#define PI 3.14


#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

#define MSG_SIZE 300

char msg[MSG_SIZE];

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

// #include "driver/uart.h"
//
// #define UART_TX_PIN         (GPIO_NUM_11) // S2 TX pin connected to C3 RX pin
// #define UART_RX_PIN         (UART_PIN_NO_CHANGE) // Not needed for sending
// #define UART_PORT_NUM       (UART_NUM_1) 
// #define UART_BAUD_RATE      (115200) // MUST MATCH C3's BAUD RATE
// #define BUF_SIZE            (1024)
//
// static void uart_init() {
//     // 1. Configure the UART parameters
//     uart_config_t uart_config = {
//         .baud_rate = UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity    = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
//
//     // 2. Install the UART driver (no event queue needed for simple transmission)
//     ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
//
//     // 3. Set the UART pins using the GPIO Matrix
//     // This is where GPIO 10 is mapped to the UART1 TX signal.
//     ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
//
//     // 4. Set the configuration parameters
//     ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
//
//     ESP_LOGI("uart_tag", "UART1 initialized on TX: GPIO%d at %d Baud.", UART_TX_PIN, UART_BAUD_RATE);
// }

char* reset_reason_to_string(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:     return "UNKNOWN";           // 0
        case ESP_RST_POWERON:     return "POWERON";           // 1
        case ESP_RST_EXT:         return "EXTERNAL_PIN";      // 2
        case ESP_RST_SW:          return "SOFTWARE_RESTART";  // 3 (esp_restart)
        case ESP_RST_PANIC:       return "PANIC_EXCEPTION";   // 4 (Software reset due to exception)
        case ESP_RST_INT_WDT:     return "INT_WATCHDOG";      // 5 (Interrupt Watchdog Reset)
        case ESP_RST_TASK_WDT:    return "TASK_WATCHDOG";     // 6 (Task Watchdog Reset)
        case ESP_RST_WDT:         return "OTHER_WATCHDOG";    // 7
        case ESP_RST_DEEPSLEEP:   return "DEEP_SLEEP_WAKEUP"; // 8
        case ESP_RST_BROWNOUT:    return "BROWNOUT";          // 9 (Voltage low)
        case ESP_RST_SDIO:        return "SDIO";              // 10
        // NOTE: Additional reasons (e.g., USB, JTAG) may exist depending on the ESP32 variant (S2, C3, S3, C6, etc.)
        default:                  return "UNRECOGNIZED";
    }
}

void decouple_adc_pins() {
    // Disable the ADC GPIO modes to prevent internal leakage
    gpio_set_direction(SOLAR_PIN, GPIO_MODE_DISABLE);
    gpio_set_direction(BATTERY_PIN, GPIO_MODE_DISABLE);
}

void disable_i2c_gpios() {
    gpio_reset_pin(BMP280_SDA); // BMP280_SDA is GPIO_NUM_9
    gpio_reset_pin(BMP280_SCL); // BMP280_SCL is GPIO_NUM_8
    // Set them to input and disable internal pull-ups/downs
    gpio_set_direction(BMP280_SDA, GPIO_MODE_DISABLE);
    gpio_set_direction(BMP280_SCL, GPIO_MODE_DISABLE);
    // If possible, power down the I2C devices or remove VCC to the bus.
}

void app_task(void *pvParameters) {

    char api_endpoint[100];
    snprintf(api_endpoint, 100, "%s/%s/data", API_URL, STATION_NAME);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(i2cdev_init());

    esp_reset_reason_t reason = esp_reset_reason();
    char* reason_str = reset_reason_to_string(reason);
    int anemometer_rotations = 0;
    int ulp_is_active_copy = 0;

    /**
     * Connectivity start wifi and mqtt
     */

    esp_err_t err = ESP_OK;
    err = wifi_connect();

    if(err !=  ESP_OK)
    {
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }


    /*
     * Initialize sensors
     */

    err = bmp_init_dev();
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \" Cannot initialize bmp\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }

    err = aht_init_dev();
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \" Cannot initialize aht\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }

    err = ams_init_dev();
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \" Cannot initialize ams\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }

    err = adc_init_units();
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize adc units\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }

    adc_pin_config_t solar_pin_config = {0};
    err = adc_init_pins(&solar_pin_config, SOLAR_PIN);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize SOLAR PIN reading\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }

    adc_pin_config_t battery_pin_config = {0};
    err = adc_init_pins(&battery_pin_config, BATTERY_PIN);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize BATTERY PIN reading\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }


    rtc_gpio_init(ANEMOMETER_PIN);
    rtc_gpio_set_direction(ANEMOMETER_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(ANEMOMETER_PIN);
    rtc_gpio_pullup_en(ANEMOMETER_PIN);
    rtc_gpio_hold_en(ANEMOMETER_PIN);

    /* start the ulp & or check the things */
    if(reason == ESP_RST_DEEPSLEEP)
    {

        if(ulp_is_active != 117)
        {
            anemometer_rotations = 0;
            ulp_is_active_copy   = 0;
            ESP_LOGI(ULP_TAG, "Resetting ulp");
        }
        else 
        {
            anemometer_rotations = ulp_rotations;
            ulp_is_active_copy   = ulp_is_active;
        }

        /* reset after deep sleep */
        //ulp_riscv_reset();
        ulp_is_active = 0;
        ulp_rotations = 0;
        
        /* wait for the reset */
        vTaskDelay(pdMS_TO_TICKS(50));

        /* check if the upl is running */
        if(ulp_is_active_copy != 117)
        {
            err = ulp_riscv_load_binary(ulp_main_bin_start, ulp_main_bin_end - ulp_main_bin_start);
            if(err !=  ESP_OK)
            {
                char tmp_msg[256];
                int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize ulp: load failed \", \"secret\": \"%s\"}", STATION_NAME, SECRET);
                https_post_json(api_endpoint, tmp_msg, len);
                esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
                esp_deep_sleep_start();
            }
            ulp_set_wakeup_period(0, 40000);
            err = ulp_riscv_run();
            if(err !=  ESP_OK)
            {
                char tmp_msg[256];
                int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize ulp: run \", \"secret\": \"%s\"}", STATION_NAME, SECRET);
                https_post_json(api_endpoint, tmp_msg, len);
                esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
                esp_deep_sleep_start();
            }
        }


        ESP_LOGI(ULP_TAG, "Found %u rotations", anemometer_rotations);
    }
    else
    {
        ulp_is_active = 0;
        ulp_rotations = 0;
        ulp_gpio_level_previous = 0;
        err = ulp_riscv_load_binary(ulp_main_bin_start, ulp_main_bin_end - ulp_main_bin_start);
        if(err !=  ESP_OK)
        {
            char tmp_msg[256];
            int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize ulp: load failed \", \"secret\": \"%s\"}", STATION_NAME, SECRET);
            https_post_json(api_endpoint, tmp_msg, len);
            esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
            esp_deep_sleep_start();
        }
        ulp_set_wakeup_period(0, 40000);
        err = ulp_riscv_run();
        if(err !=  ESP_OK)
        {
            char tmp_msg[256];
            int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Cannot initialize ulp: run \", \"secret\": \"%s\"}", STATION_NAME, SECRET);
            https_post_json(api_endpoint, tmp_msg, len);
            esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
            esp_deep_sleep_start();
        }

        ESP_LOGI(ULP_TAG, "First boot starting ulp");

    }
    


    

    /*
     * Read from sensors
     */

    // bmp hum is only a dummy
    float bmp_press, bmp_hum, bmp_temp;
    float aht_hum, aht_temp;
    uint16_t angle;
    int solar_v, solar_raw, battery_v, battery_raw;

    err = bmp_read(&bmp_temp, &bmp_press, &bmp_hum);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Error while reading from bmp\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }
    else 
    {
        ESP_LOGI(BMP_TAG, "Read %.2f C %.2f Pa from bmp", bmp_temp, bmp_press);    
    }

    err = aht_read(&aht_temp, &aht_hum);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Error while reading from aht\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }
    else 
    {
        ESP_LOGI(AHT_TAG, "Read %.2f C %.2f from aht", aht_temp, aht_hum);    
    }


    err = ams_read(&angle);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Error while reading from ams\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }
    else 
    {
        ESP_LOGI(AMS_TAG, "Read %.u from ams", angle);    
    }

    err = adc_read_pin(solar_pin_config, &solar_raw, &solar_v);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Error while reading from solar adc val\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }
    else 
    {
        ESP_LOGI(AMS_TAG, "Read %d (%d mV) from solar", solar_raw, solar_v);    
    }

    err = adc_read_pin(battery_pin_config, &battery_raw, &battery_v);
    if(err !=  ESP_OK)
    {
        char tmp_msg[256];
        int len = snprintf(tmp_msg, sizeof(tmp_msg), "{\"stationId\":\"%s\", \"message\" : \"Error while reading from battery adc val\", \"secret\": \"%s\"}", STATION_NAME, SECRET);
        https_post_json(api_endpoint, tmp_msg, len);
        esp_sleep_enable_timer_wakeup(RESET_SLEEP_TIME);
        esp_deep_sleep_start();
    }
    else 
    {
        ESP_LOGI(AMS_TAG, "Read %d (%d mV) from battery", battery_raw, battery_v);    
    }

    int angle_int = (360 * angle/4096 + ANGLE_OFFSET) % 360;


    float wind_speed = (float)anemometer_rotations * 2 * ANEMOMETER_LEN_M / (CYCLE_SLEEP_TIME / 1e6);
    int size = snprintf(msg, MSG_SIZE, 
        "{\"stationId\":\"%s\","
         "\"airHumidity\":%.2f," 
         "\"airTemperature\":%.2f," 
         "\"airPressure\":%.2f," 
         " \"solarV\":%.2f," 
         " \"batteryV\":%.2f," 
         " \"windSpeed\":%.2f,"
         " \"windDirection\":%d,"
         " \"ulpActive\":%d,"
         " \"resetReason\":\"%s\","
         " \"anemometerRotations\":%d,"
         " \"secret\":\"%s\"}",
         STATION_NAME, aht_hum, aht_temp, bmp_press, 
         2.*(float)solar_v/1000. + 0.2, 2.*(float)battery_v/1000. + 0.2,
         wind_speed, angle_int, ulp_is_active_copy, 
         reason_str, anemometer_rotations, SECRET);

    // char cc[] = "ciao";
    // mqtt_send(cc, sizeof(cc));
    https_post_json(api_endpoint, msg, size);
    ESP_LOGI("MSG", "(%u len) %s", size, msg);
    ESP_LOGI("HEAP", "Free heap: %d, Min: %d", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    gpio_set_level(GPIO_NUM_3,0);
    gpio_set_level(GPIO_NUM_5,0);

    disable_i2c_gpios();
    esp_wifi_stop();

    esp_sleep_enable_timer_wakeup(CYCLE_SLEEP_TIME);
    esp_deep_sleep_start();

    vTaskDelete(NULL);
}

void app_main()
{
   // uart_init();

    /* disable brownout sensor */    
    brownout_hal_config_t cfg = {
        .threshold = 7,			// 3.00v
        .enabled = false,
        .reset_enabled = false,		// if you want to use ".det" as a software signal, you must disable all brown-out shutdowns
        .flash_power_down = false,
        .rf_power_down = false,
    };
    brownout_hal_config(&cfg);
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    
    gpio_set_level(GPIO_NUM_3,1);
    gpio_set_level(GPIO_NUM_5,1);


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    xTaskCreate(app_task, "app_task", 4096, NULL, 5, NULL);
}
