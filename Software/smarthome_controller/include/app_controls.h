/**
 * @file app_controls.h
 * @brief Provides input handling and actuator control APIs for lamp and RGB LED hardware.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace app_controls {

void init();
void register_task_handles(TaskHandle_t lamp_task, TaskHandle_t led_task);

bool process_lamp_button_event();
bool process_joystick_button_event();
bool process_joystick_axis_event();

void toggle_lamp();
bool set_lamp(bool on);
bool get_lamp_state();

void get_led_state(uint8_t &red, uint8_t &green, uint8_t &blue, uint8_t &active_channel);

// Receives the latest pending lamp command from UI/input control.
bool receive_lamp_command(bool &desired_on, TickType_t wait_ticks);

// Sends lamp relay HTTP request to Shelly for the given target state.
bool send_lamp_http(bool on);

// Returns milliseconds since last interaction (lamp button or joystick movement)
uint32_t get_ms_since_last_interaction();

// Reset the interaction timer (used by UI to track when user returns to Side 1)
void reset_interaction_timer();

}  // namespace app_controls