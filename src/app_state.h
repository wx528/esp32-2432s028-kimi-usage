#pragma once
#include <stdint.h>

enum AppState : uint8_t {
  STATE_BOOT = 0,
  STATE_PORTAL,
  STATE_CONNECTING,
  STATE_RUNNING
};
