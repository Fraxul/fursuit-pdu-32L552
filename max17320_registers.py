import re
from datetime import datetime

inputFilename = "MAX17320_Config.INI"
header = f"// Automatically generated from {inputFilename} on {datetime.now().ctime()}"
registerCount = 0
nRSense = 0

with open("Src/max17320_registers.cpp", "w", encoding="utf-8") as cpp:
    cpp.write(f"""#include "max17320_registers.h"

{header}

const MAX17320_Register max17320_defaultConfig[] = {{
""")
    with open(inputFilename, "r", encoding="utf8") as register_file:
        reg_value = re.compile(r'^(0x[0-9a-fA-F]{3})\s*=\s*(0x[0-9a-fA-F]+)\s+//(.*)$')
        for line in register_file:
            m = reg_value.match(line)
            if (m is None):
                continue

            reg = int(m.group(1), base=16)
            value = int(m.group(2), base=16)
            comment = str(m.group(3)).strip()
            if (reg == 0x1cf):
                nRSense = value

            # Skip registers that are reserved and should not be written to.
            if (reg == 0x1c0 or reg == 0x1c1): # nPReserved0, nPReserved1
                continue
            if (reg == 0x1cb): # Reserved
                continue
            if (reg == 0x1e4 or reg == 0x1e5): # Reserved
                continue
            if (reg >= 0x1bc and reg <= 0x1bf): # nROMID[0-3]
                continue

            cpp.write(f"  {{ .id = {reg:#02x}, .value = {value:#04x} }}, // {comment}\n")
            registerCount += 1
    cpp.write("};\n")

with open("Inc/max17320_registers.h", "w", encoding="utf-8") as h:

  h.write(f"""#pragma once
#include <stdint.h>

{header}

struct MAX17320_Register {{
  uint16_t id;
  uint16_t value;
}};

static constexpr uint8_t max17320_defaultConfig_length = {registerCount};
static constexpr uint16_t max17320_nRSense = {nRSense};

// Convert value from register 0x1c to microamps using the sense resistor value
// sense resistor is 10 microOhms per LSB
// microamps = value *  (1.5625 / (nRSense * 0.00001))
// for 5 mOhms: value * (1.5625 / 0.005 == 312.5)
// sample register 0x1c value for 1 A current with 5 mOhm sense resistor: -3368

static constexpr float max17320_current_conversion_factor_milliamps = {(156250.0 / float(nRSense)) / 1000.0};
extern const MAX17320_Register max17320_defaultConfig[];
""")
