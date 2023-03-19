#include <Arduino.h>
#include <ArduinoJson.h>
#include "THIoT_ESPConfig.h"
#include "THIoT_PFCsvFileHandler.h"
#include "THIoT_PFFSService.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_PFSerialTrace.h"


#define TRANSACTION_CARD_TAG_CONSOLE(...) //SERIAL_TAG_LOGI("[TRANSACTION CARD]", __VA_ARGS__)

#define TRANSACTION_LOG_PRINTF(f_, ...)  printf_P(PSTR(f_), ##__VA_ARGS__)
#define TRANSACTION_LOG_PRINTFLF(f_, ...)  printf_P(PSTR(f_ "\r\n"), ##__VA_ARGS__)

#define TRANSACTION_CARD_LOG_FS NAND_FS_SYSTEM
/* 2,99,7C9E9E90,123456 ,20210805,111419,104000,8420,12345,141035,11420,500000,358965,11192\r\n */
#define TRANSACTION_CARD_LOG_LENGTH_MAX 128U
#define TRANSACTION_CARD_LOG_FIELD_NUM  14

const char transaction_card_log_header[] PROGMEM = "STT,Voi,Ma The,Bien So Xe,Ngay,Gio,Tien,Lit,Gia,Tong Tien,Tong Lit,Tien Nop,So Du, Ma Giao Dich";

static uint32_t ddmmyyConvert(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000;
    uint32_t mon = (yyyymmdd % 10000) / 100;
    uint32_t mday = yyyymmdd % 100;

    uint32_t d = mday * 10000 + mon * 100 + year % 100;
    return d;
}

static rtc_time_t rtcConvert(uint32_t ddmmyy, uint32_t hhmmss)
{
    rtc_time_t rtc;
    rtc.hour = hhmmss / 10000;
    rtc.min = (hhmmss % 10000) / 100;
    rtc.sec = hhmmss % 100;

    rtc.wday = 0;
    rtc.mday = ddmmyy / 10000;
    rtc.mon = (ddmmyy % 10000) / 100;
    rtc.year = ddmmyy % 100 + 2000;

    return rtc;
}

LOGTransactionCardClass::LOGTransactionCardClass(const char * path)
:FSULogStream(TRANSACTION_CARD_LOG_FS, path),
_fsService(TRANSACTION_CARD_LOG_FS, TRANSACTION_CARD_LOG_LENGTH_MAX * 2, path, sizeof(transaction_card_log_header) - 1),
_path(path)
{
    _totalObject = 0;
    _firstObject = 0;
    _lastObject = 0;
}

LOGTransactionCardClass::~LOGTransactionCardClass()
{
    end();
}

void LOGTransactionCardClass::_Object()
{
    if (_fsService.first())
    {
        _firstObject = _params.sell.pump_cnt;
    }

    if (_fsService.last())
    {
        _lastObject = _params.sell.pump_cnt;
    }

    _totalObject = _lastObject - _firstObject;

    TRANSACTION_CARD_TAG_CONSOLE("_firstObject %u", _firstObject);
    TRANSACTION_CARD_TAG_CONSOLE("_lastObject %u", _lastObject);
    TRANSACTION_CARD_TAG_CONSOLE("_totalObject %u", _totalObject);
}

void LOGTransactionCardClass::end() {}

