#include <Arduino.h>
#include <cstring>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <lvgl.h>
#include <time.h>

#include "app_controls.h"
#include "lvgl_port.h"
#include "openweather_config.h"
#include "side1_ui.h"
#include "side2_ui.h"
#include "side3_ui.h"
#include "wifi_credentials.h"

// Page switching configuration
constexpr uint32_t kAutoSwitchIntervalMs = 5000;    // 5 seconds between pages
constexpr uint32_t kIdleTimeoutMs = 10000;           // 10 seconds idle timeout on Side 1
constexpr uint32_t kArcUpdateIntervalMs = 100;       // Update arc loader every 100ms
constexpr uint32_t kDhtReadIntervalMs = 15000;       // Read DHT11 every 15 seconds
constexpr uint8_t kDhtPin = 17;
constexpr uint32_t kWlanConnectTimeoutMs = 20000;    // Try WLAN connection for up to 20 seconds
constexpr uint32_t kTimeUpdateIntervalMs = 1000;     // Refresh displayed time every second
constexpr uint32_t kWeatherRefreshIntervalMs = 3600000; // Refresh weather every 60 minutes

// State variables for page switching
uint8_t current_page = 0;  // 0 = Side 1, 1 = Side 2, 2 = Side 3
uint32_t page_switch_timer_ms = 0;
uint32_t last_page_switch_ms = 0;
uint32_t last_ui_update_ms = 0;
bool auto_cycling_enabled = false;
uint32_t last_dht_read_ms = 0;
uint32_t last_time_update_ms = 0;
uint32_t last_weather_sync_ms = 0;
bool s_time_sync_initialized = false;

DHTesp s_dht;

// Current screen objects
lv_obj_t *s_current_screen_obj = nullptr;

