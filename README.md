# M5Sensor

M5AtomS3RまたはM5AtomS3 LiteとIM920sを使用する温度センサーノードです。MCP9808から温度を取得し、IM920s経由でM5Gatewayへ8バイトのセンサーデータを送信します。

起動時にディスプレイの有無を自動検出します。同じスケッチを使用し、M5AtomS3Rでは画面に状態を表示し、M5AtomS3 Liteでは画面描画を行わずに動作します。

## Hardware

- M5AtomS3R（ディスプレイあり）またはM5AtomS3 Lite（ディスプレイなし）
- IM920s
  - TX: GPIO 5
  - RX: GPIO 6
  - RESET: GPIO 7
- MCP9808 temperature sensor

## Build

- Board:
  - M5AtomS3R: `m5stack:esp32:m5stack_atoms3r`
  - M5AtomS3 Lite: `m5stack:esp32:m5stack_atoms3`
- Required libraries:
  - M5AtomS3
  - Adafruit MCP9808 Library

Open `IM920Sen01.ino` in Arduino IDE and select the target board. `DISP`などのソース変更は不要です。
