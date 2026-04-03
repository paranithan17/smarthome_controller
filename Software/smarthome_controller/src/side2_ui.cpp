#include "side2_ui.h"

#include <WiFi.h>

#include "lvgl_port.h"

namespace side2 {

namespace {

static lv_obj_t *s_temperature_value = nullptr;
static lv_obj_t *s_humidity_value = nullptr;
static lv_obj_t *s_arc_loader = nullptr;

void update_temperature_label(int16_t temperature_c) {
  if (s_temperature_value == nullptr) {
    return;
  }

  lv_label_set_text_fmt(s_temperature_value, "%d\u00B0C", temperature_c);
}

void update_humidity_label(uint8_t humidity_percent) {
  if (s_humidity_value == nullptr) {
    return;
  }

  lv_label_set_text_fmt(s_humidity_value, "%u%%", humidity_percent);
}

void set_disconnected_labels() {
  if (s_temperature_value != nullptr) {
    lv_label_set_text(s_temperature_value, "-- \u00B0C");
  }

  if (s_humidity_value != nullptr) {
    lv_label_set_text(s_humidity_value, "-- %");
  }
}

}  // namespace

void build_screen(lv_obj_t *parent) {
  if (parent == nullptr) {
    return;
  }

  lv_obj_set_style_bg_color(parent, lv_color_hex(0xE9E9E9), 0);

  lv_obj_t *side_card = lv_obj_create(parent);
  lv_obj_set_size(side_card, 480, 320);
  lv_obj_align(side_card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(side_card, 0, 0);
  lv_obj_set_style_bg_color(side_card, lv_color_hex(0xECECEC), 0);
  lv_obj_set_style_border_color(side_card, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_style_pad_all(side_card, 4, 0);

  lv_obj_t *top_panel = lv_obj_create(side_card);
  lv_obj_set_size(top_panel, lv_pct(100), 128);
  lv_obj_align(top_panel, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(top_panel, 0, 0);
  lv_obj_set_style_bg_color(top_panel, lv_color_hex(0xF2F2F2), 0);
  lv_obj_set_style_border_color(top_panel, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_scrollbar_mode(top_panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *temperature_title = lv_label_create(top_panel);
  lv_label_set_text(temperature_title, "Room - Temperature");
  lv_obj_align(temperature_title, LV_ALIGN_TOP_MID, 0, 16);

  s_temperature_value = lv_label_create(top_panel);
  lv_label_set_text(s_temperature_value, "-- \u00B0C");
  lv_obj_align(s_temperature_value, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_obj_t *bottom_panel = lv_obj_create(side_card);
  lv_obj_set_size(bottom_panel, lv_pct(100), 154);
  lv_obj_align(bottom_panel, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_set_style_radius(bottom_panel, 0, 0);
  lv_obj_set_style_bg_color(bottom_panel, lv_color_hex(0xF2F2F2), 0);
  lv_obj_set_style_border_color(bottom_panel, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_scrollbar_mode(bottom_panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *humidity_title = lv_label_create(bottom_panel);
  lv_label_set_text(humidity_title, "Room - Humidity");
  lv_obj_align(humidity_title, LV_ALIGN_TOP_MID, 0, 24);

  s_humidity_value = lv_label_create(bottom_panel);
  lv_label_set_text(s_humidity_value, "-- %");
  lv_obj_align(s_humidity_value, LV_ALIGN_BOTTOM_MID, 0, -12);

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
}

void set_readings(int16_t temperature_c, uint8_t humidity_percent) {
  if (humidity_percent > 100 || temperature_c < -40 || temperature_c > 125) {
    set_disconnected_labels();
    return;
  }

  update_temperature_label(temperature_c);
  update_humidity_label(humidity_percent);
}

void set_sensor_disconnected() {
  set_disconnected_labels();
}

void update_arc_progress(int progress_percent) {
  lvgl_port::update_arc_loader(s_arc_loader, progress_percent);
}

}  // namespace side2
