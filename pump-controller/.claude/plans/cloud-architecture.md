# Cloud + Mobile App Architecture

> **Status: design only. Nothing here is implemented.** `WifiManager` and
> `CloudClient` are still 8-line stubs. This document records the decisions taken
> and the reasoning behind them, so the device-facing wire contract can be frozen
> before any firmware ships.

---

## Context

The firmware is at Phase 4 of six (see [milestones.md](milestones.md)): sensing
works, the AUTO control logic is unwritten, and Phase 5 was scoped as "Wi-Fi +
MQTT + remote control" for this one device.

The actual goal is larger: **a Tuya-like IoT platform for South Asia** —
Bangladesh, India, Pakistan, Nepal — letting makers ship connected products
without standing up servers. The pump controller is customer #0, not the product.

That reframing splits the work into two deliverables with opposite risk profiles:

1. **The pump product** — firmware, app panel, its cloud presence.
2. **The platform** — the PaaS others build on.

Near term there are 1–2 devices. The design below serves those two while keeping
every device-facing contract platform-shaped, because that is the only part which
cannot be changed after firmware is deployed in someone's roof box.

---

## The core idea: data points

Tuya's moat is not its broker — brokers are commodity. It is the **Data Point
(DP) model**:

```
Product (PID) = versioned list of typed DPs
DP = { id, type, unit, scale, range, access, name }
```

Storage schema, app panel UI, REST surface, automation conditions and OTA
targeting are all *generated* from that schema. A vendor defines DPs in a console
and gets a working app without writing app code.

**The `Field` enum in [`SystemState.h`](../../lib/SystemState/SystemState.h) is
already a DP table.** 19 typed entries, with per-consumer change detection
(`Consumer::CLOUD_CONSUMER`, `hasChanged()`, `markSeen()`) already built.
Formalising it into a versioned schema is a small step and the correct first one.

### Pump DP set

| DP | type | access | source in `SystemState` |
|---|---|---|---|
| `tank_level` | int 0–100 % | ro | `tankLevelPct` |
| `tank_stale` | bool | ro | `tankStale` |
| `tank_temp` | int ×10 °C | ro | `tankTempC` |
| `pump` | bool | rw | `pumpState` / `pumpRequest` |
| `mode` | enum AUTO,MANUAL | rw | `mode` |
| `bypass` | bool | rw | `bypass` |
| `voltage`, `current`, `power` | int ×10 | ro | `PowerMeter` |
| `energy_24h`, `runtime_24h`, `cycles_24h` | int | ro | `PowerStats` |
| `timer_total`, `timer_run`, `timer_break` | int sec | rw | `PumpTimer` |
| `faults` | bitmap | ro | `powerFault`, `pumpFault`, `tankSensorFault` |
| `tank_full_mm`, `tank_empty_mm` | int mm | rw | `ConfigStore` |

`rw` DPs arrive as an `ActionRequest` in `SystemState` and **never touch a driver
directly** — the rule already written into the Requests section of
`SystemState.h`. A remote command and a button press become indistinguishable
downstream, which is exactly what keeps the `PumpDriver` interlocks authoritative
for both.

---

## Decisions

| Area | Decision |
|---|---|
| Broker | EMQX OSS, self-hosted |
| Backend | Go + Postgres/TimescaleDB; service framework deferred |
| Region | Mumbai (`ap-south-1`) — latency for BD/IN/NP/PK, India DPDP residency |
| App read path | MQTT over WSS to the same broker |
| App write path | REST to the Go API, not direct MQTT publish |
| LAN path | Built in Phase A, not deferred |
| LAN transport | `POST /cmd` over HTTP + WebSocket pushing full state snapshots |
| Device identity | Per-device secret in NVS, TLS server-verify, root CA pinned |
| ThingsBoard | Not adopted — fights the DP model; revisit only if speed beats fit |

---

## The irreversible part: the wire contract

Consoles, apps and brokers get rewritten. Deployed firmware does not. Freeze this
early and keep the envelope generic — nothing pump-specific in it.

```
dn/{did}/cmd    cloud→device  {mid, dps:{...}}              QoS1
up/{did}/ack    device→cloud  {mid, ok, err?}
up/{did}/state  device→cloud  {dps:{...}}  sparse delta
up/{did}/hb     device→cloud  {rssi, up, fw, heap}          60s
up/{did}/evt    device→cloud  {code, ...}                   faults
dn/{did}/ota    cloud→device  {url, ver, sha256, sig}
```

Non-negotiable from packet one:

- **`mid` on every command, with an explicit ack.** A pump-on cannot be blindly retried.
- **Sparse deltas**, not full state dumps.
- **LWT** for offline detection.
- **Version byte**, so JSON can become CBOR later.
- **`fw` in the heartbeat**, so OTA targeting works before OTA exists.

