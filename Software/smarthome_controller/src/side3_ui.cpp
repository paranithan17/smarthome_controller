#include "side3_ui.h"

#include <Arduino.h>
#include <cstring>
#include <WiFi.h>

#include "lvgl_port.h"

namespace side3 {

static lv_obj_t *s_time_label = nullptr;
static lv_obj_t *s_date_label = nullptr;
static lv_obj_t *s_weather_label = nullptr;
static lv_obj_t *s_temp_label = nullptr;
static lv_obj_t *s_arc_loader = nullptr;

static int s_cached_hour = -1;
static int s_cached_minute = -1;
static int s_cached_second = -1;
static int s_cached_day = -1;
static int s_cached_month = -1;
static int s_cached_year = -1;
static int s_cached_temp_c = 0;
static bool s_cached_weather_valid = false;
static char s_cached_weather_condition[32] = "--";

static bool is_obj_valid(lv_obj_t *obj) {
  return obj != nullptr && lv_obj_is_valid(obj);
}

static void apply_cached_state_to_widgets() {
  if (is_obj_valid(s_time_label) && s_cached_hour >= 0) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", s_cached_hour, s_cached_minute, s_cached_second);
    lv_label_set_text(s_time_label, buffer);
  }

  if (is_obj_valid(s_date_label) && s_cached_day >= 0) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", s_cached_day, s_cached_month, s_cached_year);
    lv_label_set_text(s_date_label, buffer);
  }

  if (is_obj_valid(s_weather_label)) {
    lv_label_set_text(s_weather_label, s_cached_weather_valid ? s_cached_weather_condition : "--");
  }

  if (is_obj_valid(s_temp_label)) {
    char temp_buffer[16];
    if (s_cached_weather_valid) {
      snprintf(temp_buffer, sizeof(temp_buffer), "%d °C", s_cached_temp_c);
    } else {
      snprintf(temp_buffer, sizeof(temp_buffer), "-- °C");
    }
    lv_label_set_text(s_temp_label, temp_buffer);
  }
}

