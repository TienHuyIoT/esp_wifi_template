#ifndef	__ESP_SNTP_H
#define __ESP_SNTP_H
class ESPSntpService
{
private:
    static String _server1;
    static String _server2;
    static String _timeZone;
    static long _gtmOffset;
    static int _daylightOffset;

public:
    ESPSntpService(/* args */);
    ~ESPSntpService();
    void begin();
};

#endif // __ESP_SNTP_H