Everything else is changeable later.

---

## Dual-path commands (cloud + LAN)

### Do not detect "same Wi-Fi" — race both paths

Detecting whether the phone and device share a network is a trap. Android needs
location permission to read the SSID and still often returns `<unknown ssid>`;
iOS needs a narrowly-granted entitlement. Worse, the same SSID does not imply
reachability (AP isolation, guest VLANs) and a different SSID does not imply
unreachability (2.4/5 GHz split names).

The `mid` needed for acks solves this outright. Commands are idempotent by `mid`,
so sending on both paths at once is safe:

```
app → LAN POST   cmd(mid=X)   (400ms timeout)
app → cloud REST cmd(mid=X)   (parallel)

device: seen mid=X? → replay cached ack, do nothing
        new mid?    → enqueue once, execute once, ack both
```

First path to land wins. No permissions, no SSID logic, and no dependence on mDNS
reliability for correctness. LAN only makes it faster and outage-proof.

**Dedupe is mandatory, not an optimisation.** A double `pump=on` is harmless; a
double `timer_start(30min)` restarts the timer. The device keeps a 16-entry ring
of `{mid, ack, ts}` over a 5-minute window.

### The device is the only writer of cloud truth

The app must **not** update the server after a LAN command. Two writers means
split-brain the first time one leg fails.

```
app ──LAN cmd──▶ device ──applies──▶ SystemState ──▶ publishes delta ──▶ cloud
```

LAN is a **command transport only, never a state-sync path.** If the cloud is
unreachable when a LAN command lands, the device buffers the delta in a RAM ring
and publishes on reconnect — which is already how a `CloudClient` gated on
`hasChanged(Consumer::CLOUD_CONSUMER, …)` behaves.

`LocalApi` is therefore just another writer of request fields, exactly like
`InputManager` and `CloudClient`. Nothing reaches sideways.

### Discovery

The device advertises `_pumpctl._tcp.local` over mDNS, with TXT records `did`,
`pid`, `fw`. Android needs a held `MulticastLock` or the browse silently returns
nothing, and cheap routers drop multicast — so the app also **caches the
last-known IP per SSID** and tries it directly in parallel. Cache hit is the
common case, mDNS is the cold-start fallback, and neither is load-bearing because
the cloud path is racing anyway.

### LAN authentication

Anyone on the Wi-Fi can reach the device's HTTP server. An unauthenticated "turn
on pump" endpoint on a mains relay is not acceptable, so the local path needs its
own auth — it cannot inherit the cloud's.

At claim time the cloud hands the app a per-device **local key**, derived from the
device secret. Every LAN request carries `HMAC(localKey, mid | dps | timestamp)`.
The device verifies the HMAC, requires the timestamp to fall within a ~60s window,
and dedupes on `mid`. That gives authenticated, non-replayable commands.

Two consequences:

1. **Plain HTTP + HMAC is authentic but not confidential.** A LAN sniffer can read
   tank level and pump state. Those are not secrets, and this avoids per-device
   TLS certificates, which are genuinely painful to provision and rotate on
   ESP32. If confidentiality is ever needed, issue each device a cert from your
   own CA at claim time and upgrade the local path to HTTPS.
2. **LAN control stays disabled until the device is claimed.** Otherwise a
   factory-fresh device on a shop or shared Wi-Fi answers to anyone who finds it.

### State on the LAN path — WebSocket push

The device is the server and the phone is the client. Nothing is hosted anywhere
else, and the cloud is not in this path at all — the WebSocket runs on the ESP32
itself, on the same `esp_http_server` and the same port as `POST /cmd`.

```
GET /ws       HMAC-signed handshake (over a server nonce)
              on connect  → full state snapshot
              on change   → full state snapshot, coalesced max 1 per 250ms
              every 10s   → full state snapshot (heartbeat)

POST /cmd     HMAC + mid → 202 {mid}        commands never travel over the WS
```

**Full snapshots, not deltas.** Deltas are what make push complicated: one missed
frame and the app's view silently diverges, which then needs resync logic. A full
snapshot per frame is complete truth every time — identical semantics to a poll
response, just pushed. The state JSON is ~300 bytes, less than the HTTP headers
polling would have added. App reconnect stays trivial: socket drops → reconnect
with backoff → first frame is a full snapshot, with no resync path to write.

**Chosen over 1s polling.** Polling was considered on the grounds that it has no
connection lifecycle to manage, but that is not true in practice — HTTP/1.1
keep-alive holds a socket open per phone anyway, with request and response headers
added on top.

| | Poll 1s | WebSocket |
|---|---|---|
| Idle (pump off, panel open) | ~650 B/s forever | zero |
| Active (pump running) | ~650 B/s | ~300 B/s |
| Latency to see a change | 0–1000ms | <100ms |
| Change from another phone or the box's buttons | up to 1s late | instant |

