extern hw_timer_t *timer;

void IRAM_ATTR hw_resetModule(void)
{
    esp_restart();
}

void hw_wdt_init(uint32_t t_milisec)
{
    timer = timerBegin(0, 80, true);                  //timer 0, div 80
    timerAttachInterrupt(timer, &hw_resetModule, true);  //attach callback
    timerAlarmWrite(timer, t_milisec * 1000, false); //set time in us
    timerAlarmEnable(timer);                          //enable interrupt
}

void hw_wdt_feed(void)
{
    timerWrite(timer, 0); //reset timer (feed watchdog)
}