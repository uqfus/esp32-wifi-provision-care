# esp32-firmware-upload-ota-example

This is a sample how to remotely upload a new firmware to your esp32 based project.

Provide Wi-Fi SSID name EXAMPLE_WIFI_SSID and Wi-Fi password EXAMPLE_WIFI_PASSWORD and keep wifi_init();
or uncommend wifi_provision_care((char *)TAG); and comment wifi_init();

```
idf.py menuconfig
```
Build sample
```
idf.py build
```
Upload to esp32 devboard
```
idf.py flash
```
Sample provides two way to upload new firmware:

1. Type in browser http://esp32-firmware-upload-ota-example.local/

Select file with new firmware and press Upload button. Esp32 restarts automatically with new firmware.

2. Use "curl" command. Use update_firmware.bat

```
curl --data-binary @build\esp32-firmware-upload-ota-example.bin http://esp32-firmware-upload-ota-example.local/updateota
```
Esp32 restarts automatically with new firmware.

If you wish you can ommit "ota.html" handler at all, an keep only /updateota handler. In this case "curl" method will work.
