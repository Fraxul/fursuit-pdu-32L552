#include "main.h"
#include "Shell.h"
#include "log.h"
#include "PM_SMBUS.h"
#include "PowerManagement.h"
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO

#include <FreeRTOS.h>
#include <task.h>
#include <stream_buffer.h>
#include <string.h>

extern "C" StreamBufferHandle_t usbRxStream;
extern "C" StreamBufferHandle_t usbTxStream;

static uint8_t scratch[ushFileScratchLen];
uint8_t* ushFileScratchBuf() { return scratch; };

bool useNonBlockingReads = true;

// non-blocking read interface
static int ush_read(struct ush_object* self, char* ch) {
  return xStreamBufferReceive(usbRxStream, ch, 1, /*ticksToWait=*/ useNonBlockingReads ? 0 : portMAX_DELAY);
}

// non-blocking write interface
static int ush_write(struct ush_object* self, char ch) {
  return xStreamBufferSend(usbTxStream, &ch, 1, /*ticksToWait=*/0);
}

// I/O interface descriptor
static const struct ush_io_interface ush_iface = {
    .read = ush_read,
    .write = ush_write,
};

// working buffers allocations (size could be customized)
#define BUF_IN_SIZE 128
#define BUF_OUT_SIZE 128
#define PATH_MAX_SIZE 128

static char ush_in_buf[BUF_IN_SIZE];
static char ush_out_buf[BUF_OUT_SIZE];


// microshell descriptor
static const struct ush_descriptor ush_desc = {
  .io = &ush_iface,                          // I/O interface pointer
  .input_buffer = ush_in_buf,                // working input buffer
  .input_buffer_size = sizeof(ush_in_buf),   // working input buffer size
  .output_buffer = ush_out_buf,              // working output buffer
  .output_buffer_size = sizeof(ush_out_buf), // working output buffer size
  .path_max_length = PATH_MAX_SIZE,          // path maximum length (stack)
  .hostname = const_cast<char*>("stm32"),    // hostname (in prompt)
};


const char* nibbleTable[] = {
  "0000",
  "0001",
  "0010",
  "0011",
  "0100",
  "0101",
  "0110",
  "0111",
  "1000",
  "1001",
  "1010",
  "1011",
  "1100",
  "1101",
  "1110",
  "1111"
};

// reboot cmd file execute callback
static void reboot_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  NVIC_SystemReset();
}

static void ps_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  UBaseType_t taskCount = uxTaskGetNumberOfTasks();
  TaskStatus_t* pxTaskStatusArray = (TaskStatus_t*) pvPortMalloc(taskCount * sizeof(TaskStatus_t));
  if (pxTaskStatusArray == nullptr) {
    printf("malloc() failure allocating TaskStatus_t array (%lu bytes for %lu tasks)\n", taskCount * sizeof(TaskStatus_t), taskCount);
    return;
  }

  uint32_t totalRunTime = 0;
  taskCount = uxTaskGetSystemState(pxTaskStatusArray, taskCount, &totalRunTime);

  // Header
  printf("PID %*s\tStat\tPri\tStk\tRun Time (x100uS)\n", (int) configMAX_TASK_NAME_LEN, "Task Name");
  for (size_t x = 0; x < taskCount; ++x) {
    const char* pcStatus = 0;
    switch (pxTaskStatusArray[x].eCurrentState) {
      case eRunning:
        pcStatus = "Run";
        break;

      case eReady:
        pcStatus = "Rdy";
        break;

      case eBlocked:
        pcStatus = "Blk";
        break;

      case eSuspended:
        pcStatus = "Sus";
        break;

      case eDeleted:
        pcStatus = "Del";
        break;

      case eInvalid: // fallthrough
      default:
        break;
    }


    printf("%3lu %*s\t %s\t %lu\t %lu\t %lu\n",
      (UBaseType_t)pxTaskStatusArray[x].xTaskNumber,
      (int)configMAX_TASK_NAME_LEN, pxTaskStatusArray[x].pcTaskName,
      pcStatus,
      (UBaseType_t) pxTaskStatusArray[x].uxCurrentPriority,
      (UBaseType_t) pxTaskStatusArray[x].usStackHighWaterMark,
      (UBaseType_t)pxTaskStatusArray[x].ulRunTimeCounter);
  }

  vPortFree(pxTaskStatusArray);
}

