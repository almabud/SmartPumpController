# Mobile App Architecture

> **Status: design only. Nothing here is implemented.** No app project exists yet.
> Companion to [cloud-architecture.md](cloud-architecture.md) — that document owns
> the device, wire contract and cloud; this one owns the phone.

---

## Decision

**Build our own app.** Blynk was evaluated as a shortcut and rejected as a
foundation — it cannot be self-hosted or white-labelled, so building on it means
being a Blynk reseller rather than a platform. It is kept as **reference material
only**: its widget vocabulary and widget→type compatibility matrix are years of
design work available for free.

Blynk is however used as a **temporary interim path** to get one device live months
before this app exists — see [blynk-interim.md](blynk-interim.md). That is a
disposable firmware module, not a foundation, and it does not touch the wire
contract. This document remains the destination.

---

## Two layers, kept separate

| Layer | What it is | Source of truth |
|---|---|---|
| **DP schema** | what data exists — type, range, unit, access | the device / product definition |
| **Panel layout** | how it is presented — widget, binding, order, labels | the product's layout file |

The layout references DP ids; it never redefines them. One device can then have
several layouts (full panel, compact tile, dashboard card) with no device change.

### Panel layout JSON

```json
{
  "productId": "pump-v1",
  "layoutVersion": 3,
  "minAppVersion": "1.2.0",
  "sections": [
    {"title": "Tank", "widgets": [
      {"type": "tank_fill",  "dp": "tank_level", "min": 0, "max": 100, "unit": "%"},
      {"type": "badge",      "dp": "tank_stale", "trueLabel": "STALE", "severity": "warn"}
    ]},
    {"title": "Pump", "widgets": [
      {"type": "toggle_big", "dp": "pump", "confirm": true},
      {"type": "segmented",  "dp": "mode", "options": ["AUTO", "MANUAL"]}
    ]},
    {"title": "Power", "widgets": [
      {"type": "stat_row",   "dps": ["voltage", "current", "power"]},
      {"type": "chart_line", "dp": "power", "window": "24h"}
    ]}
  ]
}
```

### Widget vocabulary

Keep it around twelve. Each widget declares **which DP types it accepts** — a
toggle binds only to a bool, a slider only to an int with a range. That one
constraint makes the renderer safe (no invalid binding can exist) and makes a
future builder simple (the palette filters itself per DP).

`toggle` · `toggle_big` · `segmented` · `slider` · `stepper` · `gauge_radial` ·
`tank_fill` · `stat` / `stat_row` · `badge` · `chart_line` · `duration_editor` ·
`button`

That set covers the pump completely, including the timer.

**Unknown widget types and unknown DPs must be skipped and logged, never crash.**
An old app meeting a new layout is not an edge case, and neither is a new app
meeting firmware that lacks a DP.

---

## Flutter structure

```
DeviceStore     ← one per open device; fed by whichever transport is live
                  (cloud MQTT/WSS, or the device's LAN WebSocket)
CommandSink     → dual-path race, assigns mid, handles ack
WidgetFactory   Widget build(WidgetSpec, DeviceStore, CommandSink)
                  — a switch on spec.type; widgets know nothing about transport
```

Widgets bind to a listenable per DP. Transport selection, the LAN/cloud race and
`mid` dedupe all live in `CommandSink` — no widget ever knows which path its
command took.

### Why Flutter specifically

A future builder needs a **live preview**. Flutter Web compiles the *identical*
renderer into the web console, so WYSIWYG is guaranteed by construction.

This is not theoretical. Blynk's own documentation states that its web and mobile
widgets are configured separately, "with the Map widget being an exception due to
different codebases for mobile and web" — two renderers, and the drift already
leaks into user-visible behaviour. One renderer compiled to both targets avoids
the entire class of problem.

Secondary reasons: strongest BLE story (`flutter_blue_plus`), and custom-painted
gauges are straightforward.

---

## Build order

