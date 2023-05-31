#pragma once
#include <stdint.h>
#include <stddef.h>
#include "main.h"

typedef void(*HandlerFn)();

#ifdef BUTTONS_IMPL
struct ButtonInput {
  GPIO_TypeDef* gpioPort;
  uint32_t gpioPin;
  uint8_t isActiveHigh;
};

// Indices need to line up with ButtonId enum below.
static constexpr ButtonInput buttonInputDefs[] = {
  { Power_Button_GPIO_Port, Power_Button_Pin, /*isActiveHigh=*/ 0 },
};

#endif // BUTTONS_IMPL

enum ButtonId {
  kPowerButton,
  kButtonCount
};

#ifdef __cplusplus
extern "C" {
#endif

void Button_SetShortPressHandler(enum ButtonId btn, HandlerFn handler);
void Button_SetLongPressHandler(enum ButtonId btn, HandlerFn handler);
void Button_SetHoldHandler(enum ButtonId btn, HandlerFn handler);

void Button_TickHook();

#ifdef __cplusplus
}
#endif
