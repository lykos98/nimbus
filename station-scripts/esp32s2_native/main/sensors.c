#include "sensors.h"
#include "bmp280.h"
#include "common.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"

#define MAX_TRIES 10

i2c_dev_t ams5600_dev = {0};
aht_t aht_dev = {0};
bmp280_t bmp_dev = {0};

// ADC driver

// initialize adc_channels
adc_oneshot_unit_handle_t adc_unit_handles[2];



esp_err_t adc_init_units()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config1, &adc_unit_handles[0]),
                        ADC_TAG, "Cannot initialize adc unit 1");

    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config2, &adc_unit_handles[1]),
                        ADC_TAG, "Cannot initialize adc unit 2");
    return ESP_OK;

}
// find maps of unit and pins

esp_err_t adc_init_pins(adc_pin_config_t* pin_config, gpio_num_t num)
{
    // retrieve location unit/channel
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(num, &(pin_config->unit_id), &(pin_config->channel_id)), 
                        ADC_TAG, "Cannot retrieve id channel from num");

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    int unit_id = pin_config->unit_id;

    // configure the channel
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(adc_unit_handles[unit_id], pin_config->channel_id, &config),
                    ADC_TAG, "Error in configuring channel");

    pin_config->is_calibrated = false;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = pin_config->unit_id,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // retrieve calibration handle
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_line_fitting(&cali_config, &(pin_config->cali_handle)),
                        ADC_TAG, "Cannot retrieve adc calibration handle");
    pin_config->is_calibrated = true;
    
    return ESP_OK;
}

esp_err_t adc_read_pin(adc_pin_config_t pin_config, int* raw, int* mV)
{
    int unit_id = pin_config.unit_id;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_unit_handles[unit_id], pin_config.channel_id, raw),
                        ADC_TAG, "Cannot retrieve the adc reading");
    ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(pin_config.cali_handle, *raw, mV),
                        ADC_TAG, "Failed conversion from raw to voltage");
    return ESP_OK;
}



esp_err_t ams_init_dev()
{
    ams5600_dev.addr = AMS5600_ADDR;
    ams5600_dev.port = I2C_NUM_0;
    ams5600_dev.cfg.sda_io_num = AMS5600_SDA;
    ams5600_dev.cfg.scl_io_num = AMS5600_SCL;
    ams5600_dev.cfg.master.clk_speed = 400000;
    return ESP_OK;
}

esp_err_t ams_read(uint16_t *angle)
{
    uint8_t buffer[2];

    // The i2cdev_read function is generally safer and more flexible than read_word
    // It reads data from a specific 8-bit register address.
    //
    // i2c_dev_read_reg(const i2c_dev_t *dev, uint8_t reg, void *data, size_t size)

    ESP_RETURN_ON_ERROR(i2c_dev_check_present(&ams5600_dev),
                        AMS_TAG, "Ams not showing on the i2c bus");
    ESP_RETURN_ON_ERROR(i2c_dev_read_reg(&ams5600_dev, AMS5600_REG_RAW_ANGLE_H, buffer, 2),
                        AMS_TAG, "Cannot read register of ams"); 

    // The AS5600 outputs MSB first (Big-Endian).
    // The i2cdev_read returns bytes into the buffer in the order they are read: [MSB, LSB]
    // We combine them into a 16-bit word.
    uint16_t raw_angle = (buffer[0] << 8) | buffer[1];
    
    ESP_LOGI(AMS_TAG, "Angle read %u", *angle);
    // The angle is only 12 bits, so we mask the result (0x0FFF = 4095 decimal)
    *angle = raw_angle & 0x0FFF;

    return ESP_OK;
}

esp_err_t aht_init_dev()
{
    aht_dev.mode = AHT_MODE_NORMAL;
    aht_dev.type = AHT_TYPE_AHT20;

    ESP_RETURN_ON_ERROR(aht_init_desc(&aht_dev, AHT_I2C_ADDRESS_GND, 0, AHT20_SDA, AHT20_SCL),
                        AHT_TAG, "Cannot init aht device descriptor") ;
    ESP_RETURN_ON_ERROR(aht_init(&aht_dev),
                        AHT_TAG, "Cannot init aht i2c device");

    bool calibrated;
    ESP_RETURN_ON_ERROR(aht_get_status(&aht_dev, NULL, &calibrated),
                        AHT_TAG, "Cannot retrieve calibration");
    if (calibrated)
        ESP_LOGI(AHT_TAG, "Sensor calibrated");
    else
        ESP_LOGW(AHT_TAG, "Sensor not calibrated!");

    return ESP_OK;

}

esp_err_t aht_read(float* temp, float* hum)
{
    esp_err_t res = aht_get_data(&aht_dev, temp, hum);
    return res;
}

esp_err_t bmp_init_dev()
{
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    memset(&bmp_dev, 0, sizeof(bmp280_t));

    params.mode = BMP280_MODE_FORCED;
    params.filter = BMP280_FILTER_8;

    params.oversampling_temperature = BMP280_ULTRA_HIGH_RES;
    params.oversampling_pressure = BMP280_ULTRA_HIGH_RES;

    ESP_RETURN_ON_ERROR(bmp280_init_desc(&bmp_dev, BMP280_I2C_ADDRESS_1, I2C_NUM_0, BMP280_SDA, BMP280_SCL),
                        BMP_TAG, "Cannot initialize bmp device descriptor");

    ESP_RETURN_ON_ERROR(bmp280_init(&bmp_dev, &params),
                        BMP_TAG, "Cannot initialize bmp device");
    return ESP_OK;

}

esp_err_t bmp_read(float* temp, float* press, float* hum)
{
    bool busy = true;
    bool measure_is_valid = false;

    bmp280_force_measurement(&bmp_dev);
    
    while(!measure_is_valid)
    {
        while(busy)
        {
            bmp280_is_measuring(&bmp_dev, &busy);            
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (bmp280_read_float(&bmp_dev, temp, press, hum) != ESP_OK)
        {
            printf("Temperature/pressure reading failed");
        }

        // check bounds
        if(*temp > 60. || *temp < -30. || *press > 120000. || *press < 80000 )
        {
            busy = 0;
            bmp280_force_measurement(&bmp_dev);
        }
        else 
        {
            measure_is_valid = true;
        }

    }
    return ESP_OK;
}

