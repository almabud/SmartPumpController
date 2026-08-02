// lib/SystemState/SystemState.h
#pragma once
#include <stdint.h>
#include "config.h"   // tank calibration defaults

// ---- Enumerations --------------------------------------------------------

enum class OperatingMode : uint8_t {
    AUTO,       // SceneEngine drives the pump based on tank level + scenes
    MANUAL      // buttons or remote commands drive the pump directly
};

enum class PumpState : uint8_t {
    OFF,
    ON
};

enum class ActionRequest : uint8_t {
    NONE,
    TURN_ON,
    TURN_OFF
};

enum class TimerRequest : uint8_t {
    NONE,
    START,      // arm the timer with the timerTotalSec/RunSec/BreakSec below
    CANCEL      // stop the timer and the pump with it
};

enum class TimerPhase : uint8_t {
    IDLE,       // no timer armed
    RUNNING,    // inside a RUN slice — pump should be on
    BREAKING    // inside a BREAK slice — pump should be off
};

// ---- Change detection consumers ------------------------------------------
// Each consumer tracks its own "last seen" snapshot independently.
// DisplayUI and CloudClient never interfere with each other's change detection.

enum class Consumer : uint8_t {
    DISPLAY_CONSUMER,
    CLOUD_CONSUMER,
    _COUNT
};

// ---- Field identifiers ---------------------------------------------------
// Used for field-specific change detection: state.hasChanged(Consumer::CLOUD, Field::TANK_LEVEL)

enum class Field : uint8_t {
    TANK_LEVEL,
    TANK_STALE,
    TANK_TEMP,
    TANK_SENSOR_FAULT,
    PUMP_STATE,
    OPERATING_MODE,
    VOLTAGE,
    CURRENT,
    POWER_WATTS,
    ENERGY_KWH,
    FREQUENCY,
    POWER_FAULT,
    PUMP_FAULT,
    WIFI_CONNECTED,
    CLOUD_CONNECTED,
    ACTIVE_SCENE,
    UPTIME,
    PUMP_TIMER
};

// ---- Snapshot struct (previous state per consumer) -----------------------
// One snapshot is kept per consumer. markSeen() copies current state into
// the snapshot; hasChanged() compares current state against it.

struct StateSnapshot {
    uint8_t       tankLevelPct    = 255;    // 255 = never seen, forces first draw
    bool          tankStale       = false;
    float         tankTempC       = -1.0f;
    bool          tankSensorFault = false;
    bool          tankTempFault   = false;
    PumpState     pumpState       = PumpState::OFF;
    OperatingMode mode            = OperatingMode::AUTO;
    float         voltage         = -1.0f;
    float         current         = -1.0f;
    float         powerWatts      = -1.0f;
    float         energyKwh       = -1.0f;
    float         frequency       = -1.0f;
    bool          powerFault      = false;
    bool          pumpFault       = false;
    bool          wifiConnected   = false;
    bool          cloudConnected  = false;
    uint8_t       activeSceneId   = 255;
    uint32_t      uptimeSeconds   = 0;
    TimerPhase    timerPhase      = TimerPhase::IDLE;
    uint32_t      timerRemainSec  = 0;
};

// ---- SystemState ---------------------------------------------------------
// The single source of truth for the whole system.
// Modules that sense something WRITE to the public fields directly.
// Modules that act READ from the public fields.
// No module calls another module directly — everything goes through here.

class SystemState {
public:

    // ---- Tank / radio ----------------------------------------------------
    // Written by RadioReceiver. Nothing is known until a packet lands, so the
    // defaults must read as "no data", not as a plausible tank.
    uint8_t  tankLevelPct     = 0;      // 0-100% tank fill level
    uint32_t lastPacketTimeMs = 0;      // millis() timestamp of last valid radio packet
    bool     tankStale        = true;   // true until the first valid packet arrives
    float    tankTempC        = 0.0f;   // water temperature (C), from tank sensor packet
    uint8_t  tankSeq          = 0;      // seq of the last accepted packet, for drop detection

