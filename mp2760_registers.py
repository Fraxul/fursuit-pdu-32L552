import csv
import re
from datetime import datetime

inputFilename = "MP2760GVT-XXXX.txt"

registerData = {}

with open(inputFilename, "r", encoding="utf8") as register_file:
    tsv_reader = csv.reader(register_file, delimiter="\t", skipinitialspace=True)
    for row in tsv_reader:
        # print(row)

        # Sample row format
        # 0000	0	0B	11	REG0B	3080	12416
        # 0: Variant code
        # 1: always zero?,
        # 2: address (hex)
        # 3: address (dec)
        # 4: register name
        # 5: value (hex)
        # 6: value (dec)

        if (row[0] == "END"):
            break

        regAddress = int(row[2], base=16)
        regName = str(row[4])
        regValue = int(row[5], base=16)

        # Only registers 0x05 - 0x15 are configured here.

        if (regAddress >= 0x5 and regAddress <= 0x15):
            registerData[regName] = {'address': regAddress, 'value': regValue, 'comments' : []}
            # print(f"{regName}: {regAddress:#02x} => {regValue:#06x}")

    # Parse register bit-descriptions for comments
    # format looks like:
    # **** REG05[9]:Watchdog Timer Setting when Input Absent=1-Enable
    reg_desc = re.compile(r'^\*\*\*\*\s*(REG[0-9a-fA-F]{2})(.*)$')
    for line in register_file:
        m = reg_desc.match(line)
        if (m is None):
            continue
        # print(f"{m.group(1)} => {m.group(2)}")
        reg = m.group(1)
        comment = (m.group(1) + m.group(2)).strip()
        if reg in registerData:
            registerData[reg]['comments'].append(comment)

# print(registerData)

header = f"// Automatically generated from {inputFilename} on {datetime.now().ctime()}"

with open("Inc/mp2760_registers.h", "w", encoding="utf-8") as f:

  f.write(f"""#pragma once
#include <stdint.h>

{header}

struct MP2760_Register {{
  uint8_t id;
  uint16_t value;
}};

static constexpr uint8_t mp2760_defaultConfig_length = {len(registerData)};
extern const MP2760_Register mp2760_defaultConfig[];
""")

with open("Src/mp2760_registers.cpp", "w", encoding="utf-8") as f:
    f.write(f"""#include "mp2760_registers.h"

{header}

const MP2760_Register mp2760_defaultConfig[] = {{
""")

    for data in registerData.values():
        for comment in data['comments']:
            f.write(f"  // {comment}\n")
        f.write(f"  {{ .id = {data['address']:#02x}, .value = {data['value']:#04x} }},\n")

    f.write("};\n")


