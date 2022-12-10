#include "THIoT_PFStateMachine.h"

uint8_t sm_current(sm_handle_t *sm)
{
  return sm->state;
}

void sm_active(sm_handle_t *sm, uint8_t state)
{
  sm->state = state;
  sm->pre_input = SM_PRE_INPUT_RESET;
}

uint8_t sm_once(sm_handle_t *sm)
{
  if (!sm->pre_input)
  {
    sm->pre_input = SM_PRE_INPUT_SET;
    return 1;
  }

  return 0;
}