    // Faults the node reports in-band. Distinct from tankStale: packets are
    // arriving fine, it is the sensor behind them that is unhappy.
    bool     tankSensorFault  = false;  // no echo, or echo outside the valid window
    bool     tankTempFault    = false;  // DS18B20 unreadable; tankTempC is the node's fallback

    // Calibration — the distances the sensor reports at the two extremes.
    // Seeded from config.h, user-editable from the config page in a later plan.
    uint16_t tankFullMm       = TANK_DISTANCE_FULL_MM;
    uint16_t tankEmptyMm      = TANK_DISTANCE_EMPTY_MM;

    // ---- Power monitoring ------------------------------------------------
    float    voltage          = 0.0f;   // mains voltage (V RMS)
    float    current          = 0.0f;   // load current (A RMS)
    float    powerWatts       = 0.0f;   // active power (W)
    float    energyKwh        = 0.0f;   // accumulated energy (kWh)
    float    frequency        = 0.0f;   // mains frequency (Hz)
    bool     powerFault       = false;  // true if voltage/current out of safe range

    // ---- Connectivity ----------------------------------------------------
    bool     wifiConnected    = false;  // true when Wi-Fi is connected
    bool     cloudConnected   = false;  // true when MQTT broker is connected
    int8_t   wifiRssi         = 0;

    // ---- Mode / control --------------------------------------------------
    OperatingMode mode            = OperatingMode::AUTO;
    PumpState     pumpState       = PumpState::OFF;

    // ---- Requests --------------------------------------------------------
    // Written by InputManager (local button) or CloudClient (remote app).
    // SceneEngine consumes and clears each cycle.
    ActionRequest pumpRequest     = ActionRequest::NONE;

    // ---- Pump timer ------------------------------------------------------
    // Written by DisplayUI (the edit UI) and InputManager (cancel on long press),
    // consumed by PumpTimer.
    // The duty cycle is a run slice followed by a break slice, repeating until
    // the window runs out — the last one is simply cut short, the cycle does
    // not have to divide the window. Both zero means run straight through.
    TimerRequest timerRequest   = TimerRequest::NONE;
    uint32_t     timerTotalSec  = 0;    // total window, 0 = nothing armed
    uint32_t     timerBreakSec  = 0;    // how long the pump breaks for
    uint32_t     timerRunSec    = 0;    // how long it runs between breaks

    // Written by DisplayUI, read by InputManager: while the timer is being
    // edited the buttons belong to the edit UI, so a long press must not reach
    // through to the pump or a running timer.
    bool         uiEditing      = false;

    // Written by PumpTimer, read by DisplayUI.
    TimerPhase   timerPhase     = TimerPhase::IDLE;
    uint32_t     timerRemainSec = 0;    // seconds left in the whole window
    uint32_t     timerPhaseSec  = 0;    // seconds left in the current slice

    // ---- Scene engine output (read by PumpDriver) ------------------------
    ActionRequest desiredPumpAction = ActionRequest::NONE;
    uint8_t       activeSceneId     = 0;    // ID of the currently active scene

    // ---- Fault flags -----------------------------------------------------
    // Set by drivers, read by DisplayUI and CloudClient.
    bool     pumpFault        = false;  // true if PumpDriver detected a fault

    // ---- Uptime ----------------------------------------------------------
    uint32_t uptimeSeconds    = 0;      // seconds since boot, written by main loop

    // ---- Change detection API --------------------------------------------

    // Has ANY field changed since this consumer last called markSeen()?
    bool hasChanged(Consumer consumer) const;

    // Has a SPECIFIC field changed since this consumer last called markSeen()?
    bool hasChanged(Consumer consumer, Field field) const;

    // Snapshot current state for this consumer.
    // Call after drawing (DisplayUI) or after publishing (CloudClient).
    void markSeen(Consumer consumer);

private:
    // One independent snapshot per consumer
    StateSnapshot _snapshots[static_cast<uint8_t>(Consumer::_COUNT)];
};