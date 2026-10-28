#include <WiFi.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"
#include "THIoT_ESPPing.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"
#if (PING_SERVICE_SYSTEM == 1)
#define PING_TAG_CONSOLE(...)   //CONSOLE_TAG_LOGI("[PING]", __VA_ARGS__)
#define PING_TAG_LOG(...)       FS_TAG_LOGI("[PING]", __VA_ARGS__)

typedef enum {
    PING_EV_NO_EVENT = 0x0000,
    PING_EV_TRANS_START = 0x0001,             /*!< Transaction start flag */
    PING_EV_PROCESS_SUCCESS = 0x0002,         /*!< process success. */
    PING_EV_ALL_EVENT = 0xFFFF
} pingEventEnum;

// Event bit mask for eMBMasterWaitRequestFinish()
#define PING_EVENT_REQ_MASK   (EventBits_t)( PING_EV_PROCESS_SUCCESS )
#define PING_CHECK_EVENT( event, mask ) ( event & mask )
#define PING_CLEAR_EVENT( event, mask ) do { event &= ~mask; } while(0)

EventGroupHandle_t THIoT_ESPPing::_ev_handle = NULL;
bool THIoT_ESPPing::_result = false;
float THIoT_ESPPing::_avg_time = 0;
byte THIoT_ESPPing::_count = 0;

THIoT_ESPPing::THIoT_ESPPing(/* args */)
{
    _ev_handle = xEventGroupCreate();
}

THIoT_ESPPing::~THIoT_ESPPing()
{
}

bool THIoT_ESPPing::ping(IPAddress dest, byte count) {
    uint32_t ip_pinG = dest;
    uint32_t ping_timeout = 500;    /**< receive timeout in milliseconds */
    uint32_t ping_delay = 1000;     /**< delay time in milliseconds between pings */
    uint32_t ping_count = count;
    _result = false;
    _avg_time = 0;
    _count = count;
    ping_deinit();
    esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ping_count, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ping_timeout, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_DELAY_TIME, &ping_delay, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_IP_ADDRESS, &ip_pinG, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_RES_FN, (void*)&pingResults, sizeof(uint32_t));

    (void)xEventGroupClearBits(_ev_handle, PING_EV_ALL_EVENT);
    (void)xEventGroupSetBits(_ev_handle, PING_EV_TRANS_START);
    int ret = ping_init();
    return (ERR_OK == ret);
}

bool THIoT_ESPPing::ping(const char *host, byte count) {
    IPAddress remote_addr;
    if (WiFi.hostByName(host, remote_addr)) {
        return ping(remote_addr, count);
    }

    return false;
}

float THIoT_ESPPing::averageTime() {
    return _avg_time;
}

bool THIoT_ESPPing::result() { 
    bool status = false;
    pingEventEnum xReceivedEvent;

    EventBits_t uxBits = xEventGroupWaitBits(_ev_handle,            // The event group being tested.
                                             PING_EVENT_REQ_MASK,   // The bits within the event group to wait for.
                                             pdTRUE,                // Masked bits should be cleared before returning.
                                             pdFALSE,               // Don't wait for both bits, either bit will do.
                                             portMAX_DELAY);        // Wait forever for either bit to be set.
    xReceivedEvent = (pingEventEnum)(uxBits);
    if (xReceivedEvent) {
        PING_TAG_CONSOLE("%s: returned event = 0x%x", __func__, (int)xReceivedEvent);
        if (!(xReceivedEvent & PING_EVENT_REQ_MASK)) {
            // if we wait for certain event bits but get from poll subset
            PING_TAG_CONSOLE("%s: incorrect event set = 0x%x", __func__, (int)xReceivedEvent);
        }
        if (PING_CHECK_EVENT(xReceivedEvent, PING_EV_PROCESS_SUCCESS)) {
            status = true;
        } 
    } else {
        PING_TAG_CONSOLE("%s: Incorrect event or timeout xReceivedEvent = 0x%x", __func__, (int)uxBits);
        // https://github.com/espressif/esp-idf/issues/5275
        // if a no event is received, that means vMBMasterPortEventClose()
        // has been closed, so event group has been deleted by FreeRTOS, which
        // triggers the send of 0 value to the event group to unlock this task
        // waiting on it. For this patch, handles it as a time out without assert.
        status = false;
    }

    /**
     * _result is updated from the pingResults callback function.
    */
    return (_result && status); 
}

void THIoT_ESPPing::pingResults(ping_target_id_t msgType, struct _ping_found * pf) {
    if (_count != pf->send_count) {
        return ;
    }
    _count = 0;
    _result = (pf->total_time && (pf->send_count / 2) < pf->recv_count) ? 1 : 0;
    _avg_time = (float)pf->total_time / pf->recv_count;
    PING_TAG_CONSOLE("Result:%u AvgTime:%.1fmS Sent:%d Rec:%d min(mS):%d max(mS):%d Resp(mS):%d Timeouts:%d Total Time:%d",
                     _result,
                     _avg_time,
                     pf->send_count,
                     pf->recv_count,
                     pf->min_time,
                     pf->max_time,
                     pf->resp_time,
                     pf->timeout_count,
                     pf->total_time);
    (void)xEventGroupClearBits(_ev_handle, PING_EV_ALL_EVENT);
    (void)xEventGroupSetBits(_ev_handle, PING_EV_PROCESS_SUCCESS);
}
#endif // #if (PING_SERVICE_SYSTEM == 1)
