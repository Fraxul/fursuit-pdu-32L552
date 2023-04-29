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

PD_Base = os.path.join(FRAMEWORK_DIR, 'Middlewares', 'ST', 'STM32_USBPD_Library')
Tracer_Base = os.path.join(FRAMEWORK_DIR, 'Utilities', 'TRACER_EMB')

PD_Core = os.path.join(PD_Base, 'Core')
PD_Core_src = os.path.join(PD_Core, 'src')
PD_Core_lib = os.path.join(PD_Core, 'lib')
PD_Device = os.path.join(PD_Base, 'Devices', 'STM32L5XX')
PD_Device_src = os.path.join(PD_Device, 'src')

Includes = [
  os.path.join(PD_Core, 'inc'),
  os.path.join(PD_Device, 'inc'),
  # Tracer_Base
]

#PD_Sources = [
#  os.path.join(PD_Core_src, 'usbpd_trace.c') +
#  Glob(os.path.join(PD_Device_src, '*.c'))
#]

env.Append(
  CPATH=Includes,
  CPPPATH=Includes,
  #LINKFLAGS=[f"{PD_Core_lib}/USBPDCORE_PD3_FULL_CM33_wc32.a"]

)

PD_Lib = env.BuildSources(
  os.path.join("$BUILD_DIR", "STM32_USBPD_Library"),
  PD_Base,
  src_filter = [
    "+<Core/src/usbpd_trace.c>",
    "+<Devices/STM32L5XX/src/*.c>"
  ]
)

#Tracer_Lib = env.BuildSources(
#  os.path.join("$BUILD_DIR", "TRACER_EMB"),
#  Tracer_Base
#)

#env.Dump()
#print(PD_Base)
#print(PD_Includes)
#env.PrintConfiguration()
#print(env.get('PIOFRAMEWORK'))
#print(env.PioPlatform().frameworks)
#print(env.PioPlatform().get_dir())
#print(env.PioPlatform().get_build_script())
#print(env.BoardConfig())

