#pragma once
#include <Arduino.h>
#include "SystemState.h"

class SceneEngine {
public:
    void update(SystemState& state);
};