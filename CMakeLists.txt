idf_component_register(SRCS esp32-wifi-provision-care.c
                    INCLUDE_DIRS .
                    EMBED_FILES "favicon.ico" "wifi.html.gz"
                    PRIV_REQUIRES esp_netif esp_wifi nvs_flash esp_http_server json )