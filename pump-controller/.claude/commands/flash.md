---
description: Build, upload to the ESP32-S3, and open the serial monitor
allowed-tools: Bash(pio device list), Bash(pio run -t upload:*), Bash(pio device monitor:*)
---

Flash the connected board and watch its output.

1. Confirm a board is attached: `pio device list`. If none is listed, stop and
   say so — do not attempt the upload.
2. Build and upload: `pio run -t upload`
3. Open the monitor: `pio device monitor`

The monitor runs until interrupted, so launch it in the background and report
the first lines of boot output.
