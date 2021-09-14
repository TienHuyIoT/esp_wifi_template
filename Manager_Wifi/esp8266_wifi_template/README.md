## ESP wifi template
ESP32 and ESP8266 wifi template Project

### Features
- WIFI
  - AP mode
  - STA mode
  - DNS server for AP mode
  - Async DDNS client
  - mDNS service for STA mode
  - NBNS service for STA mode
  - OTA Arduino
  - SNTP

- File system handle
  - SPIFFS
  - LittleFS
  - SD card

- WebServer
  - Async TCP
  - Websocket
  - Event Source
  - Fs_editor
  - Updater app
  - Updater FS

### Libraries
- [ESPAsyncWebServer](https://github.com/TienHuyIoT/ESPAsyncWebServer/tree/feature/update_webserver_port) - C++
- [AsyncTCP](https://github.com/TienHuyIoT/AsyncTCP/tree/feature/update_tcp_port) - C++
- [asyncHTTPrequest](https://github.com/boblemaire/asyncHTTPrequest) - C++
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson/tree/5.x) - C++


### Usage API Fs editor
**Web editor**
  - IP/edit : editor internal file system
  - IP/edit_sdfs : editor external file system

**Request to download file from sd card (method GET)**
  - IP/edit_sdfs?download=/file.txt

**Request to edit file from sd card (method GET)**
  - IP/edit_sdfs?edit=/file.txt

**Request to delect file from sd card (method DELETE)**
  - IP/edit_sdfs , path=/file.txt

**Request to download file from spiffs/LittleFS (method GET)**
  - IP/edit?download=/file.txt

**Request to edit file from spiffs/LittleFS (method GET)**
  - IP/edit?edit=/file.txt

**Request to delect file from spiffs/LittleFS (method DELETE)**
  - IP/edit , path=/file.txt

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
- [Online Convert gzip](https://online-converting.com/archives/convert-to-gzip/)
```
Convert htm to gzip. Select "Compress this file, output-gz
```
- [App PeaZip](https://peazip.github.io/index.html)
- [Online Convert file to array C](http://tomeko.net/online_tools/file_to_hex.php?lang=en)

### Tools usage json
- [Check json online](http://json.parser.online.fr/)
- [Json arduino assistant](https://arduinojson.org/v5/assistant/)

### Some edit AsyncWebserver library
```C++
  WebHandlerlmpl.h edit
  line 32 add: typedef std::function<bool(AsyncWebServerRequest *request)> ArRequestAuthenticateFunction;
  line 49 add: ArRequestAuthenticateFunction _onAuthenticate;
  line 62 add: AsyncStaticWebHandler& onAuthenticate(ArRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}

  WebHandlers.cpp edit
  Line 193
  if(_username != "" && _password != "")
    {
      if(!request->authenticate(_username.c_str(), _password.c_str()))
      {
        return request->requestAuthentication();
      } 
    }     
    else
    {
      if(_onAuthenticate)
      {
        if(!_onAuthenticate(request))
        {
          return;
        }
      }
    }

  1. WebServer.cpp at line  82
Original
void AsyncWebServer::begin(){
  _server.setNoDelay(true);  
  _server.begin();
}

Change to

void AsyncWebServer::begin(uint16_t port){
  if (port != 0)
  {
    _server.port(port);
  }
  _server.setNoDelay(true);  
  _server.begin();
}

2. ESPAsyncWebServer.h at line 408
Original
void begin();

Change to

void begin(uint16_t port = 0);

3. AsyncTCP.cpp at line 1264
Original
void AsyncServer::begin()
{}

Change to

void AsyncServer::port(uint16_t port)
{
    _port = port;
}

void AsyncServer::begin()
{}

4. AsyncTCP.h at line 194
Original
void begin();

Change to

void port(uint16_t port);
void begin();
```