#include <time.h> // time() ctime()
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <sys/time.h> // struct timeval
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_ESPAsyncEasyNTP.h"
#include "THIoT_SerialTrace.h"

#define ASYNC_NTP_CONSOLE(...) SERIAL_LOGI(__VA_ARGS__)
#define ASYNC_NTP_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[ASYNC_NTP]", __VA_ARGS__)
#define ASYNC_NTP_FUNCTION_CONSOLE(...) SERIAL_FUNCTION_TAG_LOGI("[ASYNC_NTP]", __VA_ARGS__)

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
    // _ipServer = IPAddress(132, 163,  96,  1);  // time.nist.gov
    // _ipServer = IPAddress(129, 6,  15,  28);   // time.nist.gov
    _onSyncEvent = nullptr;
    _interval = 0;
}

ESPAsyncEasyNTP::~ESPAsyncEasyNTP()
{
    end();
}

void ESPAsyncEasyNTP::begin(long gmtOffset, int daylightOffset, const char *server, int interval)
{
    _server = server;
    _interval = interval;
    ASYNC_NTP_TAG_CONSOLE("gmtOffset: %u", gmtOffset);
    ASYNC_NTP_TAG_CONSOLE("daylightOffset: %u", daylightOffset);
    ASYNC_NTP_TAG_CONSOLE("server: %s", server);
    ESPTime.setTimeZone(-gmtOffset, daylightOffset);
    _udp.onPacket([](void * arg, AsyncUDPPacket& packet){ ((ESPAsyncEasyNTP*)(arg))->_onPacket(packet); }, this);
    runAsync();
}

void ESPAsyncEasyNTP::runAsync(int interval)
{
    if (0 != interval)
    {
        _interval = interval;
    }

    constexpr int INTERVAL_MINIMUM = 15;
    if (_interval < INTERVAL_MINIMUM) {
        _interval = INTERVAL_MINIMUM;
    }

    ASYNC_NTP_FUNCTION_CONSOLE("interval %us", INTERVAL_MINIMUM);
    // The first time running always is failed. So should be run here to reduce waiting time.
    WiFi.hostByName(_server.c_str(), _ipServer);
    _tickerRunAsync.detach();
    _tickerRunAsync.attach<void*>(INTERVAL_MINIMUM, [](void* arg){((ESPAsyncEasyNTP*)(arg))->requestTime();}, this);
}

void ESPAsyncEasyNTP::requestTime()
{
    if (_udp.connected())
    {
        sendNTPpacket();
    }
    else
    {
        ASYNC_NTP_FUNCTION_CONSOLE("run hostByName %s", _server.c_str());
        if (WiFi.hostByName(_server.c_str(), _ipServer))
        {
            ASYNC_NTP_FUNCTION_CONSOLE("got ntp server IP: %s", _ipServer.toString().c_str());
            if (_udp.connect(_ipServer, NTP_REQUEST_PORT))
            {
                ASYNC_NTP_TAG_CONSOLE("udp socket connected");
                sendNTPpacket();
            }
        }
        else
        {
            ASYNC_NTP_FUNCTION_CONSOLE("hostByName IP Failed!");
        }
    }
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
        ASYNC_NTP_TAG_CONSOLE("Epoch/Unix NTP = %lu", epoch_t);

        if (_onSyncEvent) {
            struct timeval val;
            val.tv_sec = epoch_t;
            _onSyncEvent(&val);
        }

        _tickerRunAsync.detach();
        ASYNC_NTP_FUNCTION_CONSOLE("runAsync after %us", _interval);
        _tickerRunAsync.once<void*>(_interval, [](void* arg){((ESPAsyncEasyNTP*)(arg))->runAsync();}, this);
    }
    ASYNC_NTP_TAG_CONSOLE("Close udp socket");
    _udp.close();
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

void ESPAsyncEasyNTP::end() {
    if(_udp.connected()) {
        _udp.close();
    }
    _tickerRunAsync.detach();
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_EASYNTP)
ESPAsyncEasyNTP EASYNTP;
#endif
