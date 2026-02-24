idf_component_register(SRCS "main.c" "https_client.c" "wifi.c"  "sensors.c"
                       REQUIRES 
                            nvs_flash
                            esp_wifi
                            esp_netif
                            mbedtls
                            esp-tls
                            esp_http_client
                            esp_adc
                            ulp
                            spi_flash
                            json
                       INCLUDE_DIRS "")


set(ulp_app_name ulp_main)

set(ulp_riscv_sources "ulp/main.c")

set(ulp_exp_dep_srcs "main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")