The last row decides it. Pump state changes from three places — this app, another
phone, and the physical buttons — and a control UI lagging a second behind the
hardware reads as broken.

**Dead-peer reaping is configuration, not application code:**

```c
config.keep_alive_enable   = true;
config.keep_alive_idle     = 15;   // s
config.keep_alive_interval = 5;
config.keep_alive_count    = 3;    // dead peer dropped in ~30s by the TCP stack
config.lru_purge_enable    = true;
config.max_open_sockets    = 7;
```

**Cap WS clients at 4.** LWIP defaults to 10 sockets, and the device also needs
MQTT (1), an OTA download (1, transient), the HTTP listener (1) and mDNS (UDP).
Leaving the client count uncapped is how the socket budget gets exhausted.

Use ESP-IDF's `esp_http_server` rather than `ESPAsyncWebServer`. Arduino-ESP32
sits on IDF anyway, WS support has been native since IDF 4.2, and the async server
has a long record of stability problems on long-running devices.

### Why not an on-device MQTT broker

Considered and rejected. Memory is not the blocker on an S3 with 8MB PSRAM; the
real costs are:

- It is a **second stack in the opposite role** — an MQTT client to EMQX *and* an
  MQTT broker for phones. More firmware, not less.
- **Authorization would live on the MCU.** MQTT authenticates per connection; the
  LAN scheme authenticates per command. Embedded brokers have no meaningful ACL
  layer, so broker-side authz would be hand-written on the device holding the
  mains relay.
- **It reintroduces the certificate problem** — MQTT sends a password at CONNECT,
  so avoiding clear-text credentials means TLS on the local broker.
- **It adds an app transport** rather than removing one (cloud WSS + cloud REST +
  LAN MQTT = three).
- Socket budget, and half-dead session cleanup on a device that must run for months.

For reference, Tuya's own LAN protocol is not a broker either — it is TCP
request/response with AES on port 6668, plus UDP broadcast discovery.

---

## Firmware impact

### Dual-core split

The loop is flat and single-threaded, and `PowerMeter` samples the ADC on a
timing-sensitive cadence. A TLS handshake blocks for hundreds of milliseconds and
will corrupt RMS windows. `mbedTLS` can be driven incrementally, but not through
`PubSubClient` without pain. The S3 is dual-core, so:

```
core 1 (control)          core 0 (network)
─────────────────         ────────────────────────
InputManager              WifiManager
RadioReceiver             CloudClient (MQTT+TLS)
PowerMeter                LocalApi (HTTP + WS + mDNS)
PumpTimer
SceneEngine        ◀──── cmdQueue  ────┐
PumpDriver         ────▶ deltaQueue ───┘
DisplayUI
```

**`SystemState` stays single-threaded on core 1.** Networking never touches it.
Two small FreeRTOS queues cross the boundary: inbound `{mid, src, dps}`, outbound
DP deltas. No mutex, no struct shared across cores, and no new failure mode in the
control path. `hasChanged(Consumer::LAN_CONSUMER, …)` runs on core 1 and feeds
`deltaQueue`; core 0 fans out to the WebSocket clients.

`LocalApi` returns `202 {mid}` immediately and never waits for `SceneEngine` to
drain, so rule 1 — no `delay()` outside `setup()` — survives intact.

### Module changes

| Module | Change |
|---|---|
| `lib/WifiManager/` | Non-blocking connect/reconnect with backoff; writes `wifiConnected`, `wifiRssi` |
| `lib/CloudClient/` | MQTT+TLS client; publishes DP deltas via `Consumer::CLOUD_CONSUMER`, drains `dn/cmd` into `cmdQueue` |
| `lib/LocalApi/` | **new** — `esp_http_server`, HMAC verify, WS snapshot push, mDNS advertise |
| `lib/SystemState/` | Add `Consumer::LAN_CONSUMER` to gate WS pushes; replace the single `pumpRequest` write point with a small command inbox/outbox — the same handoff pattern as the existing `configDirty` flag |
| `lib/SceneEngine/` | Drain the command inbox instead of reading one `ActionRequest` field |

---

## Cloud shape

```
ESP32 ──TLS/MQTT──▶ EMQX ──rule engine──▶ ingest svc ──▶ Postgres+TimescaleDB
                      ▲                                      │
                      │ (MQTT over WSS, user-scoped ACL)     │
                   Flutter app ◀────── REST/WS API ◀─────────┘
                                            └──▶ FCM / APNs
```

