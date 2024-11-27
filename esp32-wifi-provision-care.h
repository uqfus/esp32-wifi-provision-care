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

#ifdef __cplusplus
}
#endif
