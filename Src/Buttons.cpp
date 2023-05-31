#define BUTTONS_IMPL
#include "Buttons.h"
#include <assert.h>
#include "log.h"
#include <FreeRTOS.h>
#include <timers.h>

static const int32_t kMinDebounceTime = 15;
static const int32_t kLongPressTime = 1000;
static const int32_t kHoldTime = 4000;

// Make sure that the button input defs and enums line up.
static_assert((sizeof(buttonInputDefs) / sizeof(buttonInputDefs[0])) == ((size_t)kButtonCount));

static void callButtonHandler(void* pFn, uint32_t unused) {
  HandlerFn fn = (HandlerFn) pFn;
  fn();
}

struct Button {
  // Initialize the button as "pressed" and "hold handler was fired".

  // If the button is held during power-up, this will prevent it from
  // firing any additional events until it's released.

  // If the button is not held during power-up, it will immediately
  // be released, but the high time will be too short to send any events.

  int32_t m_highTime = 0;
  int32_t m_lowTime = 0;
  bool m_previousState = true;
  bool m_didFireHoldHandler = true;

  HandlerFn m_shortPressHandler = nullptr;
  HandlerFn m_longPressHandler = nullptr;
  HandlerFn m_holdHandler = nullptr;

  void fireShortPressHandler() {
    // logprintf("Button(%p) short-press\n", this);
    if (m_shortPressHandler != nullptr) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xTimerPendFunctionCallFromISR(callButtonHandler, (void*) m_shortPressHandler, 0, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }

  void fireLongPressHandler() {
    // logprintf("Button(%p) long-press\n", this);
    if (m_longPressHandler != nullptr) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xTimerPendFunctionCallFromISR(callButtonHandler, (void*) m_longPressHandler, 0, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }

  void fireHoldHandler() {
    // logprintf("Button(%p) hold\n", this);
    if (m_holdHandler != nullptr) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xTimerPendFunctionCallFromISR(callButtonHandler, (void*) m_holdHandler, 0, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
};

Button buttonState[kButtonCount];

void Button_SetShortPressHandler(ButtonId btn, HandlerFn handler) {
  assert(btn < kButtonCount);
  buttonState[btn].m_shortPressHandler = handler;
}
void Button_SetLongPressHandler(ButtonId btn, HandlerFn handler) {
  assert(btn < kButtonCount);
  buttonState[btn].m_longPressHandler = handler;
}
void Button_SetHoldHandler(ButtonId btn, HandlerFn handler) {
  assert(btn < kButtonCount);
  buttonState[btn].m_holdHandler = handler;
}

void Button_TickHook() {
  for (size_t btnIdx = 0; btnIdx < kButtonCount; ++btnIdx) {
    auto& state = buttonState[btnIdx];
    bool pressed = LL_GPIO_IsInputPinSet(buttonInputDefs[btnIdx].gpioPort, buttonInputDefs[btnIdx].gpioPin) == buttonInputDefs[btnIdx].isActiveHigh;

    if (pressed)
      state.m_highTime = __QADD(state.m_highTime, 1);
    else
      state.m_lowTime = __QADD(state.m_lowTime, 1);



    if (state.m_previousState == true && !pressed) {
      // was previously pressed
      if (state.m_lowTime > kMinDebounceTime) {
        // has been low long enough to count as a release
        state.m_previousState = false;
        if (state.m_didFireHoldHandler) {
          // Already fired a hold handler for this, so don't fire a short or long press on key-up.
        } else if (state.m_highTime > kLongPressTime) {
          state.fireLongPressHandler();
        } else {
          state.fireShortPressHandler();
        }
        state.m_didFireHoldHandler = false;
        state.m_highTime = 0;
      }
    } else if (pressed) {
      if (state.m_previousState == false && state.m_highTime > kMinDebounceTime) {
        state.m_lowTime = 0;
        state.m_previousState = true;
      }

      if (state.m_highTime > kHoldTime && (state.m_didFireHoldHandler == false)) {
        // fire long-hold handler
        state.fireHoldHandler();
        state.m_didFireHoldHandler = true;
      }
    }


  }
}