- **EMQX**: HTTP auth hook → Go auth service validates device credentials and
  returns the topic ACL. Devices get publish-only on `up/${clientid}/#` and
  subscribe-only on `dn/${clientid}/#`. App users get subscribe-only on devices
  they own — that needs a per-topic ownership lookup on every connect and
  subscribe, so cache it hard.
- **Asymmetric app paths.** Read over WSS (cheap fan-out, nothing to scale); write
  over REST, so commands get authorization, audit, rate limiting and `mid`
  assignment. Topic ACLs alone cannot express "may set `pump` but not
  `tank_full_mm`", which a platform needs.
- **Postgres** for users, homes, devices, products/DP schema, automations and OTA
  campaigns, plus the device shadow as JSONB. **TimescaleDB in the same Postgres**
  for telemetry — no second database until forced. Store on-change plus rollups;
  never store the heartbeat stream.
- **Shadow semantics**: desired vs reported. A command to an offline device writes
  desired and delivers on reconnect (QoS1, `clean_session=false`) — essential
  where power cuts are routine. Reported state is authoritative; version the
  desired-writes so a stale desired cannot resurrect.

### Provisioning and claiming

1. At flash time each device gets a `deviceId` + secret in a dedicated NVS
   partition. Flash encryption and NVS encryption before units leave your hands;
   not needed for the two on the bench.
2. The app requests a **claim token** from the cloud, bound to the user.
3. App → device over BLE: Wi-Fi SSID/password **plus** the claim token.
4. The device connects, authenticates with its own credential, and presents the token.
5. The cloud binds device → user's home. Unclaimable by anyone else until factory reset.

Without steps 2–4, whoever scans first owns the device.

### Cost sanity check

One pump ≈ 1.5k msg/day ≈ 45k/month. 10k devices ≈ 450M msg/month ≈ **170 msg/s**
— unremarkable for EMQX on a €8/mo Hetzner box. The same load on AWS IoT Core is
roughly $450/mo. Self-hosting wins by around 50×, and at the target price point
for these markets that gap is the business.

---

## Staging

- **0 (interim)** — one device live on Blynk in about a week, so the hardware can
  be watched over days while Phase A is built. Disposable firmware module, no
  effect on the wire contract. See [blynk-interim.md](blynk-interim.md).
- **A (now)** — pump end to end: EMQX + a thin Go API + Postgres + a Flutter
  panel, LAN path included. Schema-driven from day one, but only one product
  hardcoded.
- **B** — multi-device: homes, rooms, sharing, a multi-product DP registry,
  automations, push alerts.
- **C** — platform: developer console, DP designer, flashable SDK, OTA campaigns,
  white-label app, billing (bKash / SSLCommerz).

Do not build C first. Do make A's *wire contract* C-shaped.

---

## Verification

1. **Wire contract before firmware.** Drive a fake device with `mosquitto_pub`
   against EMQX and the Go ingest service. Confirm the shadow updates, the ack
   round-trips, and that an unknown `mid` executes exactly once while a repeat
   replays the cached ack.
2. **Safety first, on the bench, with no mains connected.** Send `pump=on` from
   the app while `tank_stale=true` and confirm `PumpDriver` refuses. Remote
   commands must hit the same interlocks as a button press — that is the entire
   point of routing them through `SystemState`.
3. **Dual-path race.** Turn off mobile data and confirm the LAN command lands.
   Turn off the router's WAN and confirm LAN still works and the delta reaches the
   cloud on reconnect. Send both paths simultaneously and confirm one execution.
4. **Timing regression.** Compare `PowerMeter` RMS output before and after moving
   networking to core 0, with a TLS handshake forced mid-sample.
5. **WebSocket lifecycle.** Connect 4 phones and kill Wi-Fi on two without closing
   cleanly; confirm the TCP keepalive drops them within ~30s and heap returns to
   baseline. Leave one connected for 24h and watch for drift. Attempt a 5th client
   and confirm it is refused rather than starving MQTT of a socket.
6. **Claiming.** Attempt to claim an already-claimed device from a second account
   and confirm refusal.

---

## Open questions

Not settled in the design session, and worth resolving before Phase A starts:

- **DP schema versioning** — what happens when a product adds a DP while old
  firmware is still in the field.
- **Flutter panel renderer** — how a DP list becomes a working app screen with no
  per-product app code. This is the platform's real product surface.
- **OTA design** — firmware signing, rollout targeting by product + version +
  percentage, dual-partition apply, automatic rollback.
- **Go backend layout** — repo structure, the auth/ingest/api split, EMQX hook
  contracts, and the Postgres + Timescale schema for shadow and telemetry.

---

## Prerequisite

Phase 4's AUTO logic and the remaining `PumpDriver` interlocks are still unwritten,
and nothing has been confirmed on hardware since the pump timer landed. Remote
control lands *on top of* those interlocks. Close that gap before the cloud is
allowed to turn a relay.
