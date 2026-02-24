#pragma once

#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "esp_netif.h"
#include <esp_system.h>
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"

#include <aht.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>

#include "config.h"


#if !defined(WIFI_SSID) || !defined(WIFI_PSWD)
    #error "Please define WIFI_SSID and WIFI_PWSD"
#endif

#define WIFI_FAIL_BIT BIT0
#define WIFI_CONNECTED_BIT BIT1
#define IP_GOT_BIT BIT3

#define WIFI_TAG "wifi"
#define ESP_MQTT_ERR 1


esp_err_t wifi_connect();
 
