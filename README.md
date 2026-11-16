# M5Sensor

M5AtomS3とIM920sを使用する温度センサーノードです。MCP9808から温度を取得し、IM920s経由でM5Gatewayへ8バイトのセンサーデータを送信します。

## Hardware

- M5AtomS3
- IM920s
  - TX: GPIO 5
  - RX: GPIO 6
  - RESET: GPIO 7
- MCP9808 temperature sensor

## Build

- Board: M5AtomS3 (`m5stack:esp32:m5stack_atoms3`)
- Required libraries:
  - M5AtomS3
  - Adafruit MCP9808 Library

Open `IM920Sen01.ino` in Arduino IDE and select M5AtomS3 as the target board.
