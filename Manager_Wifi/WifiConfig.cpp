#include "WifiConfig.h"
#include "FileConfig.h"

WifiConfigClass::WifiConfigClass()
: WifiSTATo(600000),WifiSTADisTo(30000)
{
  wlStatus = WL_IDLE_STATUS;
  LedPresent = false;
}

void WifiConfigClass::WifiLedStatus(IOBlink *Led){
  LedStatus = Led;
  LedPresent = true;
}

void WifiConfigClass::WifiBuzzStatus(IOBlink *Buzz){
  BuzzStatus = Buzz;
  BuzzPresent = true;
}

bool WifiConfigClass::WifiConnect(void) {  
  WIFI_DEBUG("STA Mac: %s", WiFi.macAddress().c_str());  
  uint8_t SsidLeng = strlen(FileConfig.ConfigFile.STA.ssid);
  if (SsidLeng == 0){
    WiFi.disconnect();
    WIFI_DEBUG("No SSID");
    return false;
  }  
  WIFI_DEBUG("Connecting as wifi client %s - %s",FileConfig.ConfigFile.STA.ssid, FileConfig.ConfigFile.STA.pass);
  /*
  if (FileConfig.ConfigFile.STA.SaticIp) {
    WiFi.config(FileConfig.ConfigFile.STA.Ip, FileConfig.ConfigFile.STA.Gw, FileConfig.ConfigFile.STA.Sn, FileConfig.ConfigFile.STA.Dns);
    WIFI_DEBUG("Static IP enable");
    WIFI_DEBUG("Ip: %s", FileConfig.ConfigFile.STA.Ip.toString().c_str());
    WIFI_DEBUG("Gw: %s", FileConfig.ConfigFile.STA.Gw.toString().c_str());
    WIFI_DEBUG("Sn: %s", FileConfig.ConfigFile.STA.Sn.toString().c_str());
    WIFI_DEBUG("Dns: %s", FileConfig.ConfigFile.STA.Dns.toString().c_str());
  }
  */
#ifdef ESP8266
  WiFi.hostname(FileConfig.ConfigFile.STA.HostName); //Tên đăng ký vào router
#elif defined(ESP32)
  WiFi.setHostname(FileConfig.ConfigFile.STA.HostName); //Tên đăng ký vào router
#endif  
  if (strlen(FileConfig.ConfigFile.STA.pass) >= 8) WiFi.begin(FileConfig.ConfigFile.STA.ssid, FileConfig.ConfigFile.STA.pass);
  else WiFi.begin(FileConfig.ConfigFile.STA.ssid);
  WifiSTATo.ToEUpdate(600000ul);  //Tạo lại timeout kết nối
  WifiSTADisTo.ToEUpdate(30000ul);
  return true;
}

void WifiConfigClass::WifiConnectStatusProc(void) {
  //for reference:
  //typedef enum {
  //    WL_NO_SHIELD = 255,   // for compatibility with WiFi Shield library
  //    WL_IDLE_STATUS = 0,
  //    WL_NO_SSID_AVAIL = 1,
  //    WL_SCAN_COMPLETED = 2,
  //    WL_CONNECTED = 3,
  //    WL_CONNECT_FAILED = 4,
  //    WL_CONNECTION_LOST = 5,
  //    WL_DISCONNECTED = 6
  //} wl_status_t;
  wl_status_t St = WiFi.status();
  if (wlStatus != St) {
    wlStatus = St;
    WIFI_DEBUG("wlStatus: %u", (uint8_t)St);
    if (St == WL_CONNECTED) {
      if(LedPresent==true) LedStatus->attach_ms(111, 100, 900);
      if(BuzzPresent==true) BuzzStatus->attach_ms(3, 100, 200);
      FileConfig.ConfigFile.STA.Ip = WiFi.localIP();
      FileConfig.ConfigFile.STA.Gw = WiFi.gatewayIP();
      FileConfig.ConfigFile.STA.Sn = WiFi.subnetMask();
      FileConfig.ConfigFile.STA.Dns = WiFi.dnsIP();
      if (!FileConfig.ConfigFile.STA.SaticIp) {
        WIFI_DEBUG("Connected to %s OK", FileConfig.ConfigFile.STA.ssid);
        WIFI_DEBUG("Ip: %s", FileConfig.ConfigFile.STA.Ip.toString().c_str());
        WIFI_DEBUG("Gw: %s", FileConfig.ConfigFile.STA.Gw.toString().c_str());
        WIFI_DEBUG("Sn: %s", FileConfig.ConfigFile.STA.Sn.toString().c_str());
        WIFI_DEBUG("Dns: %s", FileConfig.ConfigFile.STA.Dns.toString().c_str());
      }
    } else if (St == WL_NO_SSID_AVAIL) {
      WiFi.disconnect();
      if(LedPresent==true)LedStatus->attach_ms(111, 100, 1900);
      if(BuzzPresent==true) BuzzStatus->attach_ms(10, 100, 2900);
      WifiSTATo.ToEUpdate(60000ul);  //Tạo lại timeout kết nối 1p lan dau tien khoi dong
    }
  }

  if (WifiSTADisTo.ToEExpired()){
    if (St != WL_CONNECTED) WiFi.disconnect();
  }
  
  if (!WifiSTATo.ToEExpired()) return;
  WifiSTATo.ToEUpdate(600000ul);
  if(St == WL_IDLE_STATUS
  || strcmp(WiFi.SSID().c_str(),FileConfig.ConfigFile.STA.ssid) 
  || strcmp(WiFi.psk().c_str(),FileConfig.ConfigFile.STA.pass)) {
    WiFi.disconnect();
    WifiConnect();
  }
}

