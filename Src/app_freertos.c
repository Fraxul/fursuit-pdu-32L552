/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stream_buffer.h"
#include "stm32l5xx_ll_tim.h"
#include <stdio.h>
#include "Buttons.h"
#include "log.h"
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

StreamBufferHandle_t usbRxStream;
#define usbRxStreamSize 127
static StaticStreamBuffer_t staticUsbRxStream;
static uint8_t staticUsbRxStreamStorage[usbRxStreamSize + 1];

StreamBufferHandle_t usbTxStream;
#define usbTxStreamSize 127
static StaticStreamBuffer_t staticUsbTxStream;
static uint8_t staticUsbTxStreamStorage[usbTxStreamSize + 1];

#define DECLARE_TASK(TaskName, StackSizeInWords) \
  TaskHandle_t TaskName##Handle;                   \
  uint32_t TaskName##Stack[StackSizeInWords];    \
  StaticTask_t TaskName##ControlBlock;    \
  extern void Task_##TaskName(void *arg);  \
  static const uint32_t TaskName##_StackSize = StackSizeInWords;

#define START_TASK(TaskName, Priority) \
  TaskName##Handle = xTaskCreateStatic(Task_##TaskName, #TaskName, TaskName##_StackSize, /*arg=*/ 0, Priority, TaskName##Stack, &TaskName##ControlBlock);

DECLARE_TASK(USB_Tx, 128);
DECLARE_TASK(Shell, 256);
DECLARE_TASK(PowerManagement, 256);
DECLARE_TASK(Display, 256);
DECLARE_TASK(WS2812, 256);

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);
void vApplicationIdleHook(void);
void vApplicationTickHook(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
#define PERF_TIMER TIM7
void configureTimerForRunTimeStats(void)
{
  LL_TIM_DisableCounter(PERF_TIMER);
  LL_TIM_DisableARRPreload(PERF_TIMER);
  LL_TIM_SetAutoReload(PERF_TIMER, IS_TIM_32B_COUNTER_INSTANCE(PERF_TIMER) ? 0xffffffffUL : 0xffffUL);
  LL_TIM_SetCounter(PERF_TIMER, 0);
  LL_TIM_SetPrescaler(PERF_TIMER, (F_CPU / 10000UL));
  if (IS_TIM_COUNTER_MODE_SELECT_INSTANCE(PERF_TIMER)) {
    LL_TIM_SetCounterMode(PERF_TIMER, LL_TIM_COUNTERMODE_UP);
  }
  LL_TIM_EnableCounter(PERF_TIMER);
}

unsigned long getRunTimeCounterValue(void)
{
  return LL_TIM_GetCounter(PERF_TIMER);
}
/* USER CODE END 1 */

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */

   __WFI();
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */

  // Blink the LED to show that the system is still alive
  TickType_t ticks = xTaskGetTickCount();
  if (ticks & 0x200) {
    LL_GPIO_SetOutputPin(LED_GPIO_Port, LED_Pin);
  } else {
    LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);
  }

  // Run UI button debouncing and handlers
  Button_TickHook();
}
/* USER CODE END 3 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  /* Run time stack overflow checking is performed if
  configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
  called if a stack overflow is detected. */

  logprintf("FATAL: Stack overflow in task %s\n", pcTaskName);

  abort();
}

/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  usbRxStream = xStreamBufferCreateStatic(usbRxStreamSize, /*triggerLevel=*/1, staticUsbRxStreamStorage, &staticUsbRxStream);
  usbTxStream = xStreamBufferCreateStatic(usbTxStreamSize, /*triggerLevel=*/1, staticUsbTxStreamStorage, &staticUsbTxStream);
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
/* add queues, ... */

#if 0 // disable useless default task
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
#endif

  START_TASK(Shell, osPriorityNormal);
  START_TASK(USB_Tx, osPriorityHigh);
  START_TASK(PowerManagement, osPriorityNormal);
  START_TASK(Display, osPriorityNormal);
  START_TASK(WS2812, osPriorityNormal);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
/* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

