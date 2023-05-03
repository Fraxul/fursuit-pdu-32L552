/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usbpd_dpm_user.c
  * @author  MCD Application Team
  * @brief   USBPD DPM user code
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

#define USBPD_DPM_USER_C
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usbpd_core.h"
#include "usbpd_dpm_user.h"
#include "usbpd_pdo_defs.h"
#include "usbpd_dpm_core.h"
#include "usbpd_dpm_conf.h"
#include "usbpd_pwr_if.h"
#include "usbpd_pwr_user.h"
#include "cmsis_os.h"
#if defined(_TRACE)
#include "usbpd_trace.h"
#include "string.h"
#include "stdio.h"
#endif /* _TRACE */
/* USER CODE BEGIN Includes */
#include "log.h"
#include <string.h>
#include "PowerManagement.h"
#include "FreeRTOS.h"
#include "timers.h"

/* USER CODE END Includes */

/** @addtogroup STM32_USBPD_APPLICATION
  * @{
  */

/** @addtogroup STM32_USBPD_APPLICATION_DPM_USER
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN Private_Typedef */

/* USER CODE END Private_Typedef */

/* Private define ------------------------------------------------------------*/
/** @defgroup USBPD_USER_PRIVATE_DEFINES USBPD USER Private Defines
  * @{
  */
#if (osCMSIS < 0x20000U)
void                USBPD_DPM_UserExecute(void const *argument);
#else
void                USBPD_DPM_UserExecute(void *argument);
#endif /* osCMSIS < 0x20000U */
/* USER CODE BEGIN Private_Define */

/* USER CODE END Private_Define */

/**
  * @}
  */

/* Private macro -------------------------------------------------------------*/
/** @defgroup USBPD_USER_PRIVATE_MACROS USBPD USER Private Macros
  * @{
  */
#if defined(_TRACE)
#define DPM_USER_DEBUG_TRACE_SIZE       50u
#define DPM_USER_DEBUG_TRACE(_PORT_, ...)  do {                                                                \
      char _str[DPM_USER_DEBUG_TRACE_SIZE];                                                                    \
      uint8_t _size = snprintf(_str, DPM_USER_DEBUG_TRACE_SIZE, __VA_ARGS__);                                  \
      if (_size < DPM_USER_DEBUG_TRACE_SIZE)                                                                   \
        USBPD_TRACE_Add(USBPD_TRACE_DEBUG, (uint8_t)(_PORT_), 0, (uint8_t*)_str, strlen(_str));                \
      else                                                                                                     \
        USBPD_TRACE_Add(USBPD_TRACE_DEBUG, (uint8_t)(_PORT_), 0, (uint8_t*)_str, DPM_USER_DEBUG_TRACE_SIZE);   \
  } while(0)

#define DPM_USER_ERROR_TRACE(_PORT_, _STATUS_, ...)  do {                                                      \
    if (USBPD_OK != _STATUS_) {                                                                                \
        char _str[DPM_USER_DEBUG_TRACE_SIZE];                                                                  \
        uint8_t _size = snprintf(_str, DPM_USER_DEBUG_TRACE_SIZE, __VA_ARGS__);                                \
        if (_size < DPM_USER_DEBUG_TRACE_SIZE)                                                                 \
          USBPD_TRACE_Add(USBPD_TRACE_DEBUG, (uint8_t)(_PORT_), 0, (uint8_t*)_str, strlen(_str));              \
        else                                                                                                   \
          USBPD_TRACE_Add(USBPD_TRACE_DEBUG, (uint8_t)(_PORT_), 0, (uint8_t*)_str, DPM_USER_DEBUG_TRACE_SIZE); \
    }                                                                                                          \
  } while(0)
#else
#define DPM_USER_DEBUG_TRACE(_PORT_, ...)
#define DPM_USER_ERROR_TRACE(_PORT_, _STATUS_, ...)
#endif /* _TRACE */
/* USER CODE BEGIN Private_Macro */

#if 0
#undef DPM_USER_DEBUG_TRACE
#undef DPM_USER_ERROR_TRACE

