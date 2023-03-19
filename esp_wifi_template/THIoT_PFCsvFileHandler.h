#ifndef __LOG_TRANSACTION_CARD_H
#define __LOG_TRANSACTION_CARD_H

#include <Arduino.h>
#include <functional>
#include <stdint.h>
#include "THIoT_PFFSService.h"
#include "THIoT_ESPTimeSystem.h"

#define LICENSE_PLATES_LENGTH_MAX        10	/* 76C1-26415 */

typedef struct
{
	uint32_t cmd;
	uint32_t pump_id;
	uint32_t Trans_id;
	uint32_t rule;
	uint32_t IDtag;
	uint32_t time;
	uint32_t date;
	uint32_t cash_total;
	uint32_t volume_total;
	uint32_t topup_total;
	int32_t  balance_total;
	struct
	{
		uint32_t Volume;
		uint32_t Cash;
		uint32_t Cost;
		uint32_t pump_cnt;
		uint32_t cltl;	
	} sell;
	char License_Plates[LICENSE_PLATES_LENGTH_MAX + 1];
} card_info_trans_t;

typedef struct {
    uint32_t date; // ddmmyy
    uint32_t time; // hhmmss
} total_time_t;

typedef struct {
    size_t start;
    size_t end;
} position_log_t;

typedef card_info_trans_t params_transaction_card_t;

class LOGTransactionCardClass : public FSULogStream
{
private:
    params_transaction_card_t _params;
    params_transaction_card_t _paramsUp;
    params_transaction_card_t _paramsDown;
    FSULogService _fsService;
    String _path;
    uint32_t _totalObject;
    uint32_t _firstObject;
    uint32_t _lastObject;
    uint32_t _queryCount;
    position_log_t _pos_params;
    position_log_t _pos_paramsUp;
    position_log_t _pos_paramsDown;
    void _Object();
public:
    LOGTransactionCardClass(const char * path);
    ~LOGTransactionCardClass();
    void begin(const char * path = nullptr);
    void end();
    position_log_t posParams() { return _pos_params;}
    position_log_t posParamsDown() { return _pos_paramsDown;}
    position_log_t posParamsUp() { return _pos_paramsUp;}
    uint32_t totalNumberObject();
    bool parser(const char* buff, params_transaction_card_t* params);
    String toString(params_transaction_card_t* params);
    bool firstParams(params_transaction_card_t* params);
    bool lastParams(params_transaction_card_t* params);
    bool nextParams(params_transaction_card_t* params);
    bool getParams(params_transaction_card_t* params);
    bool getUpParams(params_transaction_card_t* params);
    bool getDownParams(params_transaction_card_t* params);
    void addParams(params_transaction_card_t* params);
    void saveAsFile(const char* buff);
    void clear() {
        clean();
        _totalObject = 0;
    }
};

#endif // __LOG_TRANSACTION_CARD_H