void LOGTransactionCardClass::begin(const char * path)
{
    if (path != nullptr)
    {
        _path = path;
        _fsService.setPath(path);
    }

    _fsService.onParseLogWithArg([](void* arg, fsContent_t *cxt)
    {
        TRANSACTION_CARD_TAG_CONSOLE("onParseLogWithArg()");
        LOGTransactionCardClass* handler = (LOGTransactionCardClass*)(arg);
        bool status = handler->parser(cxt->buff, &handler->_params);
        if (status)
        {
            handler->_pos_params.start = cxt->pos;
            handler->_pos_params.end   = cxt->pos + cxt->len;
        }
        return status;
    }, this);

    /** 
     * @param [out] value the epoch time
     * */
    _fsService.onLookupLogWithArg([](void* arg, fsContent_t *cxt, int* value)
    {
        TRANSACTION_CARD_TAG_CONSOLE("onLookupLogWithArg()");
        
        LOGTransactionCardClass* handler = (LOGTransactionCardClass*)(arg);
        params_transaction_card_t param;
        if (handler->parser(cxt->buff, &param))
        {
            handler->_pos_params.start = cxt->pos;
            handler->_pos_params.end   = cxt->pos + cxt->len;
            rtc_time_t rtc = rtcConvert(ddmmyyConvert(param.date), param.time);
            int tNow = ESPTime.makeNowFromRtc(&rtc);
            *value = tNow;
            handler->_params = param;
            TRANSACTION_CARD_TAG_CONSOLE("onLookupLogWithArg() true");
            return true;
        }
        return false;
    }, this);

    /** 
     * @param [in] value the epoch time
     * */
    _fsService.onQueryLogWithArg([](void* arg, fsContent_t *cxt, int value)
    {
        TRANSACTION_CARD_TAG_CONSOLE("onQueryLogWithArg()");
        bool status = false;
        LOGTransactionCardClass* handler = (LOGTransactionCardClass*)(arg);
        params_transaction_card_t param;
        if (handler->parser(cxt->buff, &param))
        {
            handler->_pos_params.start = cxt->pos;
            handler->_pos_params.end   = cxt->pos + cxt->len;
            rtc_time_t rtc = rtcConvert(ddmmyyConvert(param.date), param.time);
            int tNow = ESPTime.makeNowFromRtc(&rtc);
            handler->_params = param;

            if (tNow <= value)
            {
                handler->_paramsDown = param;
                handler->_pos_paramsDown.start = cxt->pos;
                handler->_pos_paramsDown.end   = cxt->pos + cxt->len;
                if (tNow == value)
                {
                    handler->_paramsUp = param;
                    handler->_pos_paramsUp.start = cxt->pos;
                    handler->_pos_paramsUp.end   = cxt->pos + cxt->len;
                    status = true;
                }
            }
            else
            {
                handler->_paramsUp = param;
                handler->_pos_paramsUp.start = cxt->pos;
                handler->_pos_paramsUp.end   = cxt->pos + cxt->len;
                status = true;
            }

            handler->_queryCount++;
        }

        if (status)
        {
            TRANSACTION_CARD_TAG_CONSOLE("onQueryLogWithArg() true");
        }

        return status;
    }, this);

    _Object();
}

uint32_t LOGTransactionCardClass::totalNumberObject()
{
    return _totalObject;
}

bool LOGTransactionCardClass::firstParams(params_transaction_card_t* params)
{
    if (_fsService.first())
    {
        *params = _params;
        return true;
    }
    return false;
}

bool LOGTransactionCardClass::lastParams(params_transaction_card_t* params)
{
    if (_fsService.last())
    {
        *params = _params;
        return true;
    }
    return false;
}

bool LOGTransactionCardClass::nextParams(params_transaction_card_t* params)
{
    if (_fsService.next())
    {
        *params = _params;
        return true;
    }
    return false;
}

bool LOGTransactionCardClass::getParams(params_transaction_card_t* params)
{
    rtc_time_t rtc = rtcConvert(params->date, params->time);
    int tNow = ESPTime.makeNowFromRtc(&rtc);

    _queryCount = 0;
    _fsService.withValue(tNow);
    /* When the function withValue() called, _queryCount shall updated in a callback function */
    if (_queryCount > 0)
    {
        *params = _params;
        return true;
    }

    return false;
}

bool LOGTransactionCardClass::getUpParams(params_transaction_card_t* params)
{
    rtc_time_t rtc = rtcConvert(params->date, params->time);
    int tNow = ESPTime.makeNowFromRtc(&rtc);
    memset((uint8_t*)&_paramsUp, 0, sizeof(params_transaction_card_t));
    _queryCount = 0;
    _fsService.withValue(tNow);
    /* When the function withValue() called, _queryCount shall updated in a callback function */
    if (_paramsUp.Trans_id != 0)
    {
        *params = _paramsUp;
        return true;
    }

    return false;
}