#define DPM_USER_DEBUG_TRACE(_PORT_, msg, ...) logprintf("DPM DEBUG: " msg "\n", ## __VA_ARGS__ )
#define DPM_USER_ERROR_TRACE(_PORT_, _STATUS_, msg, ...) \
  do { if (USBPD_OK != _STATUS_) { logprintf("DPM ERROR %x: " msg "\n", _STATUS_, ##__VA_ARGS__); } } while (0)
#endif
/* USER CODE END Private_Macro */
/**
  * @}
  */

/* Private variables ---------------------------------------------------------*/
/** @defgroup USBPD_USER_PRIVATE_VARIABLES USBPD USER Private Variables
  * @{
  */

/* USER CODE BEGIN Private_Variables */

#define kMaxSrcPDOs 8
USBPD_PDO_TypeDef srcPDOs[kMaxSrcPDOs];
xTimerHandle hardResetTimer;

/* USER CODE END Private_Variables */
/**
  * @}
  */

/* Private function prototypes -----------------------------------------------*/
/** @defgroup USBPD_USER_PRIVATE_FUNCTIONS USBPD USER Private Functions
  * @{
  */
/* USER CODE BEGIN USBPD_USER_PRIVATE_FUNCTIONS_Prototypes */

void HardResetTimerFn(TimerHandle_t hTimer) {
  if (srcPDOs[0].d32 == 0) {
    // No PDOs have been received yet, so issue a hard reset.
    DPM_USER_DEBUG_TRACE(0, "Hard-reset timer expired");
    USBPD_DPM_RequestHardReset(/*PortNum=*/ 0);
  }
}

/* USER CODE END USBPD_USER_PRIVATE_FUNCTIONS_Prototypes */
/**
  * @}
  */

/* Exported functions ------- ------------------------------------------------*/
/** @defgroup USBPD_USER_EXPORTED_FUNCTIONS USBPD USER Exported Functions
  * @{
  */
/* USER CODE BEGIN USBPD_USER_EXPORTED_FUNCTIONS */

/* USER CODE END USBPD_USER_EXPORTED_FUNCTIONS */

/** @defgroup USBPD_USER_EXPORTED_FUNCTIONS_GROUP1 USBPD USER Exported Functions called by DPM CORE
  * @{
  */
/* USER CODE BEGIN USBPD_USER_EXPORTED_FUNCTIONS_GROUP1 */

/* USER CODE END USBPD_USER_EXPORTED_FUNCTIONS_GROUP1 */

/**
  * @brief  Initialize DPM (port power role, PWR_IF, CAD and PE Init procedures)
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_UserInit(void)
{
/* USER CODE BEGIN USBPD_DPM_UserInit */
  hardResetTimer = xTimerCreate("CAD HardReset", pdMS_TO_TICKS(2000), /*repeating=*/ pdFALSE, 0, HardResetTimerFn);

  return USBPD_OK;
/* USER CODE END USBPD_DPM_UserInit */
}

/**
  * @brief  User delay implementation which is OS dependent
  * @param  Time time in ms
  * @retval None
  */
void USBPD_DPM_WaitForTime(uint32_t Time)
{
  osDelay(Time);
}

/**
  * @brief  User processing time, it is recommended to avoid blocking task for long time
  * @param  argument  DPM User event
  * @retval None
  */
#if (osCMSIS < 0x20000U)
void USBPD_DPM_UserExecute(void const *argument)
#else
void USBPD_DPM_UserExecute(void *argument)
#endif /* osCMSIS < 0x20000U */
{
/* USER CODE BEGIN USBPD_DPM_UserExecute */

/* USER CODE END USBPD_DPM_UserExecute */
}

/**
  * @brief  UserCableDetection reporting events on a specified port from CAD layer.
  * @param  PortNum The handle of the port
  * @param  State CAD state
  * @retval None
  */
void USBPD_DPM_UserCableDetection(uint8_t PortNum, USBPD_CAD_EVENT State)
{
/* USER CODE BEGIN USBPD_DPM_UserCableDetection */
switch (State) {
    case USBPD_CAD_EVENT_ATTACHED:
    case USBPD_CAD_EVENT_ATTEMC:
      DPM_USER_DEBUG_TRACE(PortNum, "USBPD_DPM_UserCableDetection: Attached (State = %d)", State);
      // Start a timer that will send a hard reset if we don't receive PDOs from the source shortly.
      // This handles the case where the microcontroller reboots while a Type-C cable is plugged in;
      // the power contract will still be active, but we don't know anything about it, so we can't
      // manage the charger effectively.
      xTimerReset(hardResetTimer, portMAX_DELAY);
      break;

    case USBPD_CAD_EVENT_DETACHED:
    case USBPD_CAD_EVENT_CABLE_DETACHED:
      DPM_USER_DEBUG_TRACE(PortNum, "USBPD_DPM_UserCableDetection: Detached (State = %d)", State);
      PM_DisconnectPower();
      break;

    default:
      DPM_USER_DEBUG_TRACE(PortNum, "ADVICE: update USBPD_DPM_UserCableDetection (State = %d)", State);
      break;
}


/* USER CODE END USBPD_DPM_UserCableDetection */
}

/**
  * @brief  function used to manage user timer.
  * @param  PortNum Port number
  * @retval None
  */
void USBPD_DPM_UserTimerCounter(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_DPM_UserTimerCounter */

/* USER CODE END USBPD_DPM_UserTimerCounter */
}

/**
  * @}
  */

/** @defgroup USBPD_USER_EXPORTED_FUNCTIONS_GROUP2 USBPD USER Exported Callbacks functions called by PE
  * @{
  */

/**
  * @brief  Callback function called by PE to inform DPM about PE event.
  * @param  PortNum The current port number
  * @param  EventVal @ref USBPD_NotifyEventValue_TypeDef
  * @retval None
  */
void USBPD_DPM_Notification(uint8_t PortNum, USBPD_NotifyEventValue_TypeDef EventVal)
{
/* USER CODE BEGIN USBPD_DPM_Notification */
  /* Manage event notified by the stack? */
  switch(EventVal)
  {
//    case USBPD_NOTIFY_POWER_EXPLICIT_CONTRACT :
//      break;
//    case USBPD_NOTIFY_REQUEST_ACCEPTED:
//      break;
//    case USBPD_NOTIFY_REQUEST_REJECTED:
//    case USBPD_NOTIFY_REQUEST_WAIT:
//      break;
//    case USBPD_NOTIFY_POWER_SWAP_TO_SNK_DONE:
//      break;
    case USBPD_NOTIFY_STATE_SNK_READY:
      DPM_USER_DEBUG_TRACE(PortNum, "Sink ready, notifying PM of power state change");
      inputPowerState.isReady = 1;
      PM_NotifyInputPowerStateUpdated();
      break;
//    case USBPD_NOTIFY_HARDRESET_RX:
//    case USBPD_NOTIFY_HARDRESET_TX:
//      break;
//    case USBPD_NOTIFY_STATE_SRC_DISABLED:
//      break;
//    case USBPD_NOTIFY_ALERT_RECEIVED :
//      break;
//    case USBPD_NOTIFY_CABLERESET_REQUESTED :
//      break;
//    case USBPD_NOTIFY_MSG_NOT_SUPPORTED :
//      break;
//    case USBPD_NOTIFY_PE_DISABLED :
//      break;
//    case USBPD_NOTIFY_USBSTACK_START:
//      break;
//    case USBPD_NOTIFY_USBSTACK_STOP:
//      break;
//    case USBPD_NOTIFY_DATAROLESWAP_DFP :
//      break;
//    case USBPD_NOTIFY_DATAROLESWAP_UFP :
//      break;
    default:
      DPM_USER_DEBUG_TRACE(PortNum, "ADVICE: USBPD_DPM_Notification:%d", EventVal);
      break;
  }
/* USER CODE END USBPD_DPM_Notification */
}

/**
  * @brief  Callback function called by PE layer when HardReset message received from PRL
  * @param  PortNum The current port number
  * @param  CurrentRole the current role
  * @param  Status status on hard reset event
  * @retval None
  */
void USBPD_DPM_HardReset(uint8_t PortNum, USBPD_PortPowerRole_TypeDef CurrentRole, USBPD_HR_Status_TypeDef Status)
{
/* USER CODE BEGIN USBPD_DPM_HardReset */
  DPM_USER_DEBUG_TRACE(PortNum, "Hard reset received, disconnecting power.");
  PM_DisconnectPower();
/* USER CODE END USBPD_DPM_HardReset */
}

/**
  * @brief  DPM callback to allow PE to retrieve information from DPM/PWR_IF.
  * @param  PortNum Port number
  * @param  DataId  Type of data to be updated in DPM based on @ref USBPD_CORE_DataInfoType_TypeDef
  * @param  Ptr     Pointer on address where DPM data should be written (u8 pointer)
  * @param  Size    Pointer on nb of u8 written by DPM
  * @retval None
  */
void USBPD_DPM_GetDataInfo(uint8_t PortNum, USBPD_CORE_DataInfoType_TypeDef DataId, uint8_t *Ptr, uint32_t *Size)
{
/* USER CODE BEGIN USBPD_DPM_GetDataInfo */
  /* Check type of information targeted by request */
  switch(DataId)
  {
//  case USBPD_CORE_DATATYPE_SNK_PDO:           /*!< Handling of port Sink PDO, requested by get sink capa*/
    // break;
//  case USBPD_CORE_EXTENDED_CAPA:              /*!< Source Extended capability message content          */
    // break;
//  case USBPD_CORE_DATATYPE_REQ_VOLTAGE:       /*!< Get voltage value requested for BIST tests, expect 5V*/
//    *Size = 4;
//    (void)memcpy((uint8_t *)Ptr, (uint8_t *)&DPM_Ports[PortNum].DPM_RequestedVoltage, *Size);
    // break;
//  case USBPD_CORE_INFO_STATUS:                /*!< Information status message content                  */
    // break;
//  case USBPD_CORE_MANUFACTURER_INFO:          /*!< Retrieve of Manufacturer info message content       */
    // break;
//  case USBPD_CORE_BATTERY_STATUS:             /*!< Retrieve of Battery status message content          */
    // break;
//  case USBPD_CORE_BATTERY_CAPABILITY:         /*!< Retrieve of Battery capability message content      */
    // break;
  default:
    DPM_USER_DEBUG_TRACE(PortNum, "ADVICE: update USBPD_DPM_GetDataInfo:%d", DataId);
    break;
  }
/* USER CODE END USBPD_DPM_GetDataInfo */
}

/**
  * @brief  DPM callback to allow PE to update information in DPM/PWR_IF.
  * @param  PortNum Port number
  * @param  DataId  Type of data to be updated in DPM based on @ref USBPD_CORE_DataInfoType_TypeDef
  * @param  Ptr     Pointer on the data
  * @param  Size    Nb of bytes to be updated in DPM
  * @retval None
  */
void USBPD_DPM_SetDataInfo(uint8_t PortNum, USBPD_CORE_DataInfoType_TypeDef DataId, uint8_t *Ptr, uint32_t Size)
{
/* USER CODE BEGIN USBPD_DPM_SetDataInfo */
  /* Check type of information targeted by request */
  switch(DataId)
  {
//  case USBPD_CORE_DATATYPE_RDO_POSITION:      /*!< Reset the PDO position selected by the sink only */
    // break;
  case USBPD_CORE_DATATYPE_RCV_SRC_PDO:       /*!< Storage of Received Source PDO values        */
    memset(srcPDOs, 0, sizeof(srcPDOs));
    memcpy(srcPDOs, Ptr, Size);

    DPM_USER_DEBUG_TRACE(PortNum, "Received %u PDOs from source", (Size / 4));

    // Kill the reset timer now that we have PDOs
    xTimerStop(hardResetTimer, portMAX_DELAY);

    break;
//  case USBPD_CORE_DATATYPE_RCV_SNK_PDO:       /*!< Storage of Received Sink PDO values          */
    // break;
//  case USBPD_CORE_EXTENDED_CAPA:              /*!< Source Extended capability message content   */
    // break;
//  case USBPD_CORE_PPS_STATUS:                 /*!< PPS Status message content                   */
    // break;
//  case USBPD_CORE_INFO_STATUS:                /*!< Information status message content           */
    // break;
//  case USBPD_CORE_ALERT:                      /*!< Storing of received Alert message content    */
    // break;
//  case USBPD_CORE_GET_MANUFACTURER_INFO:      /*!< Storing of received Get Manufacturer info message content */
    // break;
//  case USBPD_CORE_GET_BATTERY_STATUS:         /*!< Storing of received Get Battery status message content    */
    // break;
//  case USBPD_CORE_GET_BATTERY_CAPABILITY:     /*!< Storing of received Get Battery capability message content*/
    // break;
//  case USBPD_CORE_SNK_EXTENDED_CAPA:          /*!< Storing of Sink Extended capability message content       */
    // break;
  default:
    DPM_USER_DEBUG_TRACE(PortNum, "ADVICE: update USBPD_DPM_SetDataInfo:%d Size=%lu", DataId, Size);
    break;
  }
/* USER CODE END USBPD_DPM_SetDataInfo */

}

/**
  * @brief  Evaluate received Capabilities Message from Source port and prepare the request message
  * @param  PortNum         Port number
  * @param  PtrRequestData  Pointer on selected request data object
  * @param  PtrPowerObjectType  Pointer on the power data object
  * @retval None
  */
void USBPD_DPM_SNK_EvaluateCapabilities(uint8_t PortNum, uint32_t *PtrRequestData, USBPD_CORE_PDO_Type_TypeDef *PtrPowerObjectType)
{
/* USER CODE BEGIN USBPD_DPM_SNK_EvaluateCapabilities */

  uint8_t bestPDOIndex = 0;
  uint32_t bestPDOPower_uW = 0; // millivolts * milliamps = microwatts

  inputPowerState.isReady = 0;

  for (size_t i = 0; i < kMaxSrcPDOs; ++i) {
    if (srcPDOs[i].d32 == 0)
      break; // end of array is zero-filled.

    USBPD_PDO_TypeDef pdo = srcPDOs[i];
    uint32_t power_uW = 0;

    switch (pdo.GenericPDO.PowerObject) {
      case USBPD_CORE_PDO_TYPE_FIXED: {
        DPM_USER_DEBUG_TRACE(PortNum, "PDO %u: Fixed, %u mV, %u mA", i, pdo.SRCFixedPDO.VoltageIn50mVunits * 50, pdo.SRCFixedPDO.MaxCurrentIn10mAunits * 10);
        power_uW = pdo.SRCFixedPDO.VoltageIn50mVunits * 50 * pdo.SRCFixedPDO.MaxCurrentIn10mAunits * 10;
        if (power_uW > bestPDOPower_uW) {
          bestPDOPower_uW = power_uW;
          bestPDOIndex = i;

          inputPowerState.maxCurrent_mA = pdo.SRCFixedPDO.MaxCurrentIn10mAunits * 10;
          inputPowerState.minVoltage_mV = inputPowerState.maxVoltage_mV = pdo.SRCFixedPDO.VoltageIn50mVunits * 50;
          inputPowerState.maxPower_mW = power_uW / 1000;
        }
      } break;

      case USBPD_CORE_PDO_TYPE_BATTERY: {
        DPM_USER_DEBUG_TRACE(PortNum, "PDO %u: Battery, %u mV - %u mV, %u mW", i, pdo.SRCBatteryPDO.MinVoltageIn50mVunits * 50, pdo.SRCBatteryPDO.MaxVoltageIn50mVunits * 50, pdo.SRCBatteryPDO.MaxAllowablePowerIn250mWunits * 250);
        power_uW = pdo.SRCBatteryPDO.MaxAllowablePowerIn250mWunits * 250000;
        if (power_uW > bestPDOPower_uW) {
          bestPDOPower_uW = power_uW;
          bestPDOIndex = i;

          inputPowerState.minVoltage_mV = pdo.SRCBatteryPDO.MinVoltageIn50mVunits * 50;
          inputPowerState.maxVoltage_mV = pdo.SRCBatteryPDO.MaxVoltageIn50mVunits * 50;
          inputPowerState.maxPower_mW = pdo.SRCBatteryPDO.MaxAllowablePowerIn250mWunits * 250;

          // Max current should be capped at max power at minimum voltage
          inputPowerState.maxCurrent_mA = (inputPowerState.maxPower_mW * 1000) / inputPowerState.minVoltage_mV;
        }
      } break;

      case USBPD_CORE_PDO_TYPE_VARIABLE: {
        DPM_USER_DEBUG_TRACE(PortNum, "PDO %u: Variable, %u mV - %u mV, %u mA", i, pdo.SRCVariablePDO.MinVoltageIn50mVunits * 50, pdo.SRCVariablePDO.MaxVoltageIn50mVunits * 50, pdo.SRCVariablePDO.MaxCurrentIn10mAunits * 10);
        power_uW = pdo.SRCVariablePDO.MaxVoltageIn50mVunits * 50 * pdo.SRCVariablePDO.MaxCurrentIn10mAunits * 10;
        if (power_uW > bestPDOPower_uW) {
          bestPDOPower_uW = power_uW;
          bestPDOIndex = i;
          inputPowerState.minVoltage_mV = pdo.SRCVariablePDO.MinVoltageIn50mVunits * 50;
          inputPowerState.maxVoltage_mV = pdo.SRCVariablePDO.MaxVoltageIn50mVunits * 50;
          inputPowerState.maxCurrent_mA = pdo.SRCVariablePDO.MaxCurrentIn10mAunits * 10;
          inputPowerState.maxPower_mW = (inputPowerState.maxVoltage_mV * inputPowerState.maxCurrent_mA) / 1000;
        }
      } break;

      default:
      DPM_USER_DEBUG_TRACE(PortNum, "Unhandled extended PDO type %08lx", srcPDOs[i].d32);
    }

  }

  DPM_USER_DEBUG_TRACE(PortNum, "Best PDO is at index %u. Input power state: %lu - %lu mV, %lu mA, %lu mW",
    bestPDOIndex, inputPowerState.minVoltage_mV, inputPowerState.maxVoltage_mV, inputPowerState.maxCurrent_mA, inputPowerState.maxPower_mW);


  // Build request object based on the selected source PDO
  USBPD_SNKRDO_TypeDef rdo;
  rdo.d32 = 0;

  rdo.GenericRDO.ObjectPosition = bestPDOIndex + 1; // 1-based -- 0 is reserved.
  rdo.GenericRDO.CapabilityMismatch = 0;
  rdo.GenericRDO.USBCommunicationsCapable = 0;

  *PtrPowerObjectType = srcPDOs[bestPDOIndex].GenericPDO.PowerObject;

  if (srcPDOs[bestPDOIndex].GenericPDO.PowerObject == USBPD_CORE_PDO_TYPE_BATTERY) {
    rdo.BatteryRDO.MaxOperatingPowerIn250mWunits = srcPDOs[bestPDOIndex].SNKBatteryPDO.OperationalPowerIn250mWunits;
    rdo.BatteryRDO.OperatingPowerIn250mWunits = rdo.BatteryRDO.MaxOperatingPowerIn250mWunits;
  } else {
    rdo.FixedVariableRDO.MaxOperatingCurrent10mAunits = (*PtrPowerObjectType == USBPD_CORE_PDO_TYPE_FIXED ? srcPDOs[bestPDOIndex].SRCFixedPDO.MaxCurrentIn10mAunits : srcPDOs[bestPDOIndex].SRCVariablePDO.MaxCurrentIn10mAunits);
    rdo.FixedVariableRDO.OperatingCurrentIn10mAunits = rdo.FixedVariableRDO.MaxOperatingCurrent10mAunits;
  }

  *PtrRequestData = rdo.d32;

/* USER CODE END USBPD_DPM_SNK_EvaluateCapabilities */
}

/**
  * @brief  Callback to be used by PE to evaluate a Vconn swap
  * @param  PortNum Port number
  * @retval USBPD_ACCEPT, USBPD_REJECT, USBPD_WAIT
  */
USBPD_StatusTypeDef USBPD_DPM_EvaluateVconnSwap(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_DPM_EvaluateVconnSwap */
  USBPD_StatusTypeDef status = USBPD_REJECT;
  if (USBPD_TRUE == DPM_USER_Settings[PortNum].PE_VconnSwap)
  {
    status = USBPD_ACCEPT;
  }

  return status;
/* USER CODE END USBPD_DPM_EvaluateVconnSwap */
}

/**
  * @brief  Callback to be used by PE to manage VConn
  * @param  PortNum Port number
  * @param  State Enable or Disable VConn on CC lines
  * @retval USBPD_ACCEPT, USBPD_REJECT
  */
USBPD_StatusTypeDef USBPD_DPM_PE_VconnPwr(uint8_t PortNum, USBPD_FunctionalState State)
{
/* USER CODE BEGIN USBPD_DPM_PE_VconnPwr */
  return USBPD_ERROR;
/* USER CODE END USBPD_DPM_PE_VconnPwr */
}

/**
  * @brief  DPM callback to allow PE to forward extended message information.
  * @param  PortNum Port number
  * @param  MsgType Type of message to be handled in DPM
  *         This parameter can be one of the following values:
  *           @arg @ref USBPD_EXT_SECURITY_REQUEST Security Request extended message
  *           @arg @ref USBPD_EXT_SECURITY_RESPONSE Security Response extended message
  * @param  ptrData   Pointer on address Extended Message data could be read (u8 pointer)
  * @param  DataSize  Nb of u8 that compose Extended message
  * @retval None
  */
void USBPD_DPM_ExtendedMessageReceived(uint8_t PortNum, USBPD_ExtendedMsg_TypeDef MsgType, uint8_t *ptrData, uint16_t DataSize)
{
/* USER CODE BEGIN USBPD_DPM_ExtendedMessageReceived */

/* USER CODE END USBPD_DPM_ExtendedMessageReceived */
}

/**
  * @brief  DPM callback to allow PE to enter ERROR_RECOVERY state.
  * @param  PortNum Port number
  * @retval None
  */
void USBPD_DPM_EnterErrorRecovery(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_DPM_EnterErrorRecovery */
  /* Inform CAD to enter recovery mode */
  USBPD_CAD_EnterErrorRecovery(PortNum);
/* USER CODE END USBPD_DPM_EnterErrorRecovery */
}

/**
  * @brief  Callback used to ask application the reply status for a DataRoleSwap request
  * @note   if the callback is not set (ie NULL) the stack will automatically reject the request
  * @param  PortNum Port number
  * @retval Returned values are:
            @ref USBPD_ACCEPT if DRS can be accepted
            @ref USBPD_REJECT if DRS is not accepted in one data role (DFP or UFP) or in PD2.0 config
            @ref USBPD_NOTSUPPORTED if DRS is not supported at all by the application (in both data roles) - P3.0 only
  */
USBPD_StatusTypeDef USBPD_DPM_EvaluateDataRoleSwap(uint8_t PortNum)
{
/* USER CODE BEGIN USBPD_DPM_EvaluateDataRoleSwap */
  USBPD_StatusTypeDef status = USBPD_REJECT;
  {
    /* ACCEPT DRS if at least supported by 1 data role */
    if (((USBPD_TRUE == DPM_USER_Settings[PortNum].PE_DR_Swap_To_DFP) && (USBPD_PORTDATAROLE_UFP == DPM_Params[PortNum].PE_DataRole))
       || ((USBPD_TRUE == DPM_USER_Settings[PortNum].PE_DR_Swap_To_UFP) && (USBPD_PORTDATAROLE_DFP == DPM_Params[PortNum].PE_DataRole)))
    {
      status = USBPD_ACCEPT;
    }
  }
  return status;
/* USER CODE END USBPD_DPM_EvaluateDataRoleSwap */
}

/**
  * @brief  Callback to be used by PE to check is VBUS is ready or present
  * @param  PortNum Port number
  * @param  Vsafe   Vsafe status based on @ref USBPD_VSAFE_StatusTypeDef
  * @retval USBPD_DISABLE or USBPD_ENABLE
  */
USBPD_FunctionalState USBPD_DPM_IsPowerReady(uint8_t PortNum, USBPD_VSAFE_StatusTypeDef Vsafe)
{
/* USER CODE BEGIN USBPD_DPM_IsPowerReady */
  return ((USBPD_OK == USBPD_PWR_IF_SupplyReady(PortNum, Vsafe)) ? USBPD_ENABLE : USBPD_DISABLE);
/* USER CODE END USBPD_DPM_IsPowerReady */
}

/**
  * @}
  */

/** @defgroup USBPD_USER_EXPORTED_FUNCTIONS_GROUP3 USBPD USER Functions PD messages requests
  * @{
  */

/**
  * @brief  Request the PE to send a hard reset
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestHardReset(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_HardReset(PortNum);
  DPM_USER_ERROR_TRACE(PortNum, _status, "HARD RESET not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a cable reset.
  * @note   Only a DFP Shall generate Cable Reset Signaling. A DFP Shall only generate Cable Reset Signaling within an Explicit Contract.
            The DFP has to be supplying VCONN prior to a Cable Reset
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestCableReset(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CableReset(PortNum);
  DPM_USER_ERROR_TRACE(PortNum, _status, "CABLE RESET not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GOTOMIN message
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGotoMin(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GOTOMIN, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GOTOMIN not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a PING message
  * @note   In USB-PD stack, only ping management for P3.0 is implemented.
  *         If PD2.0 is used, PING timer needs to be implemented on user side.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestPing(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_PING, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "PING not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a request message.
  * @param  PortNum     The current port number
  * @param  IndexSrcPDO Index on the selected SRC PDO (value between 1 to 7)
  * @param  RequestedVoltage Requested voltage (in MV and use mainly for APDO)
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestMessageRequest(uint8_t PortNum, uint8_t IndexSrcPDO, uint16_t RequestedVoltage)
{
  USBPD_StatusTypeDef _status = USBPD_ERROR;
/* USER CODE BEGIN USBPD_DPM_RequestMessageRequest */
  /* To be adapted to call the PE function */
  /*       _status = USBPD_PE_Send_Request(PortNum, rdo.d32, pdo_object);*/
  DPM_USER_DEBUG_TRACE(PortNum, "ADVICE: update USBPD_DPM_RequestMessageRequest");
/* USER CODE END USBPD_DPM_RequestMessageRequest */
  DPM_USER_ERROR_TRACE(PortNum, _status, "REQUEST not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GET_SRC_CAPA message
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetSourceCapability(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_SRC_CAP, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_SRC_CAPA not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GET_SNK_CAPA message
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetSinkCapability(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_SNK_CAP, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_SINK_CAPA not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to perform a Data Role Swap.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestDataRoleSwap(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_DR_SWAP, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "DRS not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to perform a Power Role Swap.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestPowerRoleSwap(uint8_t PortNum)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "PRS not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to perform a VCONN Swap.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVconnSwap(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_VCONN_SWAP, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "VCS not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a soft reset
  * @param  PortNum The current port number
  * @param  SOPType SOP Type based on @ref USBPD_SOPType_TypeDef
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestSoftReset(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_SOFT_RESET, SOPType);
  DPM_USER_ERROR_TRACE(PortNum, _status, "SOFT_RESET not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a Source Capability message.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestSourceCapability(uint8_t PortNum)
{
  /* PE will directly get the PDO saved in structure @ref PWR_Port_PDO_Storage */
  USBPD_StatusTypeDef _status = USBPD_PE_Request_DataMessage(PortNum, USBPD_DATAMSG_SRC_CAPABILITIES, NULL);
  DPM_USER_ERROR_TRACE(PortNum, _status, "SRC_CAPA not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a VDM discovery identity
  * @param  PortNum The current port number
  * @param  SOPType SOP Type
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVDM_DiscoveryIdentify(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType)
{
  USBPD_StatusTypeDef _status = USBPD_ERROR;
/* USER CODE BEGIN USBPD_DPM_RequestVDM_DiscoveryIdentify */

/* USER CODE END USBPD_DPM_RequestVDM_DiscoveryIdentify */
  DPM_USER_ERROR_TRACE(PortNum, _status, "VDM Discovery Ident not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a VDM discovery SVID
  * @param  PortNum The current port number
  * @param  SOPType SOP Type
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVDM_DiscoverySVID(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "VDM discovery SVID not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to perform a VDM Discovery mode message on one SVID.
  * @param  PortNum The current port number
  * @param  SOPType SOP Type
  * @param  SVID    SVID used for discovery mode message
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVDM_DiscoveryMode(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, uint16_t SVID)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "VDM Discovery mode not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to perform a VDM mode enter.
  * @param  PortNum   The current port number
  * @param  SOPType   SOP Type
  * @param  SVID      SVID used for discovery mode message
  * @param  ModeIndex Index of the mode to be entered
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVDM_EnterMode(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, uint16_t SVID, uint8_t ModeIndex)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "VDM mode enter not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to perform a VDM mode exit.
  * @param  PortNum   The current port number
  * @param  SOPType   SOP Type
  * @param  SVID      SVID used for discovery mode message
  * @param  ModeIndex Index of the mode to be exit
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestVDM_ExitMode(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, uint16_t SVID, uint8_t ModeIndex)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "VDM mode exit not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to send an ALERT to port partner
  * @param  PortNum The current port number
  * @param  Alert   Alert based on @ref USBPD_ADO_TypeDef
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestAlert(uint8_t PortNum, USBPD_ADO_TypeDef Alert)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_DataMessage(PortNum, USBPD_DATAMSG_ALERT, (uint32_t*)&Alert.d32);
  DPM_USER_ERROR_TRACE(PortNum, _status, "ALERT not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to get a source capability extended
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetSourceCapabilityExt(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_SRC_CAPEXT, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_SRC_CAPA_EXT not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to get a sink capability extended
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetSinkCapabilityExt(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_SNK_CAPEXT, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_SINK_CAPA_EXT not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to get a manufacturer info
  * @param  PortNum The current port number
  * @param  SOPType SOP Type
  * @param  pManuInfoData Pointer on manufacturer info based on @ref USBPD_GMIDB_TypeDef
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetManufacturerInfo(uint8_t PortNum, USBPD_SOPType_TypeDef SOPType, uint8_t* pManuInfoData)
{
  USBPD_StatusTypeDef _status = USBPD_ERROR;
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_MANU_INFO not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to request a GET_PPS_STATUS
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetPPS_Status(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_PPS_STATUS, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_PPS_STATUS not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to request a GET_STATUS
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetStatus(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_STATUS, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_STATUS not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to perform a Fast Role Swap.
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestFastRoleSwap(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_FR_SWAP, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "FRS not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GET_COUNTRY_CODES message
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetCountryCodes(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_CtrlMessage(PortNum, USBPD_CONTROLMSG_GET_COUNTRY_CODES, USBPD_SOPTYPE_SOP);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_COUNTRY_CODES not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GET_COUNTRY_INFO message
  * @param  PortNum     The current port number
  * @param  CountryCode Country code (1st character and 2nd of the Alpha-2 Country)
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetCountryInfo(uint8_t PortNum, uint16_t CountryCode)
{
  USBPD_StatusTypeDef _status = USBPD_PE_Request_DataMessage(PortNum, USBPD_DATAMSG_GET_COUNTRY_INFO, (uint32_t*)&CountryCode);
  DPM_USER_ERROR_TRACE(PortNum, _status, "GET_COUNTRY_INFO not accepted by the stack");
  return _status;
}

/**
  * @brief  Request the PE to send a GET_BATTERY_CAPA
  * @param  PortNum         The current port number
  * @param  pBatteryCapRef  Pointer on the Battery Capability reference
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetBatteryCapability(uint8_t PortNum, uint8_t *pBatteryCapRef)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "GET_BATTERY_CAPA not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to send a GET_BATTERY_STATUS
  * @param  PortNum           The current port number
  * @param  pBatteryStatusRef Pointer on the Battery Status reference
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestGetBatteryStatus(uint8_t PortNum, uint8_t *pBatteryStatusRef)
{
  DPM_USER_ERROR_TRACE(PortNum, USBPD_ERROR, "GET_BATTERY_STATUS not accepted by the stack");
  return USBPD_ERROR;
}

/**
  * @brief  Request the PE to send a SECURITY_REQUEST
  * @param  PortNum The current port number
  * @retval USBPD Status
  */
USBPD_StatusTypeDef USBPD_DPM_RequestSecurityRequest(uint8_t PortNum)
{
  USBPD_StatusTypeDef _status = USBPD_ERROR;
  DPM_USER_ERROR_TRACE(PortNum, _status, "SECURITY_REQUEST not accepted by the stack");
  return _status;
}

/**
  * @}
  */

/** @addtogroup USBPD_USER_PRIVATE_FUNCTIONS
  * @{
  */

/* USER CODE BEGIN USBPD_USER_PRIVATE_FUNCTIONS */

/* USER CODE END USBPD_USER_PRIVATE_FUNCTIONS */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
