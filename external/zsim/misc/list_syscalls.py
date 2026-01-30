#!/usr/bin/python3
# Produces a list of syscalls in the current system
# Updated for Ubuntu 24.04 compatibility
import os, re, glob

# Try multiple possible locations for unistd.h
possible_paths = [
    "/usr/include/x86_64-linux-gnu/asm/unistd_64.h",
    "/usr/include/x86_64-linux-gnu/asm/unistd.h",
    "/usr/include/asm/unistd.h",
    "/usr/include/asm-generic/unistd.h",
]

# Find the first existing path
unistd_path = None
for path in possible_paths:
    if os.path.exists(path):
        unistd_path = path
        break

if unistd_path is None:
    # Fallback: search for any unistd header
    matches = glob.glob("/usr/include/*/asm/unistd*.h")
    if matches:
        unistd_path = matches[0]
    else:
        print("// ERROR: Could not find unistd.h")
        exit(1)

syscallCmd = f"gcc -E -dD {unistd_path} | grep __NR_"
syscallDefs = os.popen(syscallCmd).read()
sysList = [(int(numStr), name) for (name, numStr) in re.findall("#define __NR_(.*?) (\d+)", syscallDefs)]

if not sysList:
    print("// ERROR: No syscalls found")
    exit(1)

denseList = ["INVALID"]*(max([num for (num, name) in sysList]) + 1)
for (num, name) in sysList: denseList[num] = name
print('"' + '",\n"'.join(denseList) + '"')
