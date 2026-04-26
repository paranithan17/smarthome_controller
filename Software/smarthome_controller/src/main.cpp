/**
 * @file main.cpp
 * @brief RTOS bootstrap for the smart home controller project application.
 * @details This program was built as a project work for the course BTE5483
 * Real Time Operating System at Bern University of Applied Sciences during
 * the spring semester 2026. It was built with the help of AI assistance.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "app_controls.h"
#include "app_runtime.h"
#include "app_tasks.h"
#include "lvgl_port.h"

namespace {

// Period for the weather data fetch timer.
constexpr uint32_t weatherPeriodMs = 30UL * 60UL * 1000UL;  // 30 minutes

// Handles for all created FreeRTOS tasks.
TaskHandle_t s_lamp_task_handle = nullptr;
TaskHandle_t s_led_task_handle = nullptr;
TaskHandle_t s_display_task_handle = nullptr;
TaskHandle_t s_time_task_handle = nullptr;
TaskHandle_t s_weather_task_handle = nullptr;
TaskHandle_t s_runtime_stats_task_handle = nullptr;

app_tasks::RuntimeStatsTaskParams s_runtime_stats_task_params = {};

// Handle for the periodic weather fetch timer.
TimerHandle_t s_weather_timer = nullptr;

}  // namespace

/**
 * @brief Initializes the system, creates all application tasks, and starts the scheduler.
 * @details This function is called once on startup. It sets up serial communication,
 * initializes hardware drivers and software modules, creates all FreeRTOS tasks
 * with their respective priorities, and starts a periodic timer for weather updates.
 */
void setup() {
  // Start serial communication for debugging.
  Serial.begin(115200);
  delay(250);

  // Initialize hardware and software modules.
  lvgl_port::init();
  app_controls::init();
  app_runtime::init();
  app_runtime::connect_to_wlan_if_needed();

  // Create all application tasks.
  xTaskCreate(app_tasks::lamp_control, "lamp_control", app_tasks::stackLampControl, nullptr, app_tasks::priorityControl, &s_lamp_task_handle);
  xTaskCreate(app_tasks::led_control, "LED_control", app_tasks::stackLedControl, nullptr, app_tasks::priorityControl, &s_led_task_handle);
  xTaskCreate(app_tasks::display_ui, "display_UI", app_tasks::stackDisplayUi, nullptr, app_tasks::priorityDisplay, &s_display_task_handle);
  xTaskCreate(app_tasks::time_task, "time_task", app_tasks::stackTimeTask, nullptr, app_tasks::priorityTime, &s_time_task_handle);
  xTaskCreate(app_tasks::weather_task, "weather_task", app_tasks::stackWeatherTask, nullptr, app_tasks::priorityWeather, &s_weather_task_handle);

  s_runtime_stats_task_params.lamp_task = s_lamp_task_handle;
  s_runtime_stats_task_params.led_task = s_led_task_handle;
  s_runtime_stats_task_params.display_task = s_display_task_handle;
  s_runtime_stats_task_params.time_task = s_time_task_handle;
  s_runtime_stats_task_params.weather_task = s_weather_task_handle;

  xTaskCreate(app_tasks::runtime_stats_task,
              "runtime_stats",
              app_tasks::stackRuntimeStatsTask,
              &s_runtime_stats_task_params,
              app_tasks::priorityRuntimeStats,
              &s_runtime_stats_task_handle);

  // Register task handles with modules that need to notify them.
  app_controls::register_task_handles(s_lamp_task_handle, s_led_task_handle);
  app_runtime::set_task_handles(s_lamp_task_handle,
                                s_led_task_handle,
                                s_display_task_handle,
                                s_time_task_handle,
                                s_weather_task_handle);

  // Create and start a periodic timer to trigger weather data fetching.
  s_weather_timer = xTimerCreate("weather_timer",
                                 pdMS_TO_TICKS(weatherPeriodMs),
                                 pdTRUE,
                                 nullptr,
                                 app_tasks::weather_timer_callback);
  if (s_weather_timer != nullptr) {
    xTimerStart(s_weather_timer, 0);
  }

  // Trigger an initial weather fetch immediately on startup.
  app_runtime::notify_weather_task();

  Serial.println("[APP] FreeRTOS tasks started");
}

/**
 * @brief Main application loop.
 * @details In a FreeRTOS application, the main loop is typically idle or performs
 * very low-priority background tasks, as all primary work is handled by dedicated tasks.
 * This loop simply delays, yielding CPU time to other tasks.
 */
void loop() {
  // The main loop is idle as all work is done in FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
