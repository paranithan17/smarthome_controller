#pragma once

#include <cstdint>

namespace app_controls {

void init();
void update();
void toggle_lamp();
bool set_lamp(bool on);
void sync_state_to_ui();

// Returns milliseconds since last interaction (lamp button or joystick movement)
uint32_t get_ms_since_last_interaction();

// Reset the interaction timer (used by UI to track when user returns to Side 1)
void reset_interaction_timer();

}  // namespace app_controls