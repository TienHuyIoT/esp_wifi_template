## ESP wifi template
ESP32 and ESP8266 Arduino wifi template 

### Features
**Popular**
  - WiFI AP mode
  - WiFI STA mode
  - ETH mode
  - DNS server for AP mode
  - Async DDNS client
  - mDNS service for WiFI  STA mode
  - NBNS service for WiFI  STA mode
  - OTA Arduino with PIN option
  - Web update application
  - Web update spiffs/littleFS
  - Web monitor system params
  - Web configure WiFi AP and STA mode
  - SNTP run Async or impl by system
  - SPIFFS/LittleFS web Editor
  - SD Card web Editor
  - System params configure loaded from SPIFFS/LittleFS
  - LED display WiFi/ETH status
  - Button set factory default system params
  - Log File operating system into SPIFFS/LittleFS

**Ethernet IC supported**
  - ENC28J60 for ESP8266
  - W5100 for ESP8266
  - w5500 for ESP8266
  - LAN8720A for ESP32
  - TLK110 for ESP32

**File system handle**
  - SPIFFS
  - LittleFS
  - SD card with SPI or SDMMC(only ESP32) interface

**Async WebServer**
  - Websocket
  - Event Source
  - File system editor
  - Updater application
  - Updater file system

### THIoT_ESPConfig.h
| Link | Description |
| ------ | ------ |
| ETH_ENABLE | Enable/disable Ethernet |
| SD_CARD_ENABLE | Enable/disable SD card |
| SD_SPI_INTERFACE | SPI/MMC interface |
| USE_NAND_FS_SYSTEM | SPIFFS/LittleFS/FATFS system |
| ... | Some another configure |

### THIoT_ESPBoard.h
| Link | Description |
| ------ | ------ |
| LED_STATUS_GPIO | LED Pin |
| FACTORY_INPUT_PIN | Factory button (hold 2s) |
| ETH_GPIO_ENABLE | Input select WIFI/ETH mode |
| ... | Some another configure |

### Libraries
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - C++
- [AsyncTCP ESP32](https://github.com/me-no-dev/AsyncTCP) - C++
- [AsyncTCP ESP8266](https://github.com/me-no-dev/ESPAsyncTCP) - C++
- [asyncHTTPrequest](https://github.com/boblemaire/asyncHTTPrequest) - C++
- [EasyDDNS](https://github.com/ayushsharma82/EasyDDNS) - C++
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson/tree/5.x) - C++
- [oneButton](https://github.com/mathertel/OneButton) - C++

### HttpServer page
| Link | Description |
| ------ | ------ |
| /wifi.htm | setting wifi access point and station |
| /index.htm | Monitor some information system as Heap memory, temperature |
| /edit | web editor internal file system |
| /edit_sdfs | web editor SD card file system |
| Authorize default | pass: admin, user" admin |
| Pin default | 1234 |

### Http request API
| Link | Description |
| ------ | ------ |
| /get?param_wifi=restart | Restart ESP |
| /get?param_wifi=fw_version | Firmware version |

### Usage API file system editor
**Request to download file from sd card (method GET)**
  - /edit_sdfs?download=/file.txt
  - /edit_sdfs?download=/file.txt&filename=newName.txt

**Request to edit file from sd card (method GET)**
  - /edit_sdfs?edit=/file.txt

**Request to delete file from sd card (method DELETE)**
  - /edit_sdfs , path=/file.txt

**Request to download file from spiffs/LittleFS (method GET)**
  - /edit?download=/file.txt
  - /edit?download=/file.txt&filename=newName.txt

**Request to edit file from spiffs/LittleFS (method GET)**
  - /edit?edit=/file.txt

**Request to delete file from spiffs/LittleFS (method DELETE)**
  - /edit , path=/file.txt

### Usage ESP8266 Sketch Data Upload
**1. Download ESP8266LittleFS-2.6.0.zip**
  - Link: https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases
  - Unpack the file into C:\Users\PC-NAME\Documents\Arduino\hardware\esp8266com\esp8266\tools\mklittlefs\mklittlefs.exe

### Usage ESP32 Sketch Data Upload
**1. Download esp32fs.zip**
  - link: https://github.com/lorol/arduino-esp32fs-plugin/releases
  - Unpack the tool into "C:\Users\PC-NAME\Documents\Arduino\tools\ESP32FS\tool\esp32fs.jar"

**2. Download x86_64-w64-mingw32-mklittlefs-295fe9b.zip**
  - Link: https://github.com/earlephilhower/mklittlefs/releases
  - Unpack the file into C:\Users\PC-NAME\Documents\Arduino\hardware\espressif\esp32\tools\mklittlefs\mklittlefs.exe

**3. Download mkfatfs**
  - Link: https://github.com/labplus-cn/mkfatfs/releases/tag/v2.0.1
  - Unpack and rename the file into C:\Users\PC-NAME\Documents\Arduino\hardware\espressif\esp32\tools\mkfatfs\mkfatfs.exe

**4. mkspiffs.exe is already in directory**
  - C:\Users\PC-NAME\Documents\Arduino\hardware\espressif\esp32\tools\mkspiffs\mkspiffs.exe

**5. Install LITTLEFS**
  - Link: https://github.com/lorol/LITTLEFS
  - Or Manage Libraries with search "LITTLEFS"

### Sketch Data Upload
  - Restart Arduino IDE after any steps above
  - Tools --> ESP32 Sketch Data Upload --> [LITTLEFS, SPIFFS, FATFS, Erase Flash]
  - Tools --> ESP8266 LittleFS Data Upload
  - Tools --> ESP8266 Sketch Data Upload

### Update latest BLE Client Library to allow change MTU size (Option)
- [BLEClient.cpp](https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEClient.cpp) - C++
- [BLEClient.h](https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEClient.h) - C++

### Tools using convert gzip
- [Online Convert gzip](https://online-converting.com/archives/convert-to-gzip/) - Select "Compress this file, output-gz"
- [App PeaZip convert gzip](https://peazip.github.io/index.html)
- [Online Convert file to array C](http://tomeko.net/online_tools/file_to_hex.php?lang=en)

### Tools usage json
- [Check json online](http://json.parser.online.fr/)
- [Json arduino assistant](https://arduinojson.org/v5/assistant/)

### Some update ESPAsyncWebserver library to handle new port
- [Commit Upgrade for IDF4.4](https://github.com/TienHuyIoT/ESPAsyncWebServer/commit/15047582660096ce478df061b014545f131197e5)
- [Commit Enable authentication callback](https://github.com/TienHuyIoT/ESPAsyncWebServer/commit/6eb5b1b8914b4a36d9d655350734c1dedeaeeba7)
- [Commit Enable change tcp port](https://github.com/TienHuyIoT/ESPAsyncWebServer/commit/68610ad0c9efe5666f71529c78c008d41f0324e3)

### Some update AsyncTCP library to handle new port for ESP32
- [Commit Enable change tcp port](https://github.com/TienHuyIoT/AsyncTCP/commit/66c82d67ff3e2571d3b4989529bb6511060f377f)
