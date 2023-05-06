import re
from datetime import datetime

inputFilename = "MAX17320_Config.INI"
header = f"// Automatically generated from {inputFilename} on {datetime.now().ctime()}"
registerCount = 0

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
extern const MAX17320_Register max17320_defaultConfig[];
""")
