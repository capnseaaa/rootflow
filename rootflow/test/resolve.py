"""Replace $name and $name+offset in a test script with addresses from the ELF symbol table.
Usage: python3 resolve.py SCRIPT ELF > resolved.txt"""
import re, subprocess, sys

syms = {}
for line in subprocess.run(["arm-none-eabi-nm", sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    parts = line.split()
    if len(parts) == 3:
        syms[parts[2]] = int(parts[0], 16)


def sub(m):
    name, off = m.group(1), int(m.group(2) or 0)
    if name not in syms:
        sys.exit(f"unknown symbol ${name}")
    return "%08x" % (syms[name] + off)


for line in open(sys.argv[1]):
    sys.stdout.write(re.sub(r"\$([A-Za-z_]\w*)(?:\+(\d+))?", sub, line))
