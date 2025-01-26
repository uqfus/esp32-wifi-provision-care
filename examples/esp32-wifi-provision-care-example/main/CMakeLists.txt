idf_component_register(SRCS "esp32-wifi-provision-care-example.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif esp_wifi esp_http_server nvs_flash )