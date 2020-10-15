#define INTERNAL_TEMP_PORT Serial
#define INTERNAL_TEMP_PRINTF(f_, ...) //INTERNAL_TEMP_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

#define TEMP_OVER_PROTECT   90.0f

static float temp_last;
static uint8_t temp_over_sample, team_log_sample;
float esp32_internal_temp()
{
    float temp;
    temp = (temprature_sens_read() - 32) / 1.8;
    INTERNAL_TEMP_PRINTF("\r\nTemp: %.2f", temp);
    return temp;
}

void internal_temp_log_report()
{
    float temp;

    if (internal_temp_to.ToEExpired()) {
        internal_temp_to.ToEUpdate(1000);
        temp = esp32_internal_temp();
        if (fabs(temp - temp_last) >= 2.0f)
        {
            ++team_log_sample;
            if(2 < team_log_sample)
            {
                temp_last = temp;
                team_log_sample = 0;
                INTERNAL_TEMP_PRINTF("\r\nReport temp");
                log_report(LOG_REPORT_TEMP, (char*)"Update temperature");
            }
        }
        else
        {
            team_log_sample = 0;
        }
        

        if( temp > TEMP_OVER_PROTECT)
        {
            ++temp_over_sample;
            if (2 < temp_over_sample) {
                temp_over_sample = 0;
                INTERNAL_TEMP_PRINTF("\r\nOver temp");
                log_report(LOG_REPORT_OVER_TEMP, (char*)"Over temperature");
                // deep sleep
                timer_wakeup_second_enable(120);
            }
        }
        else
        {
            temp_over_sample = 0;
        }        
    }      
}