#include "mp2760_registers.h"

// Automatically generated from MP2760GVT-XXXX.txt on Sun May  7 22:09:51 2023

const MP2760_Register mp2760_defaultConfig[] = {
  // REG05[9]:Watchdog Timer Setting when Input Absent=0-Disable
  { .id = 0x5, .value = 0x08 },
  // REG06[7:0]:Input Minimum Voltage Limit=4560
  { .id = 0x6, .value = 0x39 },
  // REG07[6:0]:Minimum System Voltage Threshold=11000
  { .id = 0x7, .value = 0x37 },
  // REG08[6:0]:Input Current Limit=500
  { .id = 0x8, .value = 0x0a },
  // REG09[10]:SRC Output Voltage Configuration=0-By register bit
  // REG09[11]:SRC Output Voltage Offset=0
  // REG09[9:0]:SRC Output Voltage=4980
  { .id = 0x9, .value = 0xf9 },
  // REG0A[10:8]:Voltage Compensation Limit=0
  // REG0A[13:11]:Battery Impedance=100
  // REG0A[6:0]:SRC Ouput Current Limit=2000
  { .id = 0xa, .value = 0x1028 },
  // REG0B[10:9]:Battery Low Voltage Threshold=12
  // REG0B[11]:DC/DC Action when Battery Low Voltage=0-INT
  // REG0B[12]:Pre-charge Threshold=12
  // REG0B[13]:Battery Low Voltage Protection Enable=1-Enable
  // REG0B[7:0]:Battery Discharge Current Limit=6400
  // REG0B[8]:Battery Discharge Current Limit Enable=0-Disable
  { .id = 0xb, .value = 0x3080 },
  // REG0C[10:6]:VBATT_REG Setting when Warm/Cool=320
  // REG0C[12:11]:Charge Action when Cool=10-ICC
  // REG0C[14:13]:Charge Action when Warm=01-VBATT_REG
  // REG0C[5:4]:ICC Setting when Warm/Cool=01-1/4 times
  { .id = 0xc, .value = 0x3410 },
  // REG0D[1:0]:Cold Threshold=01-74.2%;(0?C)
  // REG0D[12:10]:TS Temperature Threshold=100-14.3%;(80?C)
  // REG0D[13]:TS Function Action Enable when Fault=1-INT and TS action
  // REG0D[14]:TS Sense Point=1-Battery FET
  // REG0D[15]:TS Function Enable=1-Enable
  // REG0D[3:2]:Cool Threshold=10-64.8%;(10?C)
  // REG0D[5:4]:Warm Threshold=01-32.6%;(45?C)
  // REG0D[7:6]:Hot Threshold=10-23.0%;(60?C)
  // REG0D[8]:NTC_Action=1-INT and JEITA action
  // REG0D[9]:NTC Protection Enable=1-Enable
  { .id = 0xd, .value = 0xf399 },
  // REG0E[6:4]:PWM Frequency=600
  // REG0E[7]:ADC Conversion Behavior=0-One-shot Conversion
  // REG0E[8]:ADC Conversion One-shot Enable=0-Disable ADC
  { .id = 0xe, .value = 0x10 },
  // REG0F[11:8]:Trickle Charge Current=100
  // REG0F[14:12]:Thermal Loop Temperature Threshold=111-120?C
  // REG0F[15]:Thermal loop Enable=1-Enable
  // REG0F[3:0]:Termination Current=50
  // REG0F[7:4]:Pre-charge Current=400
  { .id = 0xf, .value = 0xf241 },
  // REG10[10:9]:Battery Cell Count=4
  // REG10[11]:Recharge Threshold=100
  // REG10[12]:TS/IMON Pin Function=0-TS
  // REG10[13]:BGATE Force Off=0-Not force BGATE off
  // REG10[14]:ACGATE Force On=0-Not force ACGATE on
  // REG10[4:0]:Vtrack Per Cell=150
  // REG10[5]:Virtual Diode Enable=1-Enable
  // REG10[6]:ACGATE Driver Enable=1-Enable ACGAET
  // REG10[7]:Battery Current Sense Resistor=0-10mohm
  // REG10[8]:Input Current Sense Resistor=0-10mohm
  { .id = 0x10, .value = 0x67e },
  // REG11[10]:Input Over Voltage Deglitch Time=0-100ns
  // REG11[12:11]:System UVP=00-75%;
  // REG11[14:13]:System OVP=11-110%
  // REG11[3]:Battery OVP Enable=1-Enable BATT OVP
  // REG11[5]:System OVP enable=1-Enable SYS OVP
  // REG11[7:6]:Input Over Voltage Threshold=22.4
  // REG11[9:8]:Input Under Voltage Threshold=3.2
  { .id = 0x11, .value = 0x60e8 },
  // REG12[0]:Charge Enable=0-Charge Disable
  // REG12[1]:Input Current Limit Disable=1-IIN_LIM Enable
  // REG12[10]:Safety Timer Extension=1-Extend by 2X
  // REG12[12:11]:Safety Timer=20 hours
  // REG12[13]:Safety Timer Enable=1-Enable
  // REG12[3]:SRC Mode Enable=0-Disable Discharge
  // REG12[4]:Charge Termination Enable=1-Enable
  // REG12[5]:BGATE Driver Enable=1-Enable BGATE driver block
  // REG12[6]:DC/DC Enable=0-Disable
  // REG12[8:7]:Watchdog Timer=00-Disable Timer
  // REG12[9]:Watchdog Feed Bit=0-Normal
  { .id = 0x12, .value = 0x3c32 },
  // REG14[13:6]:Fast Charge Current=3600
  { .id = 0x14, .value = 0x1200 },
  // REG15[14:4]:Battery Full Voltage=16800
  { .id = 0x15, .value = 0x6900 },
};
