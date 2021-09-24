#include <time.h> // time() ctime()
#ifdef ESP8266
#include <sys/time.h> // struct timeval
#endif
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_SerialTrace.h"

#define ASYNC_NTP_CONSOLE(...) CONSOLE_LOGI(__VA_ARGS__)
#define ASYNC_NTP_TAG_CONSOLE(...) CONSOLE_TAG_LOGI("[ASYNC_NTP]", __VA_ARGS__)
#define ASYNC_NTP_FUNCTION_CONSOLE(...) FUNCTION_TAG_LOGI("[ASYNC_NTP]", __VA_ARGS__);

static void dumpNTPPacket (uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        ASYNC_NTP_CONSOLE ("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            ASYNC_NTP_CONSOLE ("\r\n");
        } else if ((i + 1) % 4 == 0) {
            ASYNC_NTP_CONSOLE ("| ");
        }
    }
}

ESPAsyncEasyNTP::ESPAsyncEasyNTP(/* args */)
{
    _ipServer = IPAddress(132, 163,  96,  1);   // time.nist.gov
}

ESPAsyncEasyNTP::~ESPAsyncEasyNTP()
{
    end();
}

void ESPAsyncEasyNTP::begin(long gmtOffset, int daylightOffset, const char *server)
{
    _server = server;
    ASYNC_NTP_TAG_CONSOLE("gmtOffset: %u", gmtOffset);
    ASYNC_NTP_TAG_CONSOLE("daylightOffset: %u", daylightOffset);
    ASYNC_NTP_TAG_CONSOLE("server: %s", server);
    this->setTimeZone(-gmtOffset, daylightOffset);
    _udp.onPacket([](void * arg, AsyncUDPPacket& packet){ ((ESPAsyncEasyNTP*)(arg))->_onPacket(packet); }, this);
}

void ESPAsyncEasyNTP::runAsync()
{
    ASYNC_NTP_FUNCTION_CONSOLE("IN");
    if(_udp.connected()){
        ASYNC_NTP_TAG_CONSOLE("ntp server is Connected");
        sendNTPpacket();
    }
    else
    {
        ASYNC_NTP_TAG_CONSOLE("Connect to ntp server");
        if (_udp.connect(_ipServer, NTP_REQUEST_PORT))
        {
            sendNTPpacket();
        }
    }
    ASYNC_NTP_FUNCTION_CONSOLE("OUT");
}

void ESPAsyncEasyNTP::_onPacket(AsyncUDPPacket& packet){
    ASYNC_NTP_FUNCTION_CONSOLE("UDP packet received");
    ASYNC_NTP_TAG_CONSOLE("UDP Packet Type: %s, \r\nFrom: %s:%d, To: %s:%d, \r\nLength: %u, Data:",
        packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast",
        packet.remoteIP().toString().c_str(),
        packet.remotePort(),
        packet.localIP().toString().c_str(),
        packet.localPort(),
        packet.length());
    dumpNTPPacket (packet.data(), packet.length());

    uint8_t *ntpPacketBuffer = (uint8_t*)packet.data();
    if (packet.length() >= NTP_PACKET_SIZE) {
        unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
        unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);

        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        ASYNC_NTP_TAG_CONSOLE("Seconds since Jan 1 1900 = %lu", secsSince1900);

        // now convert NTP time into )everyday time:
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        
        // subtract seventy years:
        unsigned long epoch = secsSince1900 - seventyYears;
        time_t epoch_t = epoch;   //secsSince1900 - seventyYears;
        
        // print Unix time:
        ASYNC_NTP_TAG_CONSOLE("Epoch/Unix NTP = %lu", epoch);
        ASYNC_NTP_TAG_CONSOLE("Epoch/Unix system = %lu", ESPTime.now());
        ASYNC_NTP_TAG_CONSOLE("The UTC/GMT time is %s", ESPTime.toString(epoch).c_str());
        ESPTime.set(epoch);
    }
}

void ESPAsyncEasyNTP::sendNTPpacket() {
    ASYNC_NTP_FUNCTION_CONSOLE("START");
    uint8_t ntpPacketBuffer[NTP_PACKET_SIZE]; //Buffer to store request message
                                              // set all bytes in the buffer to 0
    memset (ntpPacketBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    ntpPacketBuffer[0] = 0b11100011;   // LI, Version, Mode
    ntpPacketBuffer[1] = 0;     // Stratum, or type of clock
    ntpPacketBuffer[2] = 6;     // Polling Interval
    ntpPacketBuffer[3] = 0xEC;  // Peer Clock Precision
                                // 8 bytes of zero for Root Delay & Root Dispersion
    ntpPacketBuffer[12] = 49;
    ntpPacketBuffer[13] = 0x4E;
    ntpPacketBuffer[14] = 49;
    ntpPacketBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    _udp.write(ntpPacketBuffer, NTP_PACKET_SIZE);
}

void ESPAsyncEasyNTP::setTimeZone(long offset, int daylight)
{
    char cst[17] = {0};
    char cdt[17] = "DST";
    char tz[33] = {0};

    if(offset % 3600){
        sprintf(cst, "UTC%ld:%02u:%02u", offset / 3600, abs((offset % 3600) / 60), abs(offset % 60));
    } else {
        sprintf(cst, "UTC%ld", offset / 3600);
    }
    if(daylight != 3600){
        long tz_dst = offset - daylight;
        if(tz_dst % 3600){
            sprintf(cdt, "DST%ld:%02u:%02u", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
        } else {
            sprintf(cdt, "DST%ld", tz_dst / 3600);
        }
    }
    sprintf(tz, "%s%s", cst, cdt);
    setenv("TZ", tz, 1);
    tzset();
}

void ESPAsyncEasyNTP::end(){
    if(_udp.connected()){
        _udp.close();
    }
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_EASYNTP)
ESPAsyncEasyNTP EASYNTP;
#endif
