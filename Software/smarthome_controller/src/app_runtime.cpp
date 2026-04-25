/**
 * @file app_runtime.cpp
 * @brief Shared runtime state, networking helpers, and screen management for the RTOS application.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_runtime.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <lvgl.h>

#include "app_config.h"
#include "side1_ui.h"
#include "side3_ui.h"

namespace app_runtime {

namespace {

constexpr uint32_t autoSwitchIntervalMs = 5000;
constexpr uint32_t reconnectIntervalMs = 5000;

TaskHandle_t s_lamp_task_handle = nullptr;
TaskHandle_t s_led_task_handle = nullptr;
TaskHandle_t s_display_task_handle = nullptr;
TaskHandle_t s_time_task_handle = nullptr;
TaskHandle_t s_weather_task_handle = nullptr;

// Shared RTOS communication primitives.
QueueHandle_t s_time_queue = nullptr;
SemaphoreHandle_t s_weather_mutex = nullptr;
SemaphoreHandle_t s_network_mutex = nullptr;
EventGroupHandle_t s_light_state_events = nullptr;
EventGroupHandle_t s_ui_change_events = nullptr;

WeatherShared s_weather_shared = {"--", 0, false};

uint8_t s_current_page = 0;
bool s_auto_cycling_enabled = false;
uint32_t s_last_page_switch_ms = 0;
uint32_t s_last_reconnect_try_ms = 0;

lv_obj_t *s_current_screen_obj = nullptr;

bool extract_json_number_after_key(const String &json, const String &key, int search_start, float &out_value) {
  const int key_index = json.indexOf(key, search_start);
  if (key_index < 0) {
    return false;
  }

  const int value_start = key_index + key.length();
  int value_end = value_start;
  while (value_end < static_cast<int>(json.length())) {
    const char ch = json[value_end];
    if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '.') {
      value_end++;
      continue;
    }
    break;
  }

  if (value_end <= value_start) {
    return false;
  }

  out_value = json.substring(value_start, value_end).toFloat();
  return true;
}

bool extract_json_string_after_key(const String &json, const String &key, int search_start, String &out_value) {
  const int key_index = json.indexOf(key, search_start);
  if (key_index < 0) {
    return false;
  }

  const int value_start = key_index + key.length();
  const int value_end = json.indexOf('"', value_start);
  if (value_end < 0) {
    return false;
  }

  out_value = json.substring(value_start, value_end);
  return true;
}

EventBits_t active_channel_to_event_bit(uint8_t active_channel) {
  switch (active_channel % 3U) {
    case 0:
      return rgbSelectedRedBit;
    case 1:
      return rgbSelectedGreenBit;
    default:
      return rgbSelectedBlueBit;
  }
}

}  // namespace

void init() {
  // Single-slot queue keeps only the latest time sample for UI.
  s_time_queue = xQueueCreate(1, sizeof(TimeMessage));
  // Mutex protects read/write access to shared weather struct.
  s_weather_mutex = xSemaphoreCreateMutex();
  // Mutex serializes network operations across tasks.
  s_network_mutex = xSemaphoreCreateMutex();
  // Event groups distribute compact state and UI-change notifications.
  s_light_state_events = xEventGroupCreate();
  s_ui_change_events = xEventGroupCreate();
  strncpy(s_weather_shared.condition, "--", sizeof(s_weather_shared.condition) - 1);
  s_weather_shared.condition[sizeof(s_weather_shared.condition) - 1] = '\0';
  s_weather_shared.temp_c = 0;
  s_weather_shared.valid = false;
  s_current_page = 0;
  s_auto_cycling_enabled = false;
  s_last_page_switch_ms = millis();
  s_last_reconnect_try_ms = 0;
}

void set_task_handles(TaskHandle_t lamp_task,
                      TaskHandle_t led_task,
                      TaskHandle_t display_task,
                      TaskHandle_t time_task,
                      TaskHandle_t weather_task) {
  s_lamp_task_handle = lamp_task;
  s_led_task_handle = led_task;
  s_display_task_handle = display_task;
  s_time_task_handle = time_task;
  s_weather_task_handle = weather_task;
}

TaskHandle_t weather_task_handle() {
  return s_weather_task_handle;
}

QueueHandle_t time_queue() {
  return s_time_queue;
}

SemaphoreHandle_t weather_mutex() {
  return s_weather_mutex;
}

SemaphoreHandle_t network_mutex() {
  return s_network_mutex;
}

EventGroupHandle_t light_state_events() {
  return s_light_state_events;
}

EventGroupHandle_t ui_change_events() {
  return s_ui_change_events;
}

void connect_to_wlan_if_needed() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const uint32_t now = millis();
  if ((now - s_last_reconnect_try_ms) < reconnectIntervalMs) {
    return;
  }

  s_last_reconnect_try_ms = now;

  if (strcmp(app_config::wlanSsid, "YOUR_WLAN_NAME") == 0 ||
      strcmp(app_config::wlanPassword, "YOUR_WLAN_PASSWORD") == 0) {
    Serial.println("[WLAN] Credentials are still placeholders. Update include/app_config.h");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(app_config::wlanSsid, app_config::wlanPassword);
  Serial.printf("[WLAN] Connecting to %s\n", app_config::wlanSsid);
}

bool fetch_weather_from_openweather(char *out_condition, size_t out_condition_len, int &out_temp_c) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WEATHER] WiFi not connected, skipping fetch");
    return false;
  }

  if (strcmp(app_config::openWeatherApiKey, "YOUR_OPENWEATHER_API_KEY") == 0) {
    Serial.println("[WEATHER] API key placeholder detected. Update include/app_config.h");
    return false;
  }

  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(app_config::latitude, 6);
  url += "&lon=";
  url += String(app_config::longitude, 6);
  url += "&units=metric&appid=";
  url += app_config::openWeatherApiKey;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("[WEATHER] HTTPS begin failed");
    return false;
  }

  const int http_code = https.GET();
  if (http_code != HTTP_CODE_OK) {
    Serial.printf("[WEATHER] HTTP error: %d\n", http_code);
    https.end();
    return false;
  }

  const String payload = https.getString();
  https.end();

  const int weather_array_index = payload.indexOf("\"weather\":[");
  const int main_index = payload.indexOf("\"main\":\"", weather_array_index);

  float temp_c = 0.0F;
  String weather_main;

  const bool parsed_temp = extract_json_number_after_key(payload, "\"temp\":", 0, temp_c);
  const bool parsed_weather = extract_json_string_after_key(payload, "\"main\":\"", main_index, weather_main);

  if (!parsed_temp || !parsed_weather || weather_main.length() == 0) {
    Serial.println("[WEATHER] Failed to parse response - missing fields");
    return false;
  }

  out_temp_c = static_cast<int>(lroundf(temp_c));
  strncpy(out_condition, weather_main.c_str(), out_condition_len - 1);
  out_condition[out_condition_len - 1] = '\0';
  return true;
}

void switch_to_page(uint8_t page_index) {
  if (page_index >= 2) {
    return;
  }

  if (s_current_screen_obj != nullptr) {
    lv_obj_delete(s_current_screen_obj);
    s_current_screen_obj = nullptr;
  }

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xE9E9E9), 0);

  s_current_screen_obj = lv_obj_create(screen);
  lv_obj_set_size(s_current_screen_obj, 480, 320);
  lv_obj_align(s_current_screen_obj, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(s_current_screen_obj, 0, 0);
  lv_obj_set_style_pad_all(s_current_screen_obj, 0, 0);
  lv_obj_set_style_bg_color(s_current_screen_obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_scrollbar_mode(s_current_screen_obj, LV_SCROLLBAR_MODE_OFF);

  if (page_index == 0) {
    side1::build_screen(s_current_screen_obj);
  } else {
    side3::build_screen(s_current_screen_obj);
  }

  s_current_page = page_index;
  s_last_page_switch_ms = millis();
}

uint8_t current_page() {
  return s_current_page;
}

uint32_t page_switch_age_ms() {
  return millis() - s_last_page_switch_ms;
}

void set_auto_cycling_enabled(bool enabled) {
  s_auto_cycling_enabled = enabled;
}

bool auto_cycling_enabled() {
  return s_auto_cycling_enabled;
}

void update_arc_loaders() {
  const uint32_t elapsed_ms = millis() - s_last_page_switch_ms;
  const int progress = constrain((elapsed_ms * 100) / autoSwitchIntervalMs, 0, 100);

  if (s_current_page == 0) {
    side1::update_arc_progress(progress);
  } else {
    side3::update_arc_progress(progress);
  }
}

void set_lamp_state_bit(bool on) {
  if (s_light_state_events == nullptr) {
    return;
  }

  // Publish lamp state as a single event-group bit.
  if (on) {
    xEventGroupSetBits(s_light_state_events, lampStateBit);
  } else {
    xEventGroupClearBits(s_light_state_events, lampStateBit);
  }
}

void set_selected_rgb_channel(uint8_t active_channel) {
  if (s_light_state_events == nullptr) {
    return;
  }

  // Keep exactly one RGB selection bit active at a time.
  xEventGroupClearBits(s_light_state_events, rgbSelectionMask);
  xEventGroupSetBits(s_light_state_events, active_channel_to_event_bit(active_channel));
}

void signal_ui_change(EventBits_t bits) {
  if (s_ui_change_events != nullptr) {
    // Wake display task logic through UI change event bits.
    xEventGroupSetBits(s_ui_change_events, bits);
  }
}

EventBits_t read_light_state_bits() {
  if (s_light_state_events == nullptr) {
    return 0;
  }

  return xEventGroupGetBits(s_light_state_events);
}

bool read_weather_state(WeatherShared &out_state) {
  if (s_weather_mutex == nullptr) {
    return false;
  }

  // Non-blocking read avoids stalling display updates.
  if (xSemaphoreTake(s_weather_mutex, 0) != pdTRUE) {
    return false;
  }

  out_state = s_weather_shared;
  xSemaphoreGive(s_weather_mutex);
  return true;
}

void write_weather_state(const char *condition, int temp_c) {
  if (s_weather_mutex == nullptr) {
    return;
  }

  // Block until write lock is available to keep snapshot consistent.
  if (xSemaphoreTake(s_weather_mutex, portMAX_DELAY) != pdTRUE) {
    return;
  }

  s_weather_shared.valid = true;
  s_weather_shared.temp_c = temp_c;
  if (condition != nullptr) {
    strncpy(s_weather_shared.condition, condition, sizeof(s_weather_shared.condition) - 1);
    s_weather_shared.condition[sizeof(s_weather_shared.condition) - 1] = '\0';
  } else {
    strncpy(s_weather_shared.condition, "--", sizeof(s_weather_shared.condition) - 1);
    s_weather_shared.condition[sizeof(s_weather_shared.condition) - 1] = '\0';
  }

  xSemaphoreGive(s_weather_mutex);
}

void notify_weather_task() {
  if (s_weather_task_handle != nullptr) {
    // Task notification is a lightweight RTOS wake-up signal.
    xTaskNotifyGive(s_weather_task_handle);
  }
}

}  // namespace app_runtime