static void dmesg_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  const char *chunk1 = nullptr, *chunk2 = nullptr;
  size_t l1 = 0, l2 = 0;
  log_read(&chunk1, &l1, &chunk2, &l2);

  while (l1) {
    size_t sent = xStreamBufferSend(usbTxStream, chunk1, (l1 > 64 ? 64 : l1), 0);
    if (!sent) {
      vTaskDelay(1);
    }
    l1 -= sent;
    chunk1 += sent;
  }

  while (l2) {
    size_t sent = xStreamBufferSend(usbTxStream, chunk2, (l2 > 64 ? 64 : l2), 0);
    if (!sent) {
      vTaskDelay(1);
    }
    l2 -= sent;
    chunk2 += sent;
  }
}


static void log_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  if (argc == 1) {
    logprintf("--- Mark ---\n");
    return;
  }
  for (int i = 1; i < argc; ++i) {
    logprintf("%s\n", argv[i]);
  }
}


// set file execute callback
static void set_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  // arguments count validation
  if (argc != 2) {
    // return predefined error message
    ush_print_status(self, USH_STATUS_ERROR_COMMAND_WRONG_ARGUMENTS);
    return;
  }

  // arguments validation
  if (strcmp(argv[1], "1") == 0) {
    // turn led on
    LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
  } else if (strcmp(argv[1], "0") == 0) {
    // turn led off
    LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
  } else {
    // return predefined error message
    ush_print_status(self, USH_STATUS_ERROR_COMMAND_WRONG_ARGUMENTS);
    return;
  }
}

static SMBUS_HandleTypeDef* smbus_context_from_bus_id(uint8_t bus) {
  switch (bus) {
#if 0
    case 1:
      return &hsmbus1;
#endif
    case 3:
      return &hsmbus3;
    default:
      return nullptr;
  }
}




static void smbus_read_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  if (argc < 3) {
    // return predefined error message
    printf("usage: %s bus deviceAddress registerAddress\n", argv[0]);
    return;
  }

  uint8_t bus = strtol(argv[1], nullptr, 0);
  uint8_t deviceAddress = strtol(argv[2], nullptr, 0);
  uint8_t regAddress = strtol(argv[3], nullptr, 0);

  SMBUS_HandleTypeDef* pCtx = smbus_context_from_bus_id(bus);
  if (!pCtx) {
    printf("invalid bus ID\n");
    return;
  }

  printf("bus = %u, deviceAddress = 0x%x, regAddress = 0x%x\n", bus, deviceAddress, regAddress);
  uint16_t outValue = 0;
  if (!PM_SMBUS_ReadReg16(pCtx, deviceAddress, regAddress, outValue)) {
    printf("PM_SMBUS_ReadReg16() failed\n");
    return;
  }

  printf("%04x hex || %d dec || %s %s %s %s\n", outValue, outValue,
    nibbleTable[(outValue & 0xf000) >> 12],
    nibbleTable[(outValue & 0xf00) >> 8],
    nibbleTable[(outValue & 0xf0) >> 4],
    nibbleTable[(outValue & 0xf)]);
}


static void smbus_write16_exec_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
  if (argc < 4) {
    printf("usage: %s bus deviceAddress registerAddress value\n", argv[0]);
    return;
  }

  uint8_t bus = strtol(argv[1], nullptr, 0);
  uint8_t deviceAddress = strtol(argv[2], nullptr, 0);
  uint8_t regAddress = strtol(argv[3], nullptr, 0);
  uint16_t value = strtol(argv[4], nullptr, 0);

  SMBUS_HandleTypeDef* pCtx = smbus_context_from_bus_id(bus);
  if (!pCtx) {
    printf("invalid bus ID\n");
    return;
  }

  printf("bus = %u, deviceAddress = 0x%x, regAddress = 0x%x, value = 0x%x\n", bus, deviceAddress, regAddress, value);
  if (!PM_SMBUS_WriteReg16(pCtx, deviceAddress, regAddress, value)) {
    printf("PM_SMBUS_WriteReg16() failed\n");
    return;
  }
}

// led file get data callback
size_t led_get_data_callback(struct ush_object* self, struct ush_file_descriptor const* file, uint8_t** data) {
  // read current led state
  bool state = LL_GPIO_IsOutputPinSet(LED_RED_GPIO_Port, LED_RED_Pin);
  // return pointer to data
  *data = (uint8_t*)((state) ? "1\r\n" : "0\r\n");
  // return data size
  return strlen((char*)(*data));
}

// led file set data callback
void led_set_data_callback(struct ush_object* self, struct ush_file_descriptor const* file, uint8_t* data, size_t size) {
  // data size validation
  if (size < 1)
    return;

  // arguments validation
  if (data[0] == '1') {
    // turn led on
    LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
  } else if (data[0] == '0') {
    // turn led off
    LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
  }
}

// root directory files descriptor
static const struct ush_file_descriptor root_files[] = {
  #if 0
    {
        .name = "info.txt", // info.txt file name
        .description = NULL,
        .help = NULL,
        .exec = NULL,
        .get_data = info_get_data_callback,
    }
    #endif
};

