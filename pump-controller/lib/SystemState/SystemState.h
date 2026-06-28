// lib/SystemState/SystemState.h
#pragma once
#include <stdint.h>

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
    UPTIME
};

// ---- Snapshot struct (previous state per consumer) -----------------------
// One snapshot is kept per consumer. markSeen() copies current state into
// the snapshot; hasChanged() compares current state against it.

struct StateSnapshot {
    uint8_t       tankLevelPct    = 255;    // 255 = never seen, forces first draw
    bool          tankStale       = false;
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
};

// ---- SystemState ---------------------------------------------------------
// The single source of truth for the whole system.
// Modules that sense something WRITE to the public fields directly.
// Modules that act READ from the public fields.
// No module calls another module directly — everything goes through here.

class SystemState {
public:

    // ---- Tank / radio ----------------------------------------------------
    uint8_t  tankLevelPct     = 0;      // 0-100% tank fill level
    uint32_t lastPacketTimeMs = 0;      // millis() timestamp of last valid radio packet
    bool     tankStale        = true;   // true until first valid packet received

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