void build_screen(lv_obj_t *parent) {
  if (parent == nullptr) {
    return;
  }

  s_time_label = nullptr;
  s_date_label = nullptr;
  s_weather_label = nullptr;
  s_temp_label = nullptr;
  s_arc_loader = nullptr;

  lv_obj_set_style_bg_color(parent, lv_color_hex(0xE9E9E9), 0);

  lv_obj_t *side_card = lv_obj_create(parent);
  lv_obj_set_size(side_card, 480, 320);
  lv_obj_align(side_card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(side_card, 0, 0);
  lv_obj_set_style_bg_color(side_card, lv_color_hex(0xECECEC), 0);
  lv_obj_set_style_border_color(side_card, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_style_pad_all(side_card, 4, 0);

  // Title
  lv_obj_t *title = lv_label_create(side_card);
  lv_label_set_text(title, "Time & Weather");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  // Time Panel
  lv_obj_t *time_panel = lv_obj_create(side_card);
  lv_obj_set_size(time_panel, lv_pct(100), 100);
  lv_obj_align(time_panel, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_style_radius(time_panel, 0, 0);
  lv_obj_set_style_bg_color(time_panel, lv_color_hex(0xF2F2F2), 0);
  lv_obj_set_style_border_color(time_panel, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_scrollbar_mode(time_panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *time_subtitle = lv_label_create(time_panel);
  lv_label_set_text(time_subtitle, "Current Time");
  lv_obj_align(time_subtitle, LV_ALIGN_TOP_MID, 0, 8);

  s_time_label = lv_label_create(time_panel);
  lv_label_set_text(s_time_label, "--:--:--");
  lv_obj_align(s_time_label, LV_ALIGN_BOTTOM_MID, -100, -8);

  s_date_label = lv_label_create(time_panel);
  lv_label_set_text(s_date_label, "-- -- --");
  lv_obj_align(s_date_label, LV_ALIGN_BOTTOM_MID, 100, -8);

  // Weather Panel
  lv_obj_t *weather_panel = lv_obj_create(side_card);
  lv_obj_set_size(weather_panel, lv_pct(100), 120);
  lv_obj_align(weather_panel, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_set_style_radius(weather_panel, 0, 0);
  lv_obj_set_style_bg_color(weather_panel, lv_color_hex(0xF2F2F2), 0);
  lv_obj_set_style_border_color(weather_panel, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_scrollbar_mode(weather_panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *weather_subtitle = lv_label_create(weather_panel);
  lv_label_set_text(weather_subtitle, "Weather");
  lv_obj_align(weather_subtitle, LV_ALIGN_TOP_MID, 0, 8);

  s_weather_label = lv_label_create(weather_panel);
  lv_label_set_text(s_weather_label, "--");
  lv_obj_align(s_weather_label, LV_ALIGN_CENTER, -50, 15);

  s_temp_label = lv_label_create(weather_panel);
  lv_label_set_text(s_temp_label, "-- °C");
  lv_obj_align(s_temp_label, LV_ALIGN_CENTER, 50, 15);

  // Footer
  lv_obj_t *footer = lv_label_create(side_card);
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID().length() > 0) {
    lv_label_set_text_fmt(footer, "WLAN %s: Connected", WiFi.SSID().c_str());
  } else {
    lv_label_set_text(footer, "WLAN: Not connected");
  }
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 2, -2);

  // Create arc loader in footer right corner
  s_arc_loader = lvgl_port::create_arc_loader(side_card, 32);
  lv_obj_align(s_arc_loader, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  apply_cached_state_to_widgets();
}

void set_time(int hour, int minute, int second) {
  s_cached_hour = hour;
  s_cached_minute = minute;
  s_cached_second = second;

  if (!is_obj_valid(s_time_label)) {
    s_time_label = nullptr;
    return;
  }

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, minute, second);
  lv_label_set_text(s_time_label, buffer);
}

void set_date(int day, int month, int year) {
  s_cached_day = day;
  s_cached_month = month;
  s_cached_year = year;

  if (!is_obj_valid(s_date_label)) {
    s_date_label = nullptr;
    return;
  }

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", day, month, year);
  lv_label_set_text(s_date_label, buffer);
}

void set_weather(const char *condition, int temp_c) {
  s_cached_weather_valid = true;
  s_cached_temp_c = temp_c;
  if (condition != nullptr) {
    strncpy(s_cached_weather_condition, condition, sizeof(s_cached_weather_condition) - 1);
    s_cached_weather_condition[sizeof(s_cached_weather_condition) - 1] = '\0';
  } else {
    strncpy(s_cached_weather_condition, "--", sizeof(s_cached_weather_condition) - 1);
    s_cached_weather_condition[sizeof(s_cached_weather_condition) - 1] = '\0';
  }

  if (!is_obj_valid(s_weather_label) || !is_obj_valid(s_temp_label)) {
    s_weather_label = is_obj_valid(s_weather_label) ? s_weather_label : nullptr;
    s_temp_label = is_obj_valid(s_temp_label) ? s_temp_label : nullptr;
    return;
  }

  lv_label_set_text(s_weather_label, condition != nullptr ? condition : "--");

  char temp_buffer[16];
  snprintf(temp_buffer, sizeof(temp_buffer), "%d °C", temp_c);
  lv_label_set_text(s_temp_label, temp_buffer);
}

void set_disconnected() {
  s_cached_hour = -1;
  s_cached_minute = -1;
  s_cached_second = -1;
  s_cached_day = -1;
  s_cached_month = -1;
  s_cached_year = -1;
  s_cached_weather_valid = false;
  strncpy(s_cached_weather_condition, "--", sizeof(s_cached_weather_condition) - 1);
  s_cached_weather_condition[sizeof(s_cached_weather_condition) - 1] = '\0';

  if (is_obj_valid(s_time_label)) {
    lv_label_set_text(s_time_label, "--:--:--");
  }

  if (is_obj_valid(s_date_label)) {
    lv_label_set_text(s_date_label, "-- -- --");
  }

  if (is_obj_valid(s_weather_label)) {
    lv_label_set_text(s_weather_label, "--");
  }

  if (is_obj_valid(s_temp_label)) {
    lv_label_set_text(s_temp_label, "-- °C");
  }
}

void update_arc_progress(int progress_percent) {
  lvgl_port::update_arc_loader(s_arc_loader, progress_percent);
}

}  // namespace side3
