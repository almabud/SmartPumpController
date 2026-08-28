# Blynk — Interim Connectivity

> **Status: design only. Not implemented.** A deliberately temporary path to get
> one device live on a phone while the real platform is built. Companion to
> [cloud-architecture.md](cloud-architecture.md) and
> [mobile-app-architecture.md](mobile-app-architecture.md), both of which remain
> the destination. This document describes the thing that gets deleted.

---

## Why

Getting one pump visible and controllable from a phone costs roughly **one week**
through Blynk and **ten to twelve weeks** through our own stack, for the same
user-visible outcome:

| | Blynk | Own stack |
|---|---|---|
| Console: template + ~17 datastreams | 2–3 h | — |
| Firmware module mapping `SystemState` → datastreams | 1–2 d | — |
| Wi-Fi connect (hardcoded creds in `secrets.h`) | 2 h | 2–3 d (`WifiManager`) |
| Cloud: EMQX + TLS + auth hook + Go ingest/API | — | 1.5–2 w |
| Postgres + shadow schema | — | 3–4 d |
| `CloudClient` MQTT+TLS + core-0 split | — | 4–5 d |
| Phone app | included | 6–8 w |
| BLE provisioning (firmware + app) | not needed | 2–3 w |
| History, charts, OTA | included | Phase 6 |
| **Total** | **~1 week** | **~10–12 weeks** |

The week saved is not the real argument. **Nothing has been confirmed on hardware
since the pump timer landed** — the radio link has never run end to end, and the
Phase 2 UI work is unwatched. Blynk gets telemetry onto a phone in a week, which
makes it possible to watch the radio link, the power meter and the pump behave
over *days* rather than minutes at the bench.

Ten weeks of cloud work on unvalidated sensor data would be building on sand.

---

## Sequence — Blynk is step 3, not step 1

1. **Bench session, Nano + ESP32 together.** ~2 days. Close the unverified
   backlog: radio link end to end, config page, bypass shortcut, relay click.
   No new code.
2. **Finish Phase 4.** ~1 week. AUTO logic in `SceneEngine`, `PUMP_MAX_RUN_MS`,
   stale-tank refusal. Without this there is a remote switch, not a pump
   controller.
3. **Blynk.** ~1 week. This document.
4. **Soak.** Weeks. Real pump, real data, watched from a phone.
5. **Own stack, during the soak.** ~10–12 weeks, on hardware that is by then trusted.

**Why Blynk is not first:** it would give remote visibility into a pump that
cannot run itself. A soak period is only worth having if it is watching real AUTO
behaviour rather than a relay being toggled by hand. Two weeks of ordering buys a
soak that actually tests something.

Total to "live pump on my phone, running itself": **~4 weeks.**

---

## Design — `lib/BlynkClient/`

A module like any other. `begin()`, `update(SystemState&)`, calls no other module.
Structurally identical to the `CloudClient` that will replace it: reads state
fields, writes `ActionRequest`s.

Three edits in `main.cpp`, the same as every other module — declare it alongside
the instances at the top, `begin()` it in `setup()`, `update()` it in the `loop()`
pipeline.

### Guardrails that keep it disposable

1. **No Blynk type appears in `SystemState` or any other module.** The dependency
   stops at `BlynkClient.cpp`.
2. **Behind a build flag** in `platformio.ini` (`-D USE_BLYNK`), with the library
   added to `lib_deps`. Dropping the flag drops the module.
3. **Commands arrive as `ActionRequest`,** never as direct driver calls — so
   `PumpDriver`'s interlocks apply identically to a Blynk tap and a button press.
   This is the existing rule, not a new one.
4. **DPs are designed by our own schema, not Blynk's pin model.** The virtual-pin
   mapping exists only inside `BlynkClient`. Blynk's `pinNumber`/`pinType` baggage
   never reaches the wire contract.
