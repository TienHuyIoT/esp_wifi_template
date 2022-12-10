#ifndef __STATE_MACHINE_H
#define __STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SM_PRE_INPUT_RESET 0
#define SM_PRE_INPUT_SET 1

#define SM_HANDLE_DEFAULT            \
  {                                  \
    .pre_input = SM_PRE_INPUT_RESET, \
    .state = 0                       \
  }

typedef struct
{
  uint8_t pre_input;
  uint8_t state;
} sm_handle_t;

uint8_t sm_current(sm_handle_t *sm);
void sm_active(sm_handle_t *sm, uint8_t state);
uint8_t sm_once(sm_handle_t *sm);

#ifdef __cplusplus
}
#endif

#endif // __STATE_MACHINE_H
