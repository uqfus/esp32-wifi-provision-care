idf_component_register(SRCS "esp32-firmware-upload-ota-example.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "ota.html"
                    PRIV_REQUIRES esp_netif esp_wifi nvs_flash esp_http_server app_update )