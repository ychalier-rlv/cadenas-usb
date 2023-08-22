import serial
import time

time.sleep(10)

msg = b"!fa?\r\n"
port = "/dev/ttyACM0"

print("Startup script starting")

with serial.Serial(port, 9600, timeout=1) as ser:
    buffer = ""
    for attempt in range(100):
        print("Attempt", attempt)
        ser.write(msg)
        buffer += ser.read(2).decode()
        if "ok" in buffer:
            break
        time.sleep(1)

print("Startup script done")
