// need non block function
void esp_reset_enable()
{
    // save rtc current into fs before restart
    rtc_time_t rtc;
    rtc_get(&rtc);
    rtc_info_write(&rtc);

    delay(500);
    ESP.restart();
}