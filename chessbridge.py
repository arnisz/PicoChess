import os
import serial
import sys
import threading

RED = "\033[31m"
GREEN = "\033[32m"
RESET = "\033[0m"

DEBUG = os.getenv("CHESSBRIDGE_DEBUG") == "1"

ser = serial.Serial('COM15', 115200, timeout=1)  # Port anpassen!

def read_from_pico():
    while True:
        line = ser.readline().decode().strip()
        if line:
            sys.stdout.write(line + "\n")
            sys.stdout.flush()
            if DEBUG:
                sys.stderr.write(f"{GREEN}Pico → GUI: {line}{RESET}\n")
                sys.stderr.flush()

threading.Thread(target=read_from_pico, daemon=True).start()

while True:
    try:
        line = sys.stdin.readline()
        if not line:
            break
        if DEBUG:
            sys.stderr.write(f"{RED}GUI → Pico: {line.rstrip()}{RESET}\n")
            sys.stderr.flush()
        ser.write(line.encode())
    except KeyboardInterrupt:
        break
