# esp32-wifi-provision-care-example

On windows build can fails with error CMAKE_OBJECT_PATH_MAX

```
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```
