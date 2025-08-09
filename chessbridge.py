import serial
import sys
import threading

ser = serial.Serial('COM15', 115200, timeout=1)  # Port anpassen!

def read_from_pico():
    while True:
        line = ser.readline().decode().strip()
        if line:
            print(line, flush=True)

threading.Thread(target=read_from_pico, daemon=True).start()

while True:
    try:
        line = sys.stdin.readline()
        if not line:
            break
        ser.write(line.encode())
    except KeyboardInterrupt:
        break
