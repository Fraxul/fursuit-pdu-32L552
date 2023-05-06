#include "max17320_registers.h"

// Automatically generated from MAX17320_Config.INI on Sat May  6 04:39:14 2023

const MAX17320_Register max17320_defaultConfig[] = {
  { .id = 0x180, .value = 0x00 }, // nXTable0 Register
  { .id = 0x181, .value = 0x00 }, // nXTable1 Register
  { .id = 0x182, .value = 0x00 }, // nXTable2 Register
  { .id = 0x183, .value = 0x00 }, // nXTable3 Register
  { .id = 0x184, .value = 0x00 }, // nXTable4 Register
  { .id = 0x185, .value = 0x00 }, // nXTable5 Register
  { .id = 0x186, .value = 0x00 }, // nXTable6 Register
  { .id = 0x187, .value = 0x00 }, // nXTable7 Register
  { .id = 0x188, .value = 0x00 }, // nXTable8 Register
  { .id = 0x189, .value = 0x00 }, // nXTable9 Register
  { .id = 0x18a, .value = 0x00 }, // nXTable10 Register
  { .id = 0x18b, .value = 0x00 }, // nXTable11 Register
  { .id = 0x18c, .value = 0x00 }, // nVAlrtTh Register
  { .id = 0x18d, .value = 0x00 }, // nTAlrtTh Register
  { .id = 0x18e, .value = 0x00 }, // nIAlrtTh Register
  { .id = 0x18f, .value = 0x00 }, // nSAlrtTh Register
  { .id = 0x190, .value = 0x00 }, // nOCVTable0 Register
  { .id = 0x191, .value = 0x00 }, // nOCVTable1 Register
  { .id = 0x192, .value = 0x00 }, // nOCVTable2 Register
  { .id = 0x193, .value = 0x00 }, // nOCVTable3 Register
  { .id = 0x194, .value = 0x00 }, // nOCVTable4 Register
  { .id = 0x195, .value = 0x00 }, // nOCVTable5 Register
  { .id = 0x196, .value = 0x00 }, // nOCVTable6 Register
  { .id = 0x197, .value = 0x00 }, // nOCVTable7 Register
  { .id = 0x198, .value = 0x00 }, // nOCVTable8 Register
  { .id = 0x199, .value = 0x00 }, // nOCVTable9 Register
  { .id = 0x19a, .value = 0x00 }, // nOCVTable10 Register
  { .id = 0x19b, .value = 0x00 }, // nOCVTable11 Register
  { .id = 0x19c, .value = 0x140 }, // nIChgTerm Register
  { .id = 0x19d, .value = 0x00 }, // nFilterCfg Register
  { .id = 0x19e, .value = 0x00 }, // nVEmpty Register
  { .id = 0x19f, .value = 0x00 }, // nLearnCfg Register
  { .id = 0x1a0, .value = 0x1050 }, // nQRTable00 Register
  { .id = 0x1a1, .value = 0x8002 }, // nQRTable10 Register
  { .id = 0x1a2, .value = 0x78c }, // nQRTable20 Register
  { .id = 0x1a3, .value = 0x880 }, // nQRTable30 Register
  { .id = 0x1a4, .value = 0x00 }, // nCycles Register
  { .id = 0x1a5, .value = 0x1804 }, // nFullCapNom Register
  { .id = 0x1a6, .value = 0x8cc }, // nRComp0 Register
  { .id = 0x1a7, .value = 0x223e }, // nTempCo Register
  { .id = 0x1a8, .value = 0x00 }, // nBattStatus Register
  { .id = 0x1a9, .value = 0x14b4 }, // nFullCapRep Register
  { .id = 0x1aa, .value = 0x00 }, // ndQTot Register
  { .id = 0x1ab, .value = 0x00 }, // nMaxMinCurr Register
  { .id = 0x1ac, .value = 0x00 }, // nMaxMinVolt Register
  { .id = 0x1ad, .value = 0x00 }, // nMaxMinTemp Register
  { .id = 0x1ae, .value = 0x02 }, // nFaultLog Register
  { .id = 0x1af, .value = 0x00 }, // nTimerH Register
  { .id = 0x1b0, .value = 0x26d0 }, // nCONFIG Register
  { .id = 0x1b1, .value = 0x204 }, // nRippleCfg Register
  { .id = 0x1b2, .value = 0x00 }, // nMiscCFG Register
  { .id = 0x1b3, .value = 0x14b4 }, // nDesignCap Register
  { .id = 0x1b4, .value = 0x08 }, // nSBSCFG Register
  { .id = 0x1b5, .value = 0x06 }, // nPACKCFG Register
  { .id = 0x1b6, .value = 0x83b }, // nRelaxCFG Register
  { .id = 0x1b7, .value = 0x2241 }, // nConvgCFG Register
  { .id = 0x1b8, .value = 0xa80 }, // nNVCFG0 Register
  { .id = 0x1b9, .value = 0x182 }, // nNVCFG1 Register
  { .id = 0x1ba, .value = 0xbe2d }, // nNVCFG2 Register
  { .id = 0x1bb, .value = 0x909 }, // nHibCFG Register
  { .id = 0x1bc, .value = 0x6e26 }, // nROMID0 Register
  { .id = 0x1bd, .value = 0x9d58 }, // nROMID1 Register
  { .id = 0x1be, .value = 0xe0 }, // nROMID2 Register
  { .id = 0x1bf, .value = 0xa100 }, // nROMID3 Register
  { .id = 0x1c0, .value = 0x00 }, // nChgCtrl1 Register
  { .id = 0x1c1, .value = 0x00 }, // nPReserved1 Register
  { .id = 0x1c2, .value = 0x2061 }, // nChgCfg0 Register
  { .id = 0x1c3, .value = 0xe1 }, // nChgCtrl0 Register
  { .id = 0x1c4, .value = 0x00 }, // nRGain Register
  { .id = 0x1c5, .value = 0x00 }, // nPackResistance Register
  { .id = 0x1c6, .value = 0x00 }, // nFullSOCThr Register
  { .id = 0x1c7, .value = 0x00 }, // nTTFCFG Register
  { .id = 0x1c8, .value = 0x4000 }, // nCGAIN Register
  { .id = 0x1c9, .value = 0x00 }, // nCGTempCo Register
  { .id = 0x1ca, .value = 0x71be }, // nThermCfg Register
  { .id = 0x1cb, .value = 0x00 }, // nChgCfg1 Register
  { .id = 0x1cc, .value = 0x00 }, // nManfctrName Register
  { .id = 0x1cd, .value = 0x00 }, // nManfctrName1 Register
  { .id = 0x1ce, .value = 0x00 }, // nManfctrName2 Register
  { .id = 0x1cf, .value = 0x1f4 }, // nRSense Register
  { .id = 0x1d0, .value = 0x508c }, // nUVPrtTh Register
  { .id = 0x1d1, .value = 0x3700 }, // nTPrtTh1 Register
  { .id = 0x1d2, .value = 0x5528 }, // nTPrtTh3 Register
  { .id = 0x1d3, .value = 0x4b9c }, // nIPrtTh1 Register
  { .id = 0x1d4, .value = 0x1060 }, // nBALTh Register
  { .id = 0x1d5, .value = 0x2d0a }, // nTPrtTh2 Register
  { .id = 0x1d6, .value = 0x7a78 }, // nProtMiscTh Register
  { .id = 0x1d7, .value = 0x900 }, // nProtCfg Register
  { .id = 0x1d8, .value = 0x5d4b }, // nJEITAC Register
  { .id = 0x1d9, .value = 0x59 }, // nJEITAV Register
  { .id = 0x1da, .value = 0xb754 }, // nOVPrtTh Register
  { .id = 0x1db, .value = 0xc884 }, // nStepChg Register
  { .id = 0x1dc, .value = 0xab3d }, // nDelayCfg Register
  { .id = 0x1dd, .value = 0xe6b }, // nODSCTh Register
  { .id = 0x1de, .value = 0x4355 }, // nODSCCfg Register
  { .id = 0x1df, .value = 0xa0a9 }, // nProtCfg2 Register
  { .id = 0x1e0, .value = 0x00 }, // nDPLimit Register
  { .id = 0x1e1, .value = 0x00 }, // nScOcvLim Register
  { .id = 0x1e2, .value = 0x00 }, // nAgeFcCfg Register
  { .id = 0x1e3, .value = 0xa5b9 }, // nDesignVoltage Register
  { .id = 0x1e4, .value = 0x00 }, // nVGain Register
  { .id = 0x1e5, .value = 0x00 }, // nRFastVShdn Register
  { .id = 0x1e6, .value = 0x00 }, // nManfctrDate Register
  { .id = 0x1e7, .value = 0x00 }, // nFirstUsed Register
  { .id = 0x1e8, .value = 0x00 }, // nSerialNumber0 Register
  { .id = 0x1e9, .value = 0x00 }, // nSerialNumber1 Register
  { .id = 0x1ea, .value = 0x00 }, // nSerialNumber2 Register
  { .id = 0x1eb, .value = 0x00 }, // nDeviceName0 Register
  { .id = 0x1ec, .value = 0x00 }, // nDeviceName1 Register
  { .id = 0x1ed, .value = 0x00 }, // nDeviceName2 Register
  { .id = 0x1ee, .value = 0x00 }, // nDeviceName3 Register
  { .id = 0x1ef, .value = 0x00 }, // nDeviceName4 Register
};
