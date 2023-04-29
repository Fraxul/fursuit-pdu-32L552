import os

Import("env")

# borrowed from platforms\ststm32\builder\frameworks\stm32cube.py

platform = env.PioPlatform()
board = env.BoardConfig()

MCU = board.get("build.mcu", "")
MCU_FAMILY = MCU[0:7]

PRODUCT_LINE = board.get("build.product_line", "")
assert PRODUCT_LINE, "Missing MCU or Product Line field"

FRAMEWORK_DIR = platform.get_package_dir("framework-stm32cube%s" % MCU[5:7])

FreeRTOS_Base = os.path.join(FRAMEWORK_DIR, 'Middlewares', 'Third_Party', 'FreeRTOS', 'Source')
CMSIS_RTOS_Dir = os.path.join(FreeRTOS_Base, 'CMSIS_RTOS_V2') # TODO autodetect CMSIS_RTOS or CMSIS_RTOS_V2 from the ioc file
MCU_Port_Dir = os.path.join(FreeRTOS_Base, 'portable', 'GCC', 'ARM_CM33_NTZ/non_secure') # TODO autodetect correct core type for the MCU

# Heap_Type -- 0 = static only, 1-5  =heap1-heap5
# TODO: Should be read from the IOC file and FreeRTOSConfig.h
# FREERTOS.MEMORY_ALLOCATION=2 # 0 = Dynamic (default), 1 = Static, 2=Dynamic+Static
# (also sets configSUPPORT_DYNAMIC_ALLOCATION and configSUPPORT_STATIC_ALLOCATION in Inc/FreeRTOSConfig.h)
# FREERTOS.HEAP_NUMBER=4 # 1-5 for heap1-heap5, default is 4
Heap_type = 4

Heap_filter = [f"+<portable/MemMang/heap_{Heap_type}.c>"] if Heap_type > 0 else []

FreeRTOS_Includes = [
  os.path.join(FreeRTOS_Base, 'include'),
  CMSIS_RTOS_Dir,
  MCU_Port_Dir
]

env.Append(
  CPATH=FreeRTOS_Includes,
  CPPPATH=FreeRTOS_Includes
)

FreeRTOS_Lib = env.BuildSources(
  os.path.join("$BUILD_DIR", "FreeRTOS"),
  FreeRTOS_Base,
  src_filter = [
    "+<*.c>",
    f"+<{CMSIS_RTOS_Dir}/*.c>",
    #"+<portable/Common/*.c>", # only mpu_wrappers, not used
    f"+<{MCU_Port_Dir}/*.c>"
  ] + Heap_filter
)

#env.Dump()
#print(FreeRTOS_Base)
#print(FreeRTOS_Includes)
#env.PrintConfiguration()
#print(env.get('PIOFRAMEWORK'))
#print(env.PioPlatform().frameworks)
#print(env.PioPlatform().get_dir())
#print(env.PioPlatform().get_build_script())
#print(env.BoardConfig())

