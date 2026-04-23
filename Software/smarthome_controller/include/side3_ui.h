/**
 * @file side3_ui.h
 * @brief Declares UI functions for the time and weather view.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

#include <lvgl.h>

namespace side3 {

// Build the Side 3 screen on the given parent
void build_screen(lv_obj_t *parent);

// Update time and date display
void set_time(int hour, int minute, int second);
void set_date(int day, int month, int year);

// Update weather information
void set_weather(const char *condition, int temp_c);

// Set disconnected state when weather/time unavailable
void set_disconnected();

// Update the arc loader progress (0-100%)
void update_arc_progress(int progress_percent);

}  // namespace side3
