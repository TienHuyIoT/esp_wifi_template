#ifdef ESP8266
#define FACTORY_PORT Serial
#elif defined(ESP32)
#define FACTORY_PORT Serial
#endif
#define FACTORY_DBG(fmt, ...) FACTORY_PORT.printf("\r\n>FACTORY< " fmt, ##__VA_ARGS__)

#ifdef ESP8266
#define FACTORY_IO 0 //GPIO0
#elif defined(ESP32)
#define FACTORY_IO 0 //GPIO0
#endif
IOInput _IoFactory(FACTORY_IO,HIGH,10,10,10);
void TaskFactoryProc(void)
{
  _IoFactory.loop();
  if (_IoFactory.GetStatus() == LOW)
  {
    uint32_t T = _IoFactory.GetTime(LOW);
    if (T > 5000 && T < 10000)
    {
      _IoFactory.set_low_time_ms(10000);
      LED_STT.attach_ms(111, 1000, 1000); 
      FACTORY_DBG("OK");
      #ifdef ESP8266
      strncpy(FileConfig.ConfigFile.AP.ssid, (char*)"DTUD", Df_LengSsid);
      #elif defined(ESP32)
      strncpy(FileConfig.ConfigFile.AP.ssid, (char*)"DTUD", Df_LengSsid);
      #endif      
      strncpy(FileConfig.ConfigFile.AP.pass, "88888888", Df_LengPass);
      strncpy(FileConfig.ConfigFile.AUTH.user, "admin", Df_LengAuth);
      strncpy(FileConfig.ConfigFile.AUTH.pass, "admin", Df_LengAuth);
      memset(FileConfig.ConfigFile.STA.ssid, 0, Df_LengSsid);
      memset(FileConfig.ConfigFile.STA.pass, 0, Df_LengPass);
      FileConfig.ConfigFile.AP.Chanel = 2;
      FileConfig.ConfigFile.AP.Hidden = 0;
      FileConfig.ConfigFile.STA.SaticIp = 0;
      FileConfig.FS_FileConfig(Df_UpdateConfig);
    }
  }
}




