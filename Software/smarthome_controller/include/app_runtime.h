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

struct TimeMessage {
  int hour;
  int minute;
  int second;
  int day;
  int month;
  int year;
};

struct WeatherShared {
  char condition[32];
  int temp_c;
  bool valid;
};

constexpr EventBits_t lampStateBit = (1U << 0);
constexpr EventBits_t rgbSelectedRedBit = (1U << 1);
constexpr EventBits_t rgbSelectedGreenBit = (1U << 2);
constexpr EventBits_t rgbSelectedBlueBit = (1U << 3);
constexpr EventBits_t rgbSelectionMask = rgbSelectedRedBit | rgbSelectedGreenBit | rgbSelectedBlueBit;
constexpr EventBits_t uiLampChangedBit = (1U << 0);
constexpr EventBits_t uiLedChangedBit = (1U << 1);

void init();

void set_task_handles(TaskHandle_t lamp_task,
                      TaskHandle_t led_task,
                      TaskHandle_t display_task,
                      TaskHandle_t time_task,
                      TaskHandle_t weather_task);

TaskHandle_t weather_task_handle();

QueueHandle_t time_queue();
SemaphoreHandle_t weather_mutex();
SemaphoreHandle_t network_mutex();
EventGroupHandle_t light_state_events();
EventGroupHandle_t ui_change_events();

void connect_to_wlan_if_needed();
bool fetch_weather_from_openweather(char *out_condition, size_t out_condition_len, int &out_temp_c);

void switch_to_page(uint8_t page_index);
uint8_t current_page();
uint32_t page_switch_age_ms();
void set_auto_cycling_enabled(bool enabled);
bool auto_cycling_enabled();
void update_arc_loaders();

void set_lamp_state_bit(bool on);
void set_selected_rgb_channel(uint8_t active_channel);
void signal_ui_change(EventBits_t bits);
EventBits_t read_light_state_bits();

bool read_weather_state(WeatherShared &out_state);
void write_weather_state(const char *condition, int temp_c);

void notify_weather_task();

}  // namespace app_runtime
