#pragma once

#include "config.h"
#include <bmp280.h>
#include <aht.h>
#include <esp_check.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include "i2cdev.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"


#if !defined(AHT20_SDA) || \
    !defined(AHT20_SCL) || \
    !defined(BMP280_SDA) || \
    !defined(BMP280_SCL) || \
    !defined(AMS5600_SDA) || \
    !defined(AMS5600_SCL)  
    #error "Please define AHT20_SDA, AHT20_SCL, BMP280_SDA) \
            BMP280_SCL, AMS5600_SDA, AMS5600_SCL in config.h"
#endif


#define AMS5600_REG_RAW_ANGLE_H 0x0C
#define AMS5600_REG_RAW_ANGLE_L 0x0D

#define AMS5600_I2C_PORT I2C_NUM_0 
#define AMS5600_ADDR 0x36 

#define ADC_ATTEN       ADC_ATTEN_DB_12 // Full 3.3V range
#define ADC_WIDTH       ADC_BITWIDTH_12 // 12-bit (0-4095)
#define DEFAULT_VREF    3300.0f         // Approx max voltage in mV for fallback


#define AMS_TAG  "ams"
#define AHT_TAG  "aht"
#define BMP_TAG  "bmp"
#define ADC_TAG  "adc"

typedef struct {
    gpio_num_t gpio_num;
    adc_unit_t unit_id;
    adc_channel_t channel_id;
    adc_cali_handle_t cali_handle;
    bool is_calibrated;
} adc_pin_config_t;

esp_err_t bmp_init_dev();
esp_err_t bmp_read(float* temp, float* press, float* hum);
esp_err_t aht_read(float* temp, float* hum);
esp_err_t aht_init_dev();
esp_err_t ams_read(uint16_t *angle);
esp_err_t ams_init_dev();
esp_err_t adc_pin_init(gpio_num_t gpio_pin, adc_pin_config_t *out_config);
esp_err_t adc_init_units();
esp_err_t adc_init_pins(adc_pin_config_t* pin_config, gpio_num_t num);
esp_err_t adc_read_pin(adc_pin_config_t pin_config, int* raw, int* mV);