bool LOGTransactionCardClass::getDownParams(params_transaction_card_t* params)
{
    rtc_time_t rtc = rtcConvert(params->date, params->time);
    int tNow = ESPTime.makeNowFromRtc(&rtc);
    memset((uint8_t*)&_paramsDown, 0, sizeof(params_transaction_card_t));
    _queryCount = 0;
    _fsService.withValue(tNow);
    /* When the function withValue() called, _queryCount shall updated in a callback function */
    if (_paramsDown.Trans_id != 0)
    {
        *params = _paramsDown;
        return true;
    }

    return false;
}

void LOGTransactionCardClass::saveAsFile(const char* buff)
{
    _totalObject = 0;
    _firstObject = 0;
    _lastObject = 0;
    clean(); // clean to make sure create a new file after that.
    TRANSACTION_LOG_PRINTFLF("%s", FPSTR(transaction_card_log_header));
    TRANSACTION_LOG_PRINTF("%s", buff); // buff must be has string "\r\n"

    _Object();
}

void LOGTransactionCardClass::addParams(params_transaction_card_t* params)
{
    if (0 == _totalObject)
    {
        clean(); // clean to make sure create a new file after that.
        TRANSACTION_CARD_TAG_CONSOLE("Write header");
        TRANSACTION_LOG_PRINTFLF("%s", FPSTR(transaction_card_log_header));
    }
    
    _totalObject++;
    _lastObject++;

    /* Number object always is pump_cnt */
    // params->sell.pump_cnt = _lastObject;

    TRANSACTION_LOG_PRINTFLF("%u,%u,%lX,%s ,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u",
                params->sell.pump_cnt,
                params->pump_id,
                params->IDtag,
                params->License_Plates,
                params->date,
                params->time,
                params->sell.Cash,
                params->sell.Volume,                
                params->sell.Cost,                
                params->cash_total,
                params->volume_total,
                params->topup_total,
                params->balance_total,
                params->Trans_id
                );
    TRANSACTION_CARD_TAG_CONSOLE("Write Number Object %u", _lastObject);
}

String LOGTransactionCardClass::toString(params_transaction_card_t* params)
{
    char* buff = new(std::nothrow) char[TRANSACTION_CARD_LOG_LENGTH_MAX];

    snprintf(buff, TRANSACTION_CARD_LOG_LENGTH_MAX, "%u,%u,%lX,%s ,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u",
                params->sell.pump_cnt,
                params->pump_id,
                params->IDtag,
                params->License_Plates,
                params->date,
                params->time,
                params->sell.Cash,
                params->sell.Volume,                
                params->sell.Cost,                
                params->cash_total,
                params->volume_total,
                params->topup_total,
                params->balance_total,
                params->Trans_id
                );
    String str = buff;
    delete[] buff;
    return str;
}

/**
 * @brief Parse params_transaction_card_t from buffer content
 * @param [in] buff Data buffer input need to find
 * @param [out] params the pointer params_transaction_card_t contect params is parsed from buffer
 * @return 1(OK), 0(error) 
 */
bool LOGTransactionCardClass::parser(const char* buff, params_transaction_card_t* params) {
    /* 1,1,dd/mm/yyyy,hh:MM:ss,gia ,24996,main/web */
    int field_n = sscanf(buff, "%u,%u,%lX,%s ,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u",
                &params->sell.pump_cnt,
                &params->pump_id,
                &params->IDtag,
                params->License_Plates,
                &params->date,
                &params->time,
                &params->sell.Cash,
                &params->sell.Volume,                
                &params->sell.Cost,                
                &params->cash_total,
                &params->volume_total,
                &params->topup_total,
                &params->balance_total,
                &params->Trans_id
                );
    TRANSACTION_CARD_TAG_CONSOLE("field_n %d", field_n);
    if (TRANSACTION_CARD_LOG_FIELD_NUM == field_n)
    {
        TRANSACTION_CARD_TAG_CONSOLE("Parse log OK: sell pump_cnt = %u", params->sell.pump_cnt);
        return true;
    }

    return false;
}