**Transports and BLE first. Widgets last.** Widgets are the fun, low-risk part;
building them first hides the BLE provisioning problem until week six, and BLE is
where this kind of project actually slips.

| # | Step | Effort |
|---|---|---|
| 1 | `DeviceStore` + transports (cloud WSS, LAN WS) + `CommandSink` with dual-path `mid`. Prove it with a raw text-dump screen, no real UI | ~1 week |
| 2 | Auth, device list, **BLE provisioning + claiming** — the risky part, done early | 2 weeks ⚠️ |
| 3 | Renderer core: layout JSON parse, widget factory, graceful unknown handling | ~2 days |
| 4 | The seven widgets the pump needs: `tank_fill`, `toggle_big`, `segmented`, `stat_row`, `badge`, `chart_line`, `duration_editor` | 2 weeks |
| 5 | Offline states, error handling, theming, polish | ~1 week |

**~6–8 weeks for a usable v1**, solo, with Flutter familiarity. BLE is the
estimate most likely to be wrong.

### Schema-driven costs almost nothing extra

The widgets are the work, and they get written either way — hardcoding a pump
screen still means building a gauge, a toggle and a chart. The schema layer is a
JSON parser plus a `switch`, roughly **15–20% on top**.

Concretely: a hardcoded pump screen is about a week of UI; the same screen as a
renderer is about 2.5 weeks. That extra 1.5 weeks makes product #2 a JSON file
instead of another month. Schema-driven is close to free — provided the *visual
builder* stays deferred.

---

## Deferred, deliberately

- **Visual drag-and-drop builder** (4–6 weeks). It matters when third parties
  self-serve. There are no third parties yet. **Until then the builder is a text
  file**, and hand-authoring a layout is faster than clicking through a UI.
- **White-labelling.** Build only when a vendor is paying. Note that Apple
  (guidelines 4.2.6, 4.3) and Google push back on template-generated apps — the
  workable model is the vendor publishing under their own developer account, so
  the deliverable is a build they submit plus documentation, not a CI job that
  ships hundreds of near-identical apps from our account.

---

## Prior art

**Blynk** — the closest existing product, and effectively the thing being rebuilt
for a different market. Worth copying:

- Datastream = DP. Their schema is `{name, key, pin, datastreamType, default, units}`
  with types `INTEGER` / `DOUBLE` / `STRING` / `ENUM` / `LOCATION`.
- A published widget→datastream-type compatibility table — use it as the
  validation-rule spec.
- **Runtime-mutable datastream properties** (`.../update/property?pin=V1&isHidden=true`)
  that affect every bound widget. Lets presentation change without a layout
  redeploy — e.g. hiding a control on firmware that lacks the DP.
- Org-scoped permission vocabulary (`ORG_DEVICES_CONTROL`, `OWN_DEVICES_VIEW`) for
  the Phase B sharing model.

Worth *not* copying:

- **The pin abstraction.** Every Blynk datastream still carries
  `pinNumber` / `pinType: VIRTUAL|ANALOG|DIGITAL` — Arduino-era baggage they cannot
  shed. Our DPs are named keys; there is no pin concept.
- **Two renderers**, as above.

**ESP RainMaker** — Espressif's Tuya-like platform, Apache 2.0 device SDK with
**open-source iOS and Android apps**. Devices declare typed params and the app
renders its UI from that schema with no per-product code — the same architecture
described here, already implemented and readable. Worth studying before writing
the renderer. Its cloud is Espressif-hosted and ESP-only, with no white-labelling
or regional residency, which is where it stops being competition.

---

## Open questions

- **DP schema versioning** — what the app does when a layout references a DP the
  device's firmware does not have, and vice versa. Skipping gracefully is decided;
  what to *show* the user is not.
- **Chart data source** — history comes from the cloud (Timescale) and is
  unavailable on the LAN path. Decide whether charts simply disappear offline or
  the device buffers a short window itself.
- **Offline command UX** — what the user sees when both transports are down and a
  command cannot be delivered at all.