void WifiConfigClass::WifiInit(void){
  if (FileConfig.ConfigFile.STA.SaticIp) {
    WiFi.mode(WIFI_OFF);
    WiFi.config(FileConfig.ConfigFile.STA.Ip, FileConfig.ConfigFile.STA.Gw, FileConfig.ConfigFile.STA.Sn, FileConfig.ConfigFile.STA.Dns);
    WIFI_DEBUG("Static IP enable");
    WIFI_DEBUG("Ip: %s", FileConfig.ConfigFile.STA.Ip.toString().c_str());
    WIFI_DEBUG("Gw: %s", FileConfig.ConfigFile.STA.Gw.toString().c_str());
    WIFI_DEBUG("Sn: %s", FileConfig.ConfigFile.STA.Sn.toString().c_str());
    WIFI_DEBUG("Dns: %s", FileConfig.ConfigFile.STA.Dns.toString().c_str());
  }
  if (strlen(FileConfig.ConfigFile.AP.ssid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    //Cau hinh AP
    WIFI_DEBUG("Configuring access point %s - %s", FileConfig.ConfigFile.AP.ssid, FileConfig.ConfigFile.AP.pass);
    WiFi.softAPConfig(FileConfig.ConfigFile.AP.Ip, FileConfig.ConfigFile.AP.Ip, FileConfig.ConfigFile.AP.Sn);
    #ifdef ESP8266
      String ChipID = String(ESP.getChipId(), HEX);
      ChipID.toUpperCase();
      String Ssdi_Ap = String(FileConfig.ConfigFile.AP.ssid) + "_" + ChipID;
    #elif defined(ESP32)
      String ChipID = String((uint32_t)(ESP.getEfuseMac()>>16), HEX);
      ChipID.toUpperCase();
      String Ssdi_Ap = String(FileConfig.ConfigFile.AP.ssid) + "_" + ChipID;
    #endif 
    Ssdi_Ap.toUpperCase();  
    if (strlen(FileConfig.ConfigFile.AP.pass) >= 8) {
      WiFi.softAP(Ssdi_Ap.c_str(), FileConfig.ConfigFile.AP.pass, FileConfig.ConfigFile.AP.Chanel, FileConfig.ConfigFile.AP.Hidden);
    } else WiFi.softAP(Ssdi_Ap.c_str());
    delay(500);
    IPAddress myIP = WiFi.softAPIP();
    //In ra AP IP
    WIFI_DEBUG("AP IP address: %s",myIP.toString().c_str());
  } else {
    WiFi.mode(WIFI_STA);
  }
  for(uint8_t i=0;i<2;i++){
    if(WifiConnect()==true){
      if (WiFi.waitForConnectResult() == WL_CONNECTED) break;
      WIFI_DEBUG("Connect: Failed!");
      if(i==0){
        WiFiMode_t M = WiFi.getMode();  
        WiFi.mode(WIFI_OFF);  
        delay(5000);
        WiFi.mode(M);
        continue;
      }
      WifiSTATo.ToEUpdate(60000ul);  //Tạo lại timeout kết nối 1p lan dau tien khoi dong
      if(LedPresent==true)LedStatus->attach_ms(111, 100, 1900);
      if(BuzzPresent==true) BuzzStatus->attach_ms(10, 100, 2900);
    } 
  }   
}

wl_status_t WifiConfigClass::WifiStatus(){
  return wlStatus;
}

WifiConfigClass WifiConfig;




