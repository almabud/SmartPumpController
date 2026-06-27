# PlatformIO CLI Command Reference

> Run all commands from inside the board folder (where `platformio.ini` lives).
> e.g. `cd pump-controller` before running any of these.

---

## Building

```bash
pio run                           # build only
pio run -e esp32-s3               # build a specific environment
pio run -t clean                  # delete all build artifacts
pio run -t clean && pio run       # clean then rebuild from scratch
pio run -t compiledb              # generate compile_commands.json (fixes VS Code IntelliSense squiggles)
```

---

## Uploading

```bash
pio run -t erase                  # erase the code from the microcontroller
pio run -t upload                 # build + upload to connected board
pio run -e esp32-s3 -t upload     # build + upload specific environment
```

---

## Serial monitor

```bash
pio device monitor                # open serial monitor (uses baud from platformio.ini)
pio device monitor --baud 115200  # override baud rate
```

---

## Build + upload + monitor (most common workflow)

```bash
pio run -t upload && pio device monitor
```

---

## Devices

```bash
pio device list                   # list all connected serial/USB devices
```

---

## Libraries

```bash
pio lib list                      # list libraries installed in this project
pio lib search "RadioHead"        # search PlatformIO registry
pio lib install "RadioHead"       # install a library manually
pio lib update                    # update all installed libraries
```

---

## Testing

```bash
pio test                          # run all tests (on-target)
pio test -e native                # run native tests on your Mac (no hardware needed)
```

---

## Project

```bash
pio project init                  # initialise a new PlatformIO project in current folder
```

---

## Quick reference — daily workflow

| Task | Command |
|------|---------|
| Does it compile? | `pio run` |
| Flash to board | `pio run -t upload` |
| Watch serial output | `pio device monitor` |
| Flash and watch | `pio run -t upload && pio device monitor` |
| Fix IntelliSense squiggles | `pio run -t compiledb` |
| Start fresh | `pio run -t clean && pio run` |
| What port is my board on? | `pio device list` |

---

## Notes

- All commands must be run from inside the board folder (`pump-controller/` or
  `water-tank/`), not from the monorepo root — PlatformIO looks for
  `platformio.ini` in the current directory.
- If `pio` is not found in your regular terminal, use the
  **PlatformIO terminal** in VS Code: `Cmd+Shift+P` →
  "PlatformIO: New Terminal".
- The `-e` flag selects a specific `[env:name]` from `platformio.ini` —
  useful when you have multiple environments (e.g. `esp32-s3` and `native`).
