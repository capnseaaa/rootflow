"""Replace $name and $name+offset(+offset...) in a test script with addresses from the ELF symbol table.
Usage: python3 resolve.py SCRIPT ELF > resolved.txt"""
import re, subprocess, sys

syms = {}
for line in subprocess.run(["arm-none-eabi-nm", sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    parts = line.split()
    if len(parts) == 3:
        syms[parts[2]] = int(parts[0], 16)


def sub(m):
    name = m.group(1)
    off = sum(int(n) for n in re.findall(r"\+(\d+)", m.group(2) or ""))
    if name not in syms:
        sys.exit(f"unknown symbol ${name}")
    return "%08x" % (syms[name] + off)


for line in open(sys.argv[1]):
    if line.startswith("#"):          # comments may mention $names freely
        sys.stdout.write(line)
        continue
    sys.stdout.write(re.sub(r"\$([A-Za-z_]\w*)((?:\+\d+)*)", sub, line))
