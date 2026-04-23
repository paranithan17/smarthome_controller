/**
 * @file task_time.cpp
 * @brief FreeRTOS task that keeps time synchronized through NTP and publishes it to the UI.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "app_config.h"
#include "app_runtime.h"

namespace app_tasks {

namespace {
// The interval at which the local time is updated and sent to the UI.
constexpr uint32_t timeTickMs = 1000;
// The interval at which the system resynchronizes with the NTP server.
constexpr uint32_t ntpSyncIntervalSec = 180UL * 60UL;  // 180 minutes
// Grace period at boot before the first network activity.
constexpr uint32_t bootNetworkGraceMs = 8000;
}  // namespace

/**
 * @brief FreeRTOS task to manage time synchronization and publishing.
 * @details This task periodically synchronizes with an NTP server to get the correct time.
 * It then maintains a local epoch counter, incrementing it every second, and sends
 * the formatted time and date to the display task via a queue.
 * @param arg Task arguments (not used).
 */
void time_task(void *arg) {
  (void)arg;

  // Wait for a grace period at startup to allow the network to connect.
  vTaskDelay(pdMS_TO_TICKS(bootNetworkGraceMs));

  bool synced_once = false;
  time_t current_epoch = 0;
  // Initialize to the sync interval to trigger an immediate sync on the first run.
  uint32_t elapsed_since_sync_sec = ntpSyncIntervalSec;

  for (;;) {
    // Ensure a WLAN connection is available before attempting NTP sync.
    app_runtime::connect_to_wlan_if_needed();

    // Check if it's time to resynchronize with the NTP server.
    if ((elapsed_since_sync_sec >= ntpSyncIntervalSec) && WiFi.status() == WL_CONNECTED) {
      // Acquire the network mutex to ensure exclusive access.
      if (xSemaphoreTake(app_runtime::network_mutex(), pdMS_TO_TICKS(2000)) == pdTRUE) {
        // Configure the NTP client with timezone offsets and server pools.
        configTime(app_config::gmtOffsetSec,
             app_config::daylightOffsetSec,
                   "pool.ntp.org",
                   "time.nist.gov");

        // Fetch the current time from the NTP server.
        const time_t ntp_now = time(nullptr);
        // A simple check to see if the returned time is valid.
        if (ntp_now > 100000) {
          current_epoch = ntp_now;
          synced_once = true;
          elapsed_since_sync_sec = 0;
          Serial.println("[TIME] NTP synchronized");
        }

        // Release the network mutex.
        xSemaphoreGive(app_runtime::network_mutex());
      }
    }

    // If time has been successfully synced at least once, update and publish it.
    if (synced_once) {
      struct tm time_info;
      localtime_r(&current_epoch, &time_info);

      // Populate the time message for the UI task.
      app_runtime::TimeMessage msg;
      msg.hour = time_info.tm_hour;
      msg.minute = time_info.tm_min;
      msg.second = time_info.tm_sec;
      msg.day = time_info.tm_mday;
      msg.month = time_info.tm_mon + 1;  // tm_mon is 0-11
      msg.year = time_info.tm_year + 1900; // tm_year is years since 1900

      // Send the time message to the display task, overwriting the previous one.
      xQueueOverwrite(app_runtime::time_queue(), &msg);

      // Increment the local epoch counter and the sync timer.
      current_epoch += 1;
      elapsed_since_sync_sec += 1;
    }

    // Wait for one second before the next tick.
    vTaskDelay(pdMS_TO_TICKS(timeTickMs));
  }
}

}  // namespace app_tasks
