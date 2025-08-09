# Pi Pico Chess Engine

A chess engine targeting the Raspberry Pi Pico. Core logic is written in portable C++ and separated from the Arduino interface.

## Development

```bash
g++ -std=c++17 -x c++ -fsyntax-only -I./src -I./test pi-pico-engine.ino
cd test && make && ./chess_engine_test
```

## Arduino

```bash
arduino-cli compile --fqbn rp2040:rp2040:rpipico .
# arduino-cli upload -p /dev/ttyACM0 --fqbn rp2040:rp2040:rpipico .
```
