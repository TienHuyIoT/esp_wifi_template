#ifdef ESP8266
#define DBG_WEBSOCKET_PORT Serial
#elif defined(ESP32)
#define DBG_WEBSOCKET_PORT Serial
#endif
#define DEBUGLOG_WEBSOCKET(...) DBG_WEBSOCKET_PORT.printf(__VA_ARGS__)

typedef struct
{
  uint16_t TL; //Timelive
  struct
  {
    uint8_t Num : 3;           //Tối đa 3 kết nối
    uint8_t St : 1;            //Trạng thái kết nối.   1: Connected, 0: Disconnect
    PAGE_TAG_TypeDef Page : 2; // Page kết nối
  };
  uint8_t Pump; //Pump index
} WEBSOCKET;
#define DF_WEBSOCKET_NUM 2 /*!< Num Socket connected limit*/
WEBSOCKET WedSk[DF_WEBSOCKET_NUM];
TimeOutEvent WsTo(1000); //Timer 1s
void WsTimeLive(void)
{
  if (WsTo.ToEExpired())
  {
    WsTo.ToEUpdate(1000);
    for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
    {
      if (WedSk[i].St)
        WedSk[i].TL++;
    }
  }
}

uint8_t WsTimeLiveMax(void)
{
  uint16_t TLMax = 0;
  uint8_t TLMaxId = 0;
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (TLMax < WedSk[i].TL)
      continue;
    TLMax = WedSk[i].TL;
    TLMaxId = i;
  }
  return TLMaxId;
}

uint8_t WsConnected(PAGE_TAG_TypeDef Page)
{
  uint8_t Wsc = 0;
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (WedSk[i].St && (WedSk[i].Page == Page))
      Wsc |= (1 << i);
  }
  return Wsc;
}

bool WsConnectFull(void)
{
  uint8_t i;
  //Kiểm tra Ws Full connected
  for (i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (!WedSk[i].St)
      break;
  }
  return (i == DF_WEBSOCKET_NUM) ? true : false;
}

void WsConnectNum(uint8_t Num)
{
  uint8_t i;
  for (i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (WedSk[i].St)
      continue;
    WedSk[i].St = 1;
    WedSk[i].Num = Num;
    WedSk[i].TL = 0;
    WedSk[i].Page = P_NULL;
    break;
  }
}

void WsConnectPage(uint8_t Num, uint8_t Page)
{
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (!WedSk[i].St || WedSk[i].Num != Num)
      continue;
    WedSk[i].Page = (PAGE_TAG_TypeDef)Page;
    DEBUGLOG_WEBSOCKET("\r\nWedSk[%u].page = %u", i, Page);
  }
}

void WsConnectPump(uint8_t Num, uint8_t Pump)
{
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (!WedSk[i].St || WedSk[i].Num != Num)
      continue;
    WedSk[i].Pump = Pump;
    DEBUGLOG_WEBSOCKET("\r\nWedSk[%u].Pump = %u", i, Pump);
  }
}

void WsDisConnectNum(uint8_t Num)
{
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (!WedSk[i].St || WedSk[i].Num != Num)
      continue;
    WedSk[i].St = 0;
  }
}

void WsDisConnectIndex(uint8_t Id)
{
  webSocket.disconnect(WedSk[Id].Num);
  WedSk[Id].St = 0;
  DEBUGLOG_WEBSOCKET("\r\nDisconnect WedSk[%u]", Id);
}

void WsSendData(char *Data, PAGE_TAG_TypeDef Page)
{
  for (uint8_t i = 0; i < DF_WEBSOCKET_NUM; i++)
  {
    if (!WedSk[i].St || WedSk[i].Page != Page)
      continue;
    webSocket.sendTXT(WedSk[i].Num, Data);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
    case WStype_DISCONNECTED:
      DEBUGLOG_WEBSOCKET("[%u] Disconnected!\n", num);
      WsDisConnectNum(num);
      break;
    case WStype_CONNECTED:
    {
      //Kiểm tra nếu kết nối full thì phải huỷ connect có Timelive lâu nhất
      if (WsConnectFull())
        WsDisConnectIndex(WsTimeLiveMax());
      //Tạo kết nối mới
      WsConnectNum(num);
      IPAddress ip = webSocket.remoteIP(num);
      DEBUGLOG_WEBSOCKET("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    }
    break;
    case WStype_TEXT:
    {
      DEBUGLOG_WEBSOCKET("[%u] get Text: %s\n", num, payload);         
    }
    break;
  }
}




