#include <Arduino.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <time.h>

#include "app_controls.h"
#include "lvgl_port.h"
#include "openweather_config.h"
#include "side1_ui.h"
#include "side2_ui.h"
#include "side3_ui.h"
#include "wifi_credentials.h"

namespace {

constexpr uint32_t kAutoSwitchIntervalMs = 5000;
constexpr uint32_t kIdleTimeoutMs = 10000;
constexpr uint32_t kArcUpdateIntervalMs = 100;

constexpr uint8_t kDhtPin = 17;
constexpr uint32_t kDhtReadIntervalMs = 15000;

constexpr uint32_t kTimeUpdateIntervalMs = 1000;
constexpr uint32_t kWeatherRefreshIntervalMs = 3600000;
constexpr uint32_t kReconnectIntervalMs = 5000;

constexpr UBaseType_t kAppTaskPriority = 3;
constexpr uint32_t kAppTaskStackSize = 12288;

DHTesp s_dht;

uint8_t s_current_page = 0;
uint32_t s_last_page_switch_ms = 0;
uint32_t s_last_ui_update_ms = 0;
uint32_t s_last_dht_read_ms = 0;
uint32_t s_last_time_update_ms = 0;
uint32_t s_last_weather_sync_ms = 0;
uint32_t s_last_reconnect_try_ms = 0;
bool s_auto_cycling_enabled = false;
bool s_time_sync_initialized = false;

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

void switch_to_page(uint8_t page_index) {
  if (page_index >= 3) {
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
  } else if (page_index == 1) {
    side2::build_screen(s_current_screen_obj);
  } else {
    side3::build_screen(s_current_screen_obj);
  }

  s_current_page = page_index;
  s_last_page_switch_ms = millis();
}

void update_arc_loaders() {
  const uint32_t elapsed_ms = millis() - s_last_page_switch_ms;
  const int progress = constrain((elapsed_ms * 100) / kAutoSwitchIntervalMs, 0, 100);

  if (s_current_page == 0) {
    side1::update_arc_progress(progress);
  } else if (s_current_page == 1) {
    side2::update_arc_progress(progress);
  } else {
    side3::update_arc_progress(progress);
  }
}

void check_interactions_and_idle() {
  const uint32_t ms_since_interaction = app_controls::get_ms_since_last_interaction();

  if (s_current_page == 0 && ms_since_interaction >= kIdleTimeoutMs && !s_auto_cycling_enabled) {
    s_auto_cycling_enabled = true;
  }

  if (s_current_page == 0 && ms_since_interaction < kIdleTimeoutMs && s_auto_cycling_enabled) {
    s_auto_cycling_enabled = false;
  }
}

void check_page_switch() {
  if (!s_auto_cycling_enabled) {
    return;
  }

  const uint32_t elapsed_ms = millis() - s_last_page_switch_ms;
  if (elapsed_ms >= kAutoSwitchIntervalMs) {
    const uint8_t next_page = static_cast<uint8_t>((s_current_page + 1) % 3);
    switch_to_page(next_page);
  }
}

void connect_to_wlan() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (strcmp(wifi_credentials::kWlanSsid, "YOUR_WLAN_NAME") == 0 ||
      strcmp(wifi_credentials::kWlanPassword, "YOUR_WLAN_PASSWORD") == 0) {
    Serial.println("[WLAN] Credentials are still placeholders. Update include/wifi_credentials.h");
    return;
  }

  Serial.printf("[WLAN] Searching for %s\n", wifi_credentials::kWlanSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_credentials::kWlanSsid, wifi_credentials::kWlanPassword);
  Serial.printf("[WLAN] Connecting to %s\n", wifi_credentials::kWlanSsid);
}

void init_time_sync() {
  if (WiFi.status() != WL_CONNECTED || s_time_sync_initialized) {
    return;
  }

  configTime(openweather_config::kGmtOffsetSec, openweather_config::kDaylightOffsetSec, "pool.ntp.org", "time.nist.gov");
  s_time_sync_initialized = true;
  Serial.println("[TIME] NTP sync initialized");
}

