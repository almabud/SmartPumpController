---
description: Compile the firmware and report only the errors that matter
allowed-tools: Bash(pio run), Bash(pio run -e:*), Read, Grep
---

Build the firmware from the `pump-controller/` folder (where `platformio.ini` lives):

!`pio run`

Report the result:

- On success: state RAM/Flash usage and stop.
- On failure: quote the first compiler error verbatim, open the offending
  file at the reported line, and explain the cause. Do not fix it unless asked.

Ignore clangd/IntelliSense squiggles about `Arduino.h` or `TFT_*` identifiers —
those come from a missing include path in the editor, not from the build.
Run `pio run -t compiledb` if the user wants them gone.
