"""Patch the GBA header checksum (byte 0xBD) and pad the ROM to a 4-byte size."""
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
chk = 0
for b in data[0xA0:0xBD]:
    chk = (chk - b) & 0xFF
data[0xBD] = (chk - 0x19) & 0xFF
while len(data) % 4:
    data.append(0)
open(path, "wb").write(data)
print(f"header checksum 0x{data[0xBD]:02X}, size {len(data)} bytes")
