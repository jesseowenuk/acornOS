#!/usr/bin/env bash

KERNEL=$1
IMAGE=$2
SECTORS=$(( ($(wc -c < "$KERNEL") + 511) / 512 ))
echo "kernel: $SECTORS sectors"

# Write as 4 byte little-endian at sector 63
python3 -c "
import struct, sys

sectors = $SECTORS
data = struct.pack('<I', sectors)   # little-endian uint32
with open('$IMAGE', 'r+b') as f:
    f.seek(63 * 512)
    f.write(data)
print(f'Written {sectors} sectors to sector 63')
"