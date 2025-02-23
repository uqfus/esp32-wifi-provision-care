idf_component_register(SRCS esp32-wifi-provision-care.c
                    INCLUDE_DIRS .
                    EMBED_FILES "esp32-wifi-provision-care-favicon.ico"
                    PRIV_REQUIRES esp_netif esp_wifi nvs_flash esp_http_server app_update json )

add_custom_command(
    OUTPUT 
        non-existing.txt # force to execute on every build (https://gitlab.kitware.com/cmake/cmake/-/issues/17071)
        "${COMPONENT_DIR}/wifi.html.gz"
    WORKING_DIRECTORY ${COMPONENT_DIR}
    COMMAND python -c "import gzip,shutil;f_in=open('wifi.html','rb');f_out=gzip.open('wifi.html.gz','wb');shutil.copyfileobj(f_in,f_out)"
    VERBATIM
)

target_add_binary_data( ${COMPONENT_LIB} "wifi.html.gz" BINARY DEPENDS "${COMPONENT_DIR}/wifi.html.gz" )