void update_time_on_side3() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!s_time_sync_initialized) {
    init_time_sync();
  }

  struct tm time_info;
  if (!getLocalTime(&time_info, 100)) {
    return;
  }

  side3::set_time(time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
  side3::set_date(time_info.tm_mday, time_info.tm_mon + 1, time_info.tm_year + 1900);
}

void fetch_weather_from_openweather() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (strcmp(openweather_config::kApiKey, "YOUR_OPENWEATHER_API_KEY") == 0) {
    Serial.println("[WEATHER] API key placeholder detected. Update include/openweather_config.h");
    return;
  }

  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(openweather_config::kLatitude, 6);
  url += "&lon=";
  url += String(openweather_config::kLongitude, 6);
  url += "&units=metric&appid=";
  url += openweather_config::kApiKey;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("[WEATHER] HTTPS begin failed");
    return;
  }

  const int http_code = https.GET();
  if (http_code != HTTP_CODE_OK) {
    Serial.printf("[WEATHER] HTTP error: %d\n", http_code);
    https.end();
    return;
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
    Serial.println("[WEATHER] Failed to parse response");
    return;
  }

  const int rounded_temp = static_cast<int>(lroundf(temp_c));
  side3::set_weather(weather_main.c_str(), rounded_temp);
  Serial.printf("[WEATHER] Updated: %s, %d C\n", weather_main.c_str(), rounded_temp);
}

void init_dht_sensor() {
  pinMode(kDhtPin, INPUT_PULLUP);
  s_dht.setup(kDhtPin, DHTesp::DHT11);
  side2::set_sensor_disconnected();
  s_last_dht_read_ms = millis();

  Serial.printf("[DHT] Init complete. Type=DHT11 Pin=GPIO%u level=%d\n", kDhtPin, digitalRead(kDhtPin));
}

void update_dht_sensor() {
  const uint32_t now = millis();
  if (now - s_last_dht_read_ms < kDhtReadIntervalMs) {
    return;
  }

  s_last_dht_read_ms = now;

  const TempAndHumidity reading = s_dht.getTempAndHumidity();
  if (isnan(reading.temperature) || isnan(reading.humidity)) {
    side2::set_sensor_disconnected();
    return;
  }

  const int16_t temperature_c = static_cast<int16_t>(lroundf(reading.temperature));
  const uint8_t humidity_percent = static_cast<uint8_t>(constrain(lroundf(reading.humidity), 0L, 100L));
  side2::set_readings(temperature_c, humidity_percent);
}

void app_task(void *parameter) {
  (void)parameter;

  connect_to_wlan();
  init_time_sync();
  init_dht_sensor();
  fetch_weather_from_openweather();
  s_last_weather_sync_ms = millis();
  switch_to_page(0);

  for (;;) {
    app_controls::update();

    bool desired_lamp_on = false;
    while (app_controls::receive_lamp_command(desired_lamp_on, 0)) {
      app_controls::send_lamp_http(desired_lamp_on);
    }

    const uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED && (now - s_last_reconnect_try_ms) >= kReconnectIntervalMs) {
      s_last_reconnect_try_ms = now;
      connect_to_wlan();
      s_time_sync_initialized = false;
    }

    if (now - s_last_time_update_ms >= kTimeUpdateIntervalMs) {
      s_last_time_update_ms = now;
      update_time_on_side3();
    }

    if (now - s_last_weather_sync_ms >= kWeatherRefreshIntervalMs) {
      s_last_weather_sync_ms = now;
      fetch_weather_from_openweather();
    }

    check_interactions_and_idle();
    check_page_switch();
    update_dht_sensor();

    if (now - s_last_ui_update_ms >= kArcUpdateIntervalMs) {
      update_arc_loaders();
      s_last_ui_update_ms = now;
    }

    lvgl_port::task_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  lvgl_port::init();
  app_controls::init();

  const BaseType_t app_ok = xTaskCreatePinnedToCore(
      app_task,
      "app",
      kAppTaskStackSize,
      nullptr,
      kAppTaskPriority,
      nullptr,
      1);

  if (app_ok != pdPASS) {
    Serial.printf("[RTOS] Task create failed app=%d\n", app_ok);
  } else {
    Serial.println("[RTOS] App task started");
  }
}

void loop() {
  vTaskDelete(nullptr);
}
