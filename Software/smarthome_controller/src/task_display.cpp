/**
 * @file task_display.cpp
 * @brief FreeRTOS task that owns all LVGL screen updates and display state transitions.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>

#include "app_controls.h"
#include "app_runtime.h"
#include "lvgl_port.h"
#include "side1_ui.h"
#include "side3_ui.h"

namespace app_tasks {

namespace {

constexpr uint32_t idleTimeoutMs = 10000;
constexpr uint32_t autoSwitchIntervalMs = 5000;
constexpr uint32_t arcUpdateIntervalMs = 100;

}  // namespace

void display_ui(void *arg) {
  (void)arg;

  // Start from the main control page on task boot.
  app_runtime::switch_to_page(0);

  uint32_t last_ui_update_ms = 0;

  for (;;) {
    // Consume UI change flags without blocking; clear bits we read.
    const EventBits_t changed = xEventGroupWaitBits(app_runtime::ui_change_events(),
                            app_runtime::uiLampChangedBit | app_runtime::uiLedChangedBit,
                            pdTRUE,
                            pdFALSE,
                            0);

    // Any control interaction while on weather/time page returns to main page.
    if (changed != 0U && app_runtime::current_page() == 1) {
      app_runtime::switch_to_page(0);
      app_runtime::set_auto_cycling_enabled(false);
      app_controls::reset_interaction_timer();
    }

    // Push latest lamp state snapshot to side 1 widgets.
    const EventBits_t light_bits = app_runtime::read_light_state_bits();
    side1::set_lamp_state((light_bits & app_runtime::lampStateBit) != 0U);

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t active_channel = 0;
    app_controls::get_led_state(red, green, blue, active_channel);
    // Keep RGB controls in sync with joystick-driven LED state.
    side1::set_rgb_state(red, green, blue, active_channel);

    const app_runtime::WeatherShared weather_state = []() {
      app_runtime::WeatherShared state = {"--", 0, false};
      (void)app_runtime::read_weather_state(state);
      return state;
    }();

    // Show weather data when valid, otherwise display disconnected state.
    if (weather_state.valid) {
      side3::set_weather(weather_state.condition, weather_state.temp_c);
    } else {
      side3::set_disconnected();
    }

    // Enable auto page cycling after user inactivity timeout.
    app_runtime::set_auto_cycling_enabled(app_controls::get_ms_since_last_interaction() >= idleTimeoutMs);

    // Rotate between pages only when auto-cycling is active.
    if (app_runtime::current_page() == 0 && app_runtime::auto_cycling_enabled()) {
      if (app_runtime::page_switch_age_ms() >= autoSwitchIntervalMs) {
        const uint8_t next_page = static_cast<uint8_t>((app_runtime::current_page() + 1U) % 2U);
        app_runtime::switch_to_page(next_page);
      }
    }

    const uint32_t now = millis();
    // Advance arc animations at a fixed cadence independent of event traffic.
    if (now - last_ui_update_ms >= arcUpdateIntervalMs) {
      app_runtime::update_arc_loaders();
      last_ui_update_ms = now;
    }

    app_runtime::TimeMessage time_msg;
    // Apply latest queued time sample to side 3 clock/date labels.
    if (xQueueReceive(app_runtime::time_queue(), &time_msg, 0) == pdTRUE) {
      side3::set_time(time_msg.hour, time_msg.minute, time_msg.second);
      side3::set_date(time_msg.day, time_msg.month, time_msg.year);
    }

    // LVGL must be pumped frequently from this single owner task.
    lvgl_port::task_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace app_tasks
