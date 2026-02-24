#pragma once

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "nvs.h"
#define HTTP_TAG "http"
#define NVS_TAG "nvs"
#define MAX_HTTP_RECV_BUFFER (1024 * 100)

typedef struct {
    char *buffer;     // Pointer to the dynamically allocated payload buffer
    unsigned int len;          // Current length of the data in the buffer
    unsigned int max_len;      // Maximum allocated size of the buffer
    bool complete;    // Flag to indicate if the transfer finished
} http_response_t;

typedef struct {
    char* buffer;
    unsigned int len;
} my_string_t;

http_response_t https_get_request(const char* url);
http_response_t https_post_json(const char* url, const char* data, const size_t data_len);
esp_err_t nvs_store_string(const char *key, const char *value);
my_string_t nvs_retrieve_string(const char *key);

