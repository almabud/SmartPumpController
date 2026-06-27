// pump-controller/lib/SystemState/SystemState.h
//
// The single source of truth for the whole system. Every module either
// WRITES into this (sensing modules: RadioReceiver, PowerMeter, InputManager,
// WifiManager, CloudClient) or READS from it (acting modules: SceneEngine,
// PumpDriver, DisplayUI). No module calls another module directly —
// everything is mediated through this struct.
//
// Header-only: no logic lives here, only data.

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

// A request from any source (button, scene engine, or remote command via
// CloudClient). SceneEngine consumes these and produces desired actions.
// Drivers re-validate against safety rules regardless of request source.
enum class ActionRequest : uint8_t {
    NONE,
    TURN_ON,
    TURN_OFF
};

// ---- SystemState struct --------------------------------------------------

struct SystemState {

    // ---- Tank / radio ----------------------------------------------------
    uint8_t  tankLevelPct     = 0;      // 0-100%
    uint32_t lastPacketTimeMs = 0;      // millis() timestamp of last valid packet
    bool     tankStale        = true;   // true until first valid packet received

    // ---- Power monitoring ------------------------------------------------
    float    voltage          = 0.0f;   // mains voltage (V RMS)
    float    current          = 0.0f;   // load current (A RMS)
    float    powerWatts       = 0.0f;   // active power (W)
    float    energyKwh        = 0.0f;   // accumulated energy (kWh)
    float    frequency        = 0.0f;   // mains frequency (Hz)
    bool     powerFault       = false;

    // ---- Connectivity ----------------------------------------------------
    bool     wifiConnected    = false;
    bool     cloudConnected   = false;

    // ---- Mode / control --------------------------------------------------
    OperatingMode mode            = OperatingMode::AUTO;
    PumpState     pumpState       = PumpState::OFF;

    // ---- Requests --------------------------------------------------------
    // Written by InputManager (local) or CloudClient (remote).
    // SceneEngine consumes and clears each cycle.
    ActionRequest pumpRequest     = ActionRequest::NONE;

    // ---- Scene engine output (read by PumpDriver) ------------------------
    ActionRequest desiredPumpAction = ActionRequest::NONE;
    uint8_t       activeSceneId     = 0;

    // ---- Fault flags -----------------------------------------------------
    bool     pumpFault        = false;

    // ---- Uptime ----------------------------------------------------------
    uint32_t uptimeSeconds    = 0;
};