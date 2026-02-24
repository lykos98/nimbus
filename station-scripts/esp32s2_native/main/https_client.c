#include "https_client.h" 
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include <stdlib.h>
#include <string.h>
#define MIN(x,y) ((x < y ? x :y))

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_response_t * response = (http_response_t*) evt -> user_data;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(HTTP_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);


            int tot_len = MIN(evt->data_len + response->len, response->max_len);

            // allocate +1 bytes for null termination
            response->buffer = realloc(response->buffer, tot_len + 1);

            if (response->buffer == NULL) {
                    ESP_LOGE(HTTP_TAG, "Failed to reallocate buffer!");
                    return ESP_FAIL;
            }
            memcpy(response->buffer + response->len, evt->data, evt-> data_len);
            response->len = tot_len;

            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(HTTP_TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(HTTP_TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_FINISH");
            response->buffer[response->len] = '\0';
            // response->len = response->len + 1;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(HTTP_TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
        default:
            break;
    }
    return ESP_OK;
}

http_response_t https_get_request(const char* url) 
{
    http_response_t response = {
        .buffer = NULL,
        .complete = false,
        .max_len = MAX_HTTP_RECV_BUFFER,
        .len = 0
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = &response, 
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(HTTP_TAG, "Failed to initialize HTTP client");
        return response;
    }

    // perform the get
    ESP_ERROR_CHECK(esp_http_client_perform(client));
    esp_http_client_close(client);

    return response;
}

http_response_t https_post_json(const char* url, const char* data, const size_t data_len) 
{
    http_response_t response = {
        .buffer = NULL,
        .complete = false,
        .max_len = MAX_HTTP_RECV_BUFFER,
        .len = 0
    };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &response, 
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, data_len);
    if (client == NULL) {
        ESP_LOGE(HTTP_TAG, "Failed to initialize HTTP client");
        return response;
    }

    // perform the post

    ESP_ERROR_CHECK(esp_http_client_perform(client));
    ESP_LOGI(HTTP_TAG, "Got %s", response.buffer);
    esp_http_client_close(client);

    return response;
}

my_string_t nvs_retrieve_string(const char *key) {
    my_string_t out = {.buffer = NULL, .len = 0};
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    // 1. Open the namespace
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return out;
    }

    // 2. Get the required size of the string
    err = nvs_get_str(nvs_handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(NVS_TAG, "Key '%s' not found in NVS.", key);
        } else {
            ESP_LOGE(NVS_TAG, "Error reading string size for '%s': %s", key, esp_err_to_name(err));
        }
        nvs_close(nvs_handle);
        return out;
    }

    // 3. Allocate a buffer to hold the retrieved string
    out.buffer = (char *)malloc(required_size);
    out.len    = required_size;
    if (out.buffer == NULL) {
        ESP_LOGE(NVS_TAG, "Failed to allocate memory for string.");
        nvs_close(nvs_handle);
        return out;
    }

    // 4. Read the string into the allocated buffer
    err = nvs_get_str(nvs_handle, key, out.buffer, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error getting string '%s': %s", key, esp_err_to_name(err));
        free(out.buffer);
        out.buffer = NULL;
        out.len    = 0;
    } else {
        // ESP_LOGI(NVS_TAG, "String '%s' retrieved successfully: %s", key, out.buffer);
        ESP_LOGI(NVS_TAG, "String '%s' retrieved successfully", key);
    }

    // 5. Close the handle and return
    nvs_close(nvs_handle);
    return out;
}

esp_err_t nvs_store_string(const char *key, const char *value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // 1. Open a namespace (e.g., "storage")
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Write the string (key-value pair)
    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error setting string '%s': %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // 3. Commit the changes to flash
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error committing NVS changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(NVS_TAG, "String '%s' saved successfully.", key);
    }

    // 4. Close the handle
    nvs_close(nvs_handle);
    return err;
}
