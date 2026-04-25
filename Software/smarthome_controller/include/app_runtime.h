/**
 * @file app_runtime.h
 * @brief Defines shared queues, mutexes, event groups, and helper APIs used by all RTOS tasks.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace app_runtime {

/**
 * @brief Time payload shared from the time task to the display task.
 */
struct TimeMessage {
  /** @brief Hour in 24-hour format [0..23]. */
  int hour;
  /** @brief Minute [0..59]. */
  int minute;
  /** @brief Second [0..59]. */
  int second;
  /** @brief Day of month [1..31]. */
  int day;
  /** @brief Month [1..12]. */
  int month;
  /** @brief Full year (for example 2026). */
  int year;
};

/**
 * @brief Shared weather snapshot protected by a mutex.
 */
struct WeatherShared {
  /** @brief Text weather condition (for example "Clouds"). */
  char condition[32];
  /** @brief Temperature in degree Celsius. */
  int temp_c;
  /** @brief True when fields contain a valid API result. */
  bool valid;
};

// Event bits stored in the RTOS event group for lamp/RGB state.
constexpr EventBits_t lampStateBit = (1U << 0);       ///< Lamp on/off state bit.
constexpr EventBits_t rgbSelectedRedBit = (1U << 1);  ///< Active RGB channel = red.
constexpr EventBits_t rgbSelectedGreenBit = (1U << 2);  ///< Active RGB channel = green.
constexpr EventBits_t rgbSelectedBlueBit = (1U << 3);  ///< Active RGB channel = blue.
constexpr EventBits_t rgbSelectionMask = rgbSelectedRedBit | rgbSelectedGreenBit | rgbSelectedBlueBit;  ///< Mask for all RGB selection bits.

// Event bits used to notify the display task that UI data changed.
constexpr EventBits_t uiLampChangedBit = (1U << 0);  ///< Lamp state changed.
constexpr EventBits_t uiLedChangedBit = (1U << 1);   ///< LED state or channel changed.

/**
 * @brief Initialize shared runtime resources (queues, mutexes, event groups).
 */
void init();

/**
 * @brief Register task handles so runtime helpers can notify specific tasks.
 *
 * @param lamp_task Handle of lamp control task.
 * @param led_task Handle of LED control task.
 * @param display_task Handle of display/UI task.
 * @param time_task Handle of time producer task.
 * @param weather_task Handle of weather task.
 */
void set_task_handles(TaskHandle_t lamp_task,
                      TaskHandle_t led_task,
                      TaskHandle_t display_task,
                      TaskHandle_t time_task,
                      TaskHandle_t weather_task);

/**
 * @brief Get the registered weather task handle.
 *
 * @return Task handle or nullptr if not registered.
 */
TaskHandle_t weather_task_handle();

// RTOS primitive accessors used by cooperating tasks.
/**
 * @brief Access the queue carrying latest time values.
 *
 * @return FreeRTOS queue handle.
 */
QueueHandle_t time_queue();
/**
 * @brief Access weather-state mutex.
 *
 * @return FreeRTOS semaphore handle.
 */
SemaphoreHandle_t weather_mutex();
/**
 * @brief Access network mutex to serialize WLAN/HTTP usage.
 *
 * @return FreeRTOS semaphore handle.
 */
SemaphoreHandle_t network_mutex();
/**
 * @brief Access event group for lamp and RGB selection state.
 *
 * @return FreeRTOS event group handle.
 */
EventGroupHandle_t light_state_events();
/**
 * @brief Access event group used for UI-change notifications.
 *
 * @return FreeRTOS event group handle.
 */
EventGroupHandle_t ui_change_events();

/**
 * @brief Connect to WLAN if disconnected and reconnect interval elapsed.
 */
void connect_to_wlan_if_needed();
/**
 * @brief Fetch weather data from OpenWeather API.
 *
 * @param out_condition Destination buffer for weather text.
 * @param out_condition_len Size of destination buffer.
 * @param[out] out_temp_c Parsed temperature in Celsius.
 * @return true on successful fetch and parse.
 * @return false on connectivity/API/parse failure.
 */
bool fetch_weather_from_openweather(char *out_condition, size_t out_condition_len, int &out_temp_c);

/**
 * @brief Rebuild active screen for the selected page index.
 *
 * @param page_index Page index (0 or 1).
 */
void switch_to_page(uint8_t page_index);
/**
 * @brief Get current page index.
 *
 * @return Active page index.
 */
uint8_t current_page();
/**
 * @brief Get elapsed milliseconds since last page switch.
 *
 * @return Age in milliseconds.
 */
uint32_t page_switch_age_ms();
/**
 * @brief Enable or disable automatic page cycling.
 *
 * @param enabled True to enable auto cycling.
 */
void set_auto_cycling_enabled(bool enabled);
/**
 * @brief Query whether automatic page cycling is enabled.
 *
 * @return true if enabled.
 */
bool auto_cycling_enabled();
/**
 * @brief Update arc loader progress based on page switch age.
 */
void update_arc_loaders();

/**
 * @brief Set or clear lamp state bit in the shared light-state event group.
 *
 * @param on True to set lamp on bit, false to clear it.
 */
void set_lamp_state_bit(bool on);
/**
 * @brief Publish active RGB channel into shared event bits.
 *
 * @param active_channel Selected channel index.
 */
void set_selected_rgb_channel(uint8_t active_channel);
/**
 * @brief Raise one or more UI-change event bits.
 *
 * @param bits Bit mask from uiLampChangedBit and/or uiLedChangedBit.
 */
void signal_ui_change(EventBits_t bits);
/**
 * @brief Read current light-state event bits.
 *
 * @return Current bit field.
 */
EventBits_t read_light_state_bits();

/**
 * @brief Try to read latest weather snapshot without blocking.
 *
 * @param[out] out_state Destination for copied weather data.
 * @return true when mutex was acquired and data copied.
 * @return false when mutex unavailable or not initialized.
 */
bool read_weather_state(WeatherShared &out_state);
/**
 * @brief Write latest weather snapshot under mutex protection.
 *
 * @param condition Weather condition string or nullptr for fallback.
 * @param temp_c Temperature in Celsius.
 */
void write_weather_state(const char *condition, int temp_c);

/**
 * @brief Notify weather task via FreeRTOS task notification.
 */
void notify_weather_task();

}  // namespace app_runtime
