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

/**
 * @brief Initialize GPIO, PWM, interrupts, timers, and RTOS state for controls.
 */
void init();
/**
 * @brief Register task handles so input ISRs can wake the correct RTOS tasks.
 *
 * @param lamp_task Lamp control task handle.
 * @param led_task LED control task handle.
 */
void register_task_handles(TaskHandle_t lamp_task, TaskHandle_t led_task);

/**
 * @brief Process a pending lamp button event from the input layer.
 *
 * @return true if a lamp button action changed state.
 */
bool process_lamp_button_event();
/**
 * @brief Process a pending joystick button event.
 *
 * @return true if the selected RGB channel advanced.
 */
bool process_joystick_button_event();
/**
 * @brief Process a pending joystick axis event.
 *
 * @return true if the active RGB channel value changed.
 */
bool process_joystick_axis_event();

/**
 * @brief Toggle the lamp state and enqueue a relay command if needed.
 */
void toggle_lamp();
/**
 * @brief Set lamp state and queue the desired relay target for the lamp task.
 *
 * @param on Desired lamp state.
 * @return true when the request was accepted.
 */
bool set_lamp(bool on);
/**
 * @brief Read the cached lamp state.
 *
 * @return true if lamp is on.
 */
bool get_lamp_state();

/**
 * @brief Read the cached RGB values and selected channel.
 *
 * @param[out] red Current red PWM value.
 * @param[out] green Current green PWM value.
 * @param[out] blue Current blue PWM value.
 * @param[out] active_channel Current selected channel index.
 */
void get_led_state(uint8_t &red, uint8_t &green, uint8_t &blue, uint8_t &active_channel);

/**
 * @brief Receive the latest pending lamp command from the control queue.
 *
 * @param[out] desired_on Desired relay state.
 * @param wait_ticks Maximum time to wait for a command.
 * @return true when a command was received.
 */
bool receive_lamp_command(bool &desired_on, TickType_t wait_ticks);

/**
 * @brief Send the Shelly relay HTTP request for the given lamp target state.
 *
 * @param on Desired relay state.
 * @return true when the HTTP request was issued successfully.
 */
bool send_lamp_http(bool on);

/**
 * @brief Get elapsed milliseconds since the last user interaction.
 *
 * @return Milliseconds since the interaction timer was reset.
 */
uint32_t get_ms_since_last_interaction();

/**
 * @brief Reset the interaction timer used by the UI auto-cycling logic.
 */
void reset_interaction_timer();

}  // namespace app_controls