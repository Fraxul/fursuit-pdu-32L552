Import("env")

# STM32L5 Cortex-M33 implement VFPv5, 32 SP / 16 DP registers
env.Append(
  CFLAGS=[ "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"],
  CPPFLAGS=[ "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"],
  LINKFLAGS=[ "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"]
)
