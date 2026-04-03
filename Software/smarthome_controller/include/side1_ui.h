#pragma once

#include <stdint.h>

#include <lvgl.h>

namespace side1 {

// Builds the Side 1 layout into the given parent object.
void build_screen(lv_obj_t *parent);

void set_lamp_state(bool on);
void set_rgb_channel_value(uint8_t index, uint8_t value);
void set_active_channel(uint8_t index);
void set_rgb_state(uint8_t red, uint8_t green, uint8_t blue, uint8_t active_index);

// Update the arc loader progress (0-100%)
void update_arc_progress(int progress_percent);

}  // namespace side1