void switch_to_page(uint8_t page_index) {
  if (page_index >= 3) {
    return;
  }

  // Clear the current screen
  if (s_current_screen_obj != nullptr) {
    lv_obj_delete(s_current_screen_obj);
    s_current_screen_obj = nullptr;
  }

  // Set the new page
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xE9E9E9), 0);

  // Create container for the new page
  s_current_screen_obj = lv_obj_create(screen);
  lv_obj_set_size(s_current_screen_obj, 480, 320);
  lv_obj_align(s_current_screen_obj, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(s_current_screen_obj, 0, 0);
  lv_obj_set_style_pad_all(s_current_screen_obj, 0, 0);
  lv_obj_set_style_bg_color(s_current_screen_obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_scrollbar_mode(s_current_screen_obj, LV_SCROLLBAR_MODE_OFF);


  // Build the appropriate page
  if (page_index == 0) {
    side1::build_screen(s_current_screen_obj);
  } else if (page_index == 1) {
    side2::build_screen(s_current_screen_obj);
  } else if (page_index == 2) {
    side3::build_screen(s_current_screen_obj);
  }

  current_page = page_index;
  last_page_switch_ms = millis();
  page_switch_timer_ms = 0;
}

void update_arc_loaders() {
  uint32_t elapsed_ms = millis() - last_page_switch_ms;
  int progress = constrain((elapsed_ms * 100) / kAutoSwitchIntervalMs, 0, 100);

  if (current_page == 0) {
    side1::update_arc_progress(progress);
  } else if (current_page == 1) {
    side2::update_arc_progress(progress);
  } else if (current_page == 2) {
    side3::update_arc_progress(progress);
  }
}

void check_interactions_and_idle() {
  uint32_t ms_since_interaction = app_controls::get_ms_since_last_interaction();

  // If user has interacted, go to Side 1 and reset idle timer
  if (ms_since_interaction < 100) {
    if (current_page != 0) {
      switch_to_page(0);
      auto_cycling_enabled = false;
    }
  }

  // If on Side 1 and no interaction for 10 seconds, enable auto-cycling
  if (current_page == 0 && ms_since_interaction >= kIdleTimeoutMs && !auto_cycling_enabled) {
    auto_cycling_enabled = true;
    page_switch_timer_ms = 0;
  }

  // If on Side 1 and still interacting, disable auto-cycling
  if (current_page == 0 && ms_since_interaction < kIdleTimeoutMs && auto_cycling_enabled) {
    auto_cycling_enabled = false;
    page_switch_timer_ms = 0;
  }
}

void check_page_switch() {
  if (!auto_cycling_enabled) {
    return;
  }

  uint32_t elapsed_ms = millis() - last_page_switch_ms;
  if (elapsed_ms >= kAutoSwitchIntervalMs) {
    uint8_t next_page = (current_page + 1) % 3;
    switch_to_page(next_page);
  }
}

void init_dht_sensor() {
  pinMode(kDhtPin, INPUT_PULLUP);
  s_dht.setup(kDhtPin, DHTesp::DHT11);
  side2::set_sensor_disconnected();
  last_dht_read_ms = millis();

  Serial.printf("[DHT] Init complete. Type=DHT11 Pin=GPIO%u level=%d\n", kDhtPin, digitalRead(kDhtPin));
}

void update_dht_sensor() {
  const uint32_t now = millis();
  if (now - last_dht_read_ms < kDhtReadIntervalMs) {
    return;
  }

  last_dht_read_ms = now;

  const int pin_level_before = digitalRead(kDhtPin);
  Serial.printf("[DHT] Read start t=%lu ms pin=GPIO%u level=%d\n", now, kDhtPin, pin_level_before);

  const TempAndHumidity reading = s_dht.getTempAndHumidity();
  if (isnan(reading.temperature) || isnan(reading.humidity)) {
    Serial.printf("[DHT] Read failed (NaN). pin=GPIO%u level_after=%d\n", kDhtPin, digitalRead(kDhtPin));
    side2::set_sensor_disconnected();
    return;
  }

  const int16_t temperature_c = static_cast<int16_t>(lroundf(reading.temperature));
  const uint8_t humidity_percent = static_cast<uint8_t>(constrain(lroundf(reading.humidity), 0L, 100L));

  Serial.printf(
      "[DHT] Read OK. temp=%.1fC hum=%.1f%% -> shown: %dC %u%% level_after=%d\n",
      reading.temperature,
      reading.humidity,
      temperature_c,
      humidity_percent,
      digitalRead(kDhtPin));

  side2::set_readings(temperature_c, humidity_percent);
}

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

bool extract_json_string_after_key(
    const String &json,
    const String &key,
    int search_start,
    String &out_value) {
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

void init_time_sync() {
  if (WiFi.status() != WL_CONNECTED || s_time_sync_initialized) {
    return;
  }

  configTime(
      openweather_config::kGmtOffsetSec,
      openweather_config::kDaylightOffsetSec,
      "pool.ntp.org",
      "time.nist.gov");
  s_time_sync_initialized = true;
  Serial.println("[TIME] NTP sync initialized");
}

void update_time_on_side3() {
  const uint32_t now_ms = millis();
  if (now_ms - last_time_update_ms < kTimeUpdateIntervalMs) {
    return;
  }

  last_time_update_ms = now_ms;

  if (!s_time_sync_initialized) {
    init_time_sync();
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
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
    Serial.println("[WEATHER] Skipped: WLAN not connected");
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

void update_weather_on_side3() {
  const uint32_t now_ms = millis();
  if (now_ms - last_weather_sync_ms < kWeatherRefreshIntervalMs) {
    return;
  }

  last_weather_sync_ms = now_ms;
  fetch_weather_from_openweather();
}

void connect_to_wlan() {
  if (strcmp(wifi_credentials::kWlanSsid, "YOUR_WLAN_NAME") == 0 ||
      strcmp(wifi_credentials::kWlanPassword, "YOUR_WLAN_PASSWORD") == 0) {
    Serial.println("[WLAN] Credentials are still placeholders. Update include/wifi_credentials.h");
    return;
  }

  Serial.printf("[WLAN] Searching for %s\n", wifi_credentials::kWlanSsid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  WiFi.begin(wifi_credentials::kWlanSsid, wifi_credentials::kWlanPassword);
  Serial.printf("[WLAN] Connecting to %s\n", wifi_credentials::kWlanSsid);

  const uint32_t start_ms = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < kWlanConnectTimeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WLAN] %s: Connected\n", WiFi.SSID().c_str());
    Serial.printf("[WLAN] IP Address: %s\n", WiFi.localIP().toString().c_str());
    return;
  }

  Serial.printf("[WLAN] Connection failed for %s\n", wifi_credentials::kWlanSsid);
}

void setup() {
  Serial.begin(115200);
  delay(250);

  connect_to_wlan();
  init_time_sync();
  lvgl_port::init();
  app_controls::init();
  init_dht_sensor();

  fetch_weather_from_openweather();
  last_weather_sync_ms = millis();

  // Start with Side 1
  switch_to_page(0);
}

void loop() {
  app_controls::update();

  // Check for interactions and idle timeout
  check_interactions_and_idle();

  // Check for page switch in auto-cycling mode
  check_page_switch();

  // Read DHT11 on GPIO17 and refresh Side 2 values
  update_dht_sensor();

  // Update Side 3 time every second and weather every 60 minutes
  update_time_on_side3();
  update_weather_on_side3();

  // Update arc loaders periodically
  uint32_t now = millis();
  if (now - last_ui_update_ms >= kArcUpdateIntervalMs) {
    update_arc_loaders();
    last_ui_update_ms = now;
  }

  lvgl_port::task_handler();
  delay(5);
}