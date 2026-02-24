#include "wifi.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"
#include <esp_check.h>

static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_handler;
static EventGroupHandle_t s_wifi_event_group = NULL;
static int wifi_retry_count = 0;

static void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(WIFI_TAG, "Handling IP event, event code 0x%" PRIx32, event_id);
    switch (event_id)
    {
    case (IP_EVENT_STA_GOT_IP):
        ip_event_got_ip_t* event_ip = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, IP_GOT_BIT);
        break;
    case (IP_EVENT_STA_LOST_IP):
        ESP_LOGI(WIFI_TAG, "Lost IP");
        break;
    case (IP_EVENT_GOT_IP6):
        ip_event_got_ip6_t* event_ip6 = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Got IPv6: " IPV6STR, IPV62STR(event_ip6->ip6_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, IP_GOT_BIT);
        break;
    default:
        ESP_LOGI(WIFI_TAG, "IP event not handled");
        break;
    }
}

static void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(WIFI_TAG, "Handling WIFI event, event code 0x%" PRIx32, event_id);
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(WIFI_TAG, "Starting wifi");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(WIFI_TAG, "Connected to AP");
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); 
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(WIFI_TAG, "Disconnected. Retrying...");
        if (wifi_retry_count < 5) { // Max 5 retries
            esp_wifi_connect();
            wifi_retry_count++;
        } else {
            ESP_LOGE(WIFI_TAG, "Wi-Fi connection failed after max retries.");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); 
        }
        break;
    default:
        break;
    }
}

esp_err_t wifi_connect()
{
    esp_err_t err = ESP_OK;

    ESP_RETURN_ON_ERROR(esp_netif_init(), WIFI_TAG, "Cannot init netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), WIFI_TAG, "Cannot initialize event loop");
    esp_netif_create_default_wifi_sta();

    s_wifi_event_group = xEventGroupCreate();
    wifi_retry_count = 0; // Reset retry counter on every run

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PSWD,
            .scan_method = WIFI_FAST_SCAN,
            .bssid_set = false,
        },
    };
    
    ESP_LOGI(WIFI_TAG, "Configuring Wi-Fi Station Mode...");
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), WIFI_TAG, "Cannot initialize wifi");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), WIFI_TAG, "Cannot set wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), WIFI_TAG, "Cannot set wifi config");
    if(err != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Cannot initialize wifi");
        return err;
    }

        
    err = esp_event_handler_instance_register(IP_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &ip_event_cb,
                                              NULL,
                                              &ip_event_handler);
    ESP_RETURN_ON_ERROR(err, WIFI_TAG, "Error in registering ip event handler");

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_cb,
                                              NULL,
                                              &wifi_event_handler);

    ESP_RETURN_ON_ERROR(err, WIFI_TAG, "Error in registering wifi event handler");

    
    ESP_LOGI(WIFI_TAG, "Attempting to connect to SSID: %s", WIFI_SSID);
    ESP_RETURN_ON_ERROR(esp_wifi_start(),
                        WIFI_TAG, "Cannot start wifi");

    ESP_RETURN_ON_ERROR(esp_wifi_connect(), WIFI_TAG, "Cannot connect to wifi"); 

    ESP_LOGI(WIFI_TAG, "Waiting for Wi-Fi connection...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                           IP_GOT_BIT | WIFI_FAIL_BIT, 
                                           pdFALSE, 
                                           pdFALSE, 
                                           pdMS_TO_TICKS(60000));

    ESP_LOGI(WIFI_TAG, "Wi-Fi synchronization complete. Cleaning up resources...");

    // Unregister handlers: Prevents the crash on subsequent runs
    // No, I leave them here only to remind me of how them work 
    // Leave them alive to handle events
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler));
    
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL; // Best practice

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Wi-Fi connected successfully! IP assigned.");
        return ESP_OK;
    } else {
        ESP_LOGE(WIFI_TAG, "Wi-Fi failed to connect. Check credentials/signal.");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
}
