/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

#include "stm32l5xx_ll_adc.h"
#include "stm32l5xx_ll_tim.h"
#include "stm32l5xx_ll_ucpd.h"
#include "stm32l5xx_ll_usart.h"
#include "stm32l5xx_ll_rcc.h"
#include "stm32l5xx_ll_system.h"
#include "stm32l5xx_ll_gpio.h"
#include "stm32l5xx_ll_exti.h"
#include "stm32l5xx_ll_bus.h"
#include "stm32l5xx_ll_cortex.h"
#include "stm32l5xx_ll_utils.h"
#include "stm32l5xx_ll_pwr.h"
#include "stm32l5xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Power_Button_Pin LL_GPIO_PIN_13
#define Power_Button_GPIO_Port GPIOC
#define JetsonPwrEnable_Pin LL_GPIO_PIN_3
#define JetsonPwrEnable_GPIO_Port GPIOA
#define DispPwrEnable_Pin LL_GPIO_PIN_4
#define DispPwrEnable_GPIO_Port GPIOA
#define TC_VBUS_Sense_Pin LL_GPIO_PIN_5
#define TC_VBUS_Sense_GPIO_Port GPIOA
#define ADC_Disp5V_Sense_Pin LL_GPIO_PIN_6
#define ADC_Disp5V_Sense_GPIO_Port GPIOA
#define ADC_5V_Sense_Pin LL_GPIO_PIN_7
#define ADC_5V_Sense_GPIO_Port GPIOA
#define UCPD_DBN_Pin LL_GPIO_PIN_12
#define UCPD_DBN_GPIO_Port GPIOB
#define UCPD_FLT_Pin LL_GPIO_PIN_8
#define UCPD_FLT_GPIO_Port GPIOA
#define UCPD_DBNB5_Pin LL_GPIO_PIN_5
#define UCPD_DBNB5_GPIO_Port GPIOB
#define LED_Pin LL_GPIO_PIN_3
#define LED_GPIO_Port GPIOH

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
