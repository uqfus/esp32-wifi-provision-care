#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_http_server.h>

/**
 * @brief  Tries to connect to Wi-Fi Access Point with 
 *         credentials stored in default NVS partition
 *         on success just return
 *         on error spawns SoftAP with captive portal
 *
 * @param  ap_ssid_name[32] Wi-Fi AP SSID name NULL terminated.
 *         If empty or NULL AP name set to "esp32-wifi-provision-care-XXXX" XXXX - esp32 MAC addr last digits.
 *
 */
void wifi_provision_care(char *ap_ssid_name);

/**
 * @brief Register POST handler with desired URI to handle upload firmware over te air
 *        const httpd_uri_t updateota_uri =  { .uri = "/updateota", .method = HTTP_POST, .handler = updateota_post_handler };
 *        httpd_register_uri_handler(server, &updateota_uri);
 *        Then use curl --data-binary @firmware.bin http://esp32.local/updateota to update firware wirelesly
 *        Or use html form in your application
 *
 * @param  req httpd_server parameters
 *
 */
esp_err_t wifi_provision_care_updateota_post_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
