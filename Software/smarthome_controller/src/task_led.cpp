/**
 * @file task_led.cpp
 * @brief FreeRTOS task that handles joystick input and RGB LED state updates.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>

#include "app_controls.h"
#include "app_runtime.h"

namespace app_tasks {

void led_control(void *arg) {
  (void)arg;

  for (;;) {
    // Block until another task notifies that new joystick input is ready.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    bool led_changed = false;
    // Button events can cycle/select channels.
    if (app_controls::process_joystick_button_event()) {
      led_changed = true;
    }
    // Axis events can adjust the currently selected channel value.
    if (app_controls::process_joystick_axis_event()) {
      led_changed = true;
    }

    if (led_changed) {
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      uint8_t active_channel = 0;
      app_controls::get_led_state(red, green, blue, active_channel);
      (void)red;
      (void)green;
      (void)blue;

      // Publish the selected channel and trigger a UI refresh.
      app_runtime::set_selected_rgb_channel(active_channel);
      app_runtime::signal_ui_change(app_runtime::uiLedChangedBit);
    }
  }
}

}  // namespace app_tasks