5. **Skip Blynk.Edgent.** It brings its own provisioning state machine and OTA and
   takes over far more of the firmware. For one device on a known Wi-Fi, hardcoded
   credentials in `secrets.h` (already gitignored, per the README's safety rules)
   are two hours and almost nothing to remove later.

### Datastream mapping

| vPin | Datastream | Blynk type | Access | `SystemState` source |
|---|---|---|---|---|
| V0 | `tank_level` | INTEGER 0–100 | ro | `tankLevelPct` |
| V1 | `tank_stale` | INTEGER 0/1 | ro | `tankStale` |
| V2 | `tank_temp` | DOUBLE | ro | `tankTempC` |
| V3 | `pump` | INTEGER 0/1 | **rw** | `pumpState` → `pumpRequest` |
| V4 | `mode` | INTEGER enum | **rw** | `mode` |
| V5 | `bypass` | INTEGER 0/1 | **rw** | `bypass` |
| V6 | `voltage` | DOUBLE | ro | `voltage` |
| V7 | `current` | DOUBLE | ro | `current` |
| V8 | `power` | DOUBLE | ro | `powerWatts` |
| V9 | `energy_24h` | DOUBLE | ro | `stats24hEnergyKwh` |
| V10 | `runtime_24h` | INTEGER sec | ro | `stats24hRuntimeSec` |
| V11 | `cycles_24h` | INTEGER | ro | `stats24hCycles` |
| V12–14 | `timer_total`, `timer_run`, `timer_break` | INTEGER sec | **rw** | `timerTotalSec`, `timerRunSec`, `timerBreakSec` |
| V15 | `faults` | INTEGER bitmap | ro | `powerFault`, `pumpFault`, `tankSensorFault`, `tankTempFault` |
| V16 | `uptime` | INTEGER sec | ro | `uptimeSeconds` |
| V17 | `rssi` | INTEGER | ro | `wifiRssi` |

Publishing is gated on `hasChanged(Consumer::CLOUD_CONSUMER, …)` — the change
detection already built into `SystemState`, used exactly as `CloudClient` will.

`BLYNK_WRITE(Vn)` handlers are global, so `BlynkClient.cpp` keeps a file-scope
pointer to `SystemState` set during `begin()`. Slightly ugly, entirely contained,
and gone with the module.

### Threading

`Blynk.run()` can block for a few hundred milliseconds during a reconnect, and
`PowerMeter` samples the ADC on a timing-sensitive cadence — a blocked loop pass
corrupts the RMS window.

Staged, because the interim path is supposed to be fast:

1. **First, in-loop.** Get the mapping working and confirm data reaches the phone.
   ~1 day. Watch `PowerMeter` output for jitter around reconnects.
2. **Before the soak, move to a core-0 FreeRTOS task.** ~2 days. This is the same
   split `CloudClient` needs, described in
   [cloud-architecture.md](cloud-architecture.md) — so it is reusable work, not
   throwaway, and the soak is the part that needs stability.

Use `Blynk.config()` + a non-blocking `Blynk.connect()` rather than
`Blynk.begin()`, which blocks until connected and would stall `setup()`.

---

## What is throwaway, what is not

**Throwaway (~40% of the week):** the Blynk console template, the virtual-pin
mapping (~200 lines), the library dependency.

**Reusable:** the core-0 task split, the "network module reads state, writes
requests" discipline that `CloudClient` inherits directly, hardware validation,
and a DP list refined by actually using it — you learn which data points matter by
watching them for a fortnight.

---

## Exit criteria

Delete `lib/BlynkClient/`, the build flag and the `lib_deps` entry once:

1. `CloudClient` publishes the same DP set to EMQX and is confirmed against the
   shadow, and
2. the Flutter app renders the pump panel and commands round-trip with acks, and
3. one full soak period has run on the own-stack path without regression.

Until all three hold, both paths may run side by side — they are independent
consumers of `SystemState` and do not interact.

---

## Watch out for

- **Free-tier limits and commercial-use terms.** Fine for one personal device;
  confirm before any of this is pointed at something being sold.
- **Blynk is cloud-dependent.** No LAN fallback, so an internet outage means the
  box's own buttons only. That gap is one of the reasons the own-stack path exists.
- **Do not let this delay the wire contract.** Blynk is a display layer, not a
  decision. The contract in
  [cloud-architecture.md](cloud-architecture.md) should be frozen on its own
  schedule.
