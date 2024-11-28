# esp32-wifi-provision-care

Just connects your ESP32 to Wi-Fi using credentials in default NVS partition.
If ESP32 has no saved credentials or ESP32 fails to connect Wi-Fi
esp32-wifi-provision-care spawns Wi-Fi Access Point with captive portal.

Wi-Fi provisioning page shows Wi-Fi routers near to ESP32.

How to use.

Append
  esp32-wifi-provision-care:
    git: https://github.com/uqfus/esp32-wifi-provision-care.git
to your idf_component.yml file

Append code to your project
```
#include "esp32-wifi-provision-care.h"
```

To void app_main(void) add
```
    wifi_provision_care(NULL); // connect to wifi. AP SSID would be "esp32-wifi-provision-care-XXXX"
```
or
```
    wifi_provision_care(""); // connect to wifi. AP SSID would be "esp32-wifi-provision-care-XXXX"
```
or
```
    wifi_provision_care("MyLovelyESP32"); // connect to wifi. AP SSID would be "MyLovelyESP32"
```