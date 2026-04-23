/**
 * @file task_lamp.cpp
 * @brief FreeRTOS task that handles the lamp pushbutton and Shelly HTTP control.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>

#include "app_controls.h"
#include "app_runtime.h"

namespace app_tasks {

void lamp_control(void *arg) {
  (void)arg;

  for (;;) {
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

    bool lamp_changed = false;
    if (app_controls::process_lamp_button_event()) {
      lamp_changed = true;
    }

    bool desired_lamp_on = false;
    while (app_controls::receive_lamp_command(desired_lamp_on, 0)) {
      app_runtime::connect_to_wlan_if_needed();

      if (xSemaphoreTake(app_runtime::network_mutex(), pdMS_TO_TICKS(500)) == pdTRUE) {
        (void)app_controls::send_lamp_http(desired_lamp_on);
        xSemaphoreGive(app_runtime::network_mutex());
      }

      lamp_changed = true;
    }

    if (lamp_changed) {
      app_runtime::set_lamp_state_bit(app_controls::get_lamp_state());
      app_runtime::signal_ui_change(app_runtime::uiLampChangedBit);
    }
  }
}

}  // namespace app_tasks
