#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Tries to connect to Wi-Fi Access Point with 
 *         credentials stored in default NVS partition
 *         on success just return
 *         on error spawns SoftAP with captive portal
 *
 * @param  ap_ssid_name Wi-Fi AP SSID name if empty or NULL use "esp32-wifi-provision-care-XXXX"
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
esp_err_t updateota_post_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
