/**
 * @file task_weather.cpp
 * @brief FreeRTOS task that periodically fetches weather data from OpenWeather.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_runtime.h"

namespace app_tasks {

namespace {
// Grace period at boot before the first network activity.
constexpr uint32_t bootNetworkGraceMs = 8000;
}  // namespace

/**
 * @brief Callback executed by the periodic weather timer to notify the weather task.
 * @param timer_handle Handle of the timer that triggered this callback.
 */
void weather_timer_callback(TimerHandle_t timer_handle) {
  (void)timer_handle;
  app_runtime::notify_weather_task();
}

/**
 * @brief FreeRTOS task to fetch weather data.
 * @details This task waits for a notification, then attempts to fetch weather data from
 * the OpenWeather API. It ensures a WLAN connection is available before making the
 * request and uses a mutex to prevent concurrent network access.
 * @param arg Task arguments (not used).
 */
void weather_task(void *arg) {
  (void)arg;

  // Wait for a grace period at startup to allow the network to connect.
  vTaskDelay(pdMS_TO_TICKS(bootNetworkGraceMs));

  for (;;) {
    // Wait indefinitely for a notification to trigger a weather fetch.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Wait until a WLAN connection is established.
    while (WiFi.status() != WL_CONNECTED) {
      app_runtime::connect_to_wlan_if_needed();
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    char weather_condition[32] = "--";
    int temp_c = 0;

    // Acquire the network mutex to ensure exclusive access.
    if (xSemaphoreTake(app_runtime::network_mutex(), pdMS_TO_TICKS(3000)) == pdTRUE) {
      // Fetch the weather data from the OpenWeather API.
      const bool ok = app_runtime::fetch_weather_from_openweather(weather_condition, sizeof(weather_condition), temp_c);
      // Release the network mutex.
      xSemaphoreGive(app_runtime::network_mutex());

      // If the fetch was successful, update the shared weather state.
      if (ok) {
        app_runtime::write_weather_state(weather_condition, temp_c);
      }
    }
  }
}

}  // namespace app_tasks