// bin directory files descriptor
static const struct ush_file_descriptor bin_files[] = {
  /*
  {
    .name = "toggle",             // toogle file name
    .description = "toggle led",  // optional file description
    .help = "usage: toggle\r\n",  // optional help manual
    .exec = toggle_exec_callback, // optional execute callback
  },*/
  {
    .name = "set", // set file name
    .description = "set led",
    .help = "usage: set {0,1}\r\n",
    .exec = set_exec_callback
  },
};



// dev directory files descriptor
static const struct ush_file_descriptor dev_files[] = {
    {
        .name = "led",
        .description = NULL,
        .help = NULL,
        .exec = NULL,
        .get_data = led_get_data_callback, // optional data getter callback
        .set_data = led_set_data_callback, // optional data setter callback
    },
    File_RO(ticks, "%lu\n", HAL_GetTick()),
#if 0
    File_RO(adc_5v_sense, "%lu mv\n", ADC_read_5V_Sense()),
    File_RO(adc_disp5v_sense, "%lu mv\n", ADC_read_Disp5V_Sense()),
    File_RO(adc_vbat, "%lu mv\n", ADC_read_VBAT()),
    File_RO(adc_vrefint, "%lu mv\n", ADC_read_VRefInt()),
    File_RO(disp5v_charge_time, "%lu ms\n", Disp5V_charge_time_ms),
    File_RO(i2c, "ina260_online=%u emc2302_online=%u\n", ina260_online, emc2302_online),
    File_RO(ina260, "V=%lu mV A=%ld mA P=%lu mW Mask=%x\n", ina260_voltage, ina260_current, ina260_power, ina260_mask),
    File_RO(fan0, "%u rpm, spinupFailure = %u driveFailure = %u stalled = %u\n", fan[0].tachometer, fan[0].spinupFailure, fan[0].driveFailure, fan[0].stalled),
    File_RO(fan1, "%u rpm, spinupFailure = %u driveFailure = %u stalled = %u\n", fan[1].tachometer, fan[1].spinupFailure, fan[1].driveFailure, fan[1].stalled),
    File_RO(framecounter, "%lu\n", getFrameCounter()),
    File_RO(power_accum, "%lu mW-hr\n", static_cast<uint32_t>(power_accum >> 8) / (14062UL)),
#endif
};

// cmd files descriptor
static const struct ush_file_descriptor cmd_files[] = {
    { .name = "reboot",      .description = "reboot device",               .help = NULL, .exec = reboot_exec_callback, },
    { .name = "ps",          .description = "list tasks",                  .help = NULL, .exec = ps_exec_callback, },
    { .name = "dmesg",       .description = "show logs",                   .help = NULL, .exec = dmesg_exec_callback, },
    { .name = "log",         .description = "write data to dmesg log",     .help = NULL, .exec = log_exec_callback, },
    { .name = "pwr",         .description = "Power status",                .help = NULL, .exec = [](struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) { PM_Shell_DumpPowerStats(); }, },
    { .name = "smbus_read",  .description = "SMBUS Read",                  .help = NULL, .exec = smbus_read_exec_callback, },
    { .name = "smbus_write", .description = "SMBUS Write",                 .help = NULL, .exec = smbus_write16_exec_callback, },
};

// root directory handler
static struct ush_node_object root;
// dev directory handler
static struct ush_node_object dev;
// bin directory handler
static struct ush_node_object bin;

// cmd commands handler
static struct ush_node_object cmd;

ush_object* ushInstance() {
  static bool initialized = false;
  // microshell instance handler
  static struct ush_object ush;

  if (initialized)
    return &ush;

  initialized = true;

  // initialize microshell instance
  ush_init(&ush, &ush_desc);

  // add custom commands
  ush_commands_add(&ush, &cmd, cmd_files, sizeof(cmd_files) / sizeof(cmd_files[0]));

  // mount root directory (root must be first)
  ush_node_mount(&ush, "/", &root, root_files, sizeof(root_files) / sizeof(root_files[0]));
  // mount dev directory
  ush_node_mount(&ush, "/dev", &dev, dev_files, sizeof(dev_files) / sizeof(dev_files[0]));
  // mount bin directory
  ush_node_mount(&ush, "/bin", &bin, bin_files, sizeof(bin_files) / sizeof(bin_files[0]));

  return &ush;
}

extern "C" __attribute__((noreturn)) void Task_Shell() {

  auto inst = ushInstance();
  while (true) {
    // ush_service will return false when nothing's happening, so we swap over to a blocking
    // read in that case -- the task can sleep and wait for USB traffic.
    useNonBlockingReads = ush_service(inst);
  }
}
