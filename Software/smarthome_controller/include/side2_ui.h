#pragma once

#include <stdint.h>

#include <lvgl.h>

namespace side2 {

// Builds the Side 2 layout into the given parent object.
void build_screen(lv_obj_t *parent);

// Updates the displayed room readings.
void set_readings(int16_t temperature_c, uint8_t humidity_percent);

// Shows placeholder values when the sensor is unavailable.
void set_sensor_disconnected();

// Update the arc loader progress (0-100%)
void update_arc_progress(int progress_percent);

}  // namespace side2
