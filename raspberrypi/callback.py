# coding: utf-8

import os
import random
import re
import serial
import subprocess
import sys
import time


now = time.time()
mins = int(now / 60)

PATH_RUNNING = f"/home/pi/running_{mins}"
MOUNT_FOLDER = "/mnt/media"
PATH_LAST_LOCK = "/home/pi/lastlock"

# Prevent multiple calls
if os.path.isfile(PATH_RUNNING):
    print(f"Already running ({mins})", flush=True)
    sys.exit(0)
with open(PATH_RUNNING, "w") as file:
    file.write("a")

print(f"RUNNING {mins}", flush=True)

def exec(*cmd):
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process.wait()
    return process.stdout.read().decode(), process.stderr.read().decode()

exec("sudo", "umount", MOUNT_FOLDER)

def get_part():
    out, err = exec("lsblk")
    parts = []
    for line in out.split("\n"):
        lsplit = line.strip().split()
        if len(lsplit) < 6:
            continue
        name = lsplit[0]
        size = lsplit[3]
        dtype = lsplit[5]
        if dtype != "part":
            continue
        rmatch = re.search(r"sda\d+", name)
        if rmatch is None:
            continue
        parts.append(rmatch[0])
    if len(parts) > 0:
        return parts[0]
    return None

# Wait for partition to be available
# time.sleep(1)

part = None
for _ in range(30):
    part = get_part()
    if part is not None:
        break
    time.sleep(0.1)

if part is None:
    print("No partition to mount, exiting", flush=True)
    if os.path.isfile(PATH_RUNNING):
        os.remove(PATH_RUNNING)
    sys.exit(0)

msgs = [
    b"\x21\x62\x61\x3F\r\n", #0
    b"\x21\x62\x62\x3F\r\n", #1
    b"\x21\x62\x63\x3F\r\n", #2
    b"\x21\x62\x64\x3F\r\n", #3
    b"\x21\x62\x65\x3F\r\n", #4
    b"\x21\x62\x66\x3F\r\n", #5
    b"\x21\x62\x67\x3F\r\n", #6
    b"\x21\x62\x68\x3F\r\n", #7
]

msgfail = b"\x21\x63\x61\x3F\r\n"

lastlock = 0
if os.path.isfile(PATH_LAST_LOCK):
    with open(PATH_LAST_LOCK, "r") as file:
        lastlock = int(file.read().strip())

tosend = []
print("Mounting", part, flush=True)
exec("sudo", "mount", f"/dev/{part}", MOUNT_FOLDER)
files = next(os.walk(MOUNT_FOLDER))[2]

# print("Files:", files)

spellings = {
    0: ["batterie"],
    1: ["clavier"],
    2: ["disque dur"],
    3: ["lecteur cd"],
    4: ["m�moire vive", "m?emoire vive", "memoire vive"],
    5: ["processeur"],
    6: ["souris"],
    
}

keyfiles = {
    "batterie.jpg": 0,
    "clavier.jpg": 1,
    "disque dur.jpg": 2,
    "lecteur CD.jpg": 3,
    "mémoire vive.jpg": 4,
    "m?moire vive.jpg": 4,
    "processeur.jpg": 5,
    "souris.jpg": 6,
    "système de ventilation.jpg": 7,
    "syst?me de ventilation.jpg": 7,
}

for path in files:
    basename = os.path.basename(path)
    if basename in keyfiles:
        file_index = keyfiles[basename]
        tosend.append(msgs[file_index])
    elif basename.endswith(".txt"):
        with open(os.path.join(MOUNT_FOLDER, path), "r", encoding="utf8") as file:
            for line in file.read().split("\n"):
                try:
                    lock_index = int(line.strip())
                    if lock_index >= 0 and lock_index < len(msgs):
                        tosend.append(msgs[lock_index])
                except ValueError:
                    pass
if len(tosend) == 0:
    tosend.append(msgfail)

# Rotating lock at each plug, for testing
# if "text.txt" in files:
#     lastlock = (lastlock + 1) % len(msgs)
#     with open(PATH_LAST_LOCK, "w") as file:
#         file.write(str(lastlock))
#     tosend.append(msgs[lastlock])
# else:
#     tosend.append(msgfail)

print("Dismounting", part, flush=True)
exec("sudo", "umount", MOUNT_FOLDER)

# Determine Arduino port, default is /dev/ttyACM0
# out, err = exec("/usr/bin/python", "-m", "serial.tools.list_ports")
# port = [p.strip() for p in out.strip().split("\n")][0]
port = "/dev/ttyACM0"

with serial.Serial(port, 9600, timeout=1) as ser:
    for msg in tosend:
        print("Sending", msg, flush=True)
        buffer = ""
        for attempt in range(10):
            ser.write(msg)
            buffer += ser.read(2).decode()
            if "ok" in buffer:
                break
            time.sleep(1)

time.sleep(3)
os.remove(PATH_RUNNING)
print("DONE!", flush=True)
