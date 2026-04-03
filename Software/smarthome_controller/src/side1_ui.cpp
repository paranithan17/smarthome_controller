#include "side1_ui.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_controls.h"
#include "lvgl_port.h"

namespace side1 {

static const char *kColorNames[3] = {"Red", "Green", "Blue"};

static lv_obj_t *s_lamp_button = nullptr;
static lv_obj_t *s_lamp_button_label = nullptr;
static lv_obj_t *s_active_channel_label = nullptr;
static lv_obj_t *s_sliders[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *s_slider_values[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *s_channel_switches[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *s_arc_loader = nullptr;

static uint8_t s_cached_rgb_values[3] = {0, 0, 0};
static uint8_t s_cached_active_channel = 0;
static bool s_cached_lamp_on = false;

static bool is_obj_valid(lv_obj_t *obj) {
  return obj != nullptr && lv_obj_is_valid(obj);
}

static void update_slider_value_label(uint8_t index) {
  if (index >= 3 || !is_obj_valid(s_sliders[index]) || !is_obj_valid(s_slider_values[index])) {
    return;
  }

  char buffer[16];
  const int value = lv_slider_get_value(s_sliders[index]);
  snprintf(buffer, sizeof(buffer), "%d", value);
  lv_label_set_text(s_slider_values[index], buffer);
}

static void set_channel_selected(uint8_t index) {
  if (index >= 3) {
    return;
  }

  for (uint8_t i = 0; i < 3; i++) {
    if (!is_obj_valid(s_channel_switches[i])) {
      s_channel_switches[i] = nullptr;
      continue;
    }

    if (i == index) {
      lv_obj_add_state(s_channel_switches[i], LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(s_channel_switches[i], LV_STATE_CHECKED);
    }
  }

  if (is_obj_valid(s_active_channel_label)) {
    lv_label_set_text_fmt(s_active_channel_label, "Selected: %s", kColorNames[index]);
  }
}

static void set_lamp_label(bool on) {
  if (!is_obj_valid(s_lamp_button_label)) {
    s_lamp_button_label = nullptr;
    return;
  }

  lv_label_set_text(s_lamp_button_label, on ? "ON" : "OFF");

  if (is_obj_valid(s_lamp_button)) {
    if (on) {
      lv_obj_add_state(s_lamp_button, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(s_lamp_button, LV_STATE_CHECKED);
    }
  } else {
    s_lamp_button = nullptr;
  }
}

static void on_lamp_button_clicked(lv_event_t *e) {
  LV_UNUSED(e);
  app_controls::toggle_lamp();
}

static void init_channel_widgets() {
  for (uint8_t i = 0; i < 3; i++) {
    update_slider_value_label(i);
  }
}

static void apply_cached_state_to_widgets() {
  set_lamp_label(s_cached_lamp_on);

  for (uint8_t i = 0; i < 3; i++) {
    if (!is_obj_valid(s_sliders[i])) {
      s_sliders[i] = nullptr;
      continue;
    }

    lv_slider_set_value(s_sliders[i], s_cached_rgb_values[i], LV_ANIM_OFF);
    update_slider_value_label(i);
  }

  set_channel_selected(s_cached_active_channel);
}

void build_screen(lv_obj_t *parent) {
  if (parent == nullptr) {
    return;
  }

  // Reset references before creating new widgets for this rebuilt page.
  s_lamp_button = nullptr;
  s_lamp_button_label = nullptr;
  s_active_channel_label = nullptr;
  for (uint8_t i = 0; i < 3; i++) {
    s_sliders[i] = nullptr;
    s_slider_values[i] = nullptr;
    s_channel_switches[i] = nullptr;
  }
  s_arc_loader = nullptr;

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

  lv_obj_t *room_title = lv_label_create(top_panel);
  lv_label_set_text(room_title, "Lamp");
  lv_obj_align(room_title, LV_ALIGN_TOP_MID, 0, 8);



  s_lamp_button = lv_btn_create(top_panel);
  lv_obj_set_size(s_lamp_button, 136, 56);
  lv_obj_align(s_lamp_button, LV_ALIGN_CENTER, 0, 18);
  lv_obj_add_event_cb(s_lamp_button, on_lamp_button_clicked, LV_EVENT_CLICKED, nullptr);

  s_lamp_button_label = lv_label_create(s_lamp_button);
  lv_label_set_text(s_lamp_button_label, "OFF");
  lv_obj_center(s_lamp_button_label);

 // s_active_channel_label = lv_label_create(top_panel);
 // lv_label_set_text(s_active_channel_label, "Selected: Red");
 // lv_obj_align(s_active_channel_label, LV_ALIGN_BOTTOM_MID, 0, -12);

  lv_obj_t *bottom_panel = lv_obj_create(side_card);
  lv_obj_set_size(bottom_panel, lv_pct(100), 154);
  lv_obj_align(bottom_panel, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_set_style_radius(bottom_panel, 0, 0);
  lv_obj_set_style_bg_color(bottom_panel, lv_color_hex(0xF2F2F2), 0);
  lv_obj_set_style_border_color(bottom_panel, lv_color_hex(0x6A6A6A), 0);
  lv_obj_set_style_pad_row(bottom_panel, 6, 0);
  lv_obj_set_scrollbar_mode(bottom_panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *led_title = lv_label_create(bottom_panel);
  lv_label_set_text(led_title, "LED");
  lv_obj_align(led_title, LV_ALIGN_TOP_MID, 0, 4);

  for (uint8_t i = 0; i < 3; i++) {
    lv_obj_t *name = lv_label_create(bottom_panel);
    lv_label_set_text(name, kColorNames[i]);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 12, 30 + (i * 36));

    s_sliders[i] = lv_slider_create(bottom_panel);
    lv_obj_set_size(s_sliders[i], 140, 8);
    lv_obj_align(s_sliders[i], LV_ALIGN_TOP_LEFT, 10, 46 + (i * 36));
    lv_slider_set_range(s_sliders[i], 0, 255);
    lv_slider_set_value(s_sliders[i], 0, LV_ANIM_OFF);

    s_slider_values[i] = lv_label_create(bottom_panel);
    lv_obj_align(s_slider_values[i], LV_ALIGN_TOP_LEFT, 160, 42 + (i * 36));
    update_slider_value_label(i);

    s_channel_switches[i] = lv_switch_create(bottom_panel);
    lv_obj_set_size(s_channel_switches[i], 32, 18);
    lv_obj_align(s_channel_switches[i], LV_ALIGN_TOP_LEFT, 202, 40 + (i * 36));
  }

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

  init_channel_widgets();
  app_controls::sync_state_to_ui();
  apply_cached_state_to_widgets();
}

void set_lamp_state(bool on) {
  s_cached_lamp_on = on;
  set_lamp_label(on);
}

void set_rgb_channel_value(uint8_t index, uint8_t value) {
  if (index >= 3) {
    return;
  }

  s_cached_rgb_values[index] = value;

  if (!is_obj_valid(s_sliders[index])) {
    s_sliders[index] = nullptr;
    return;
  }

  lv_slider_set_value(s_sliders[index], value, LV_ANIM_OFF);
  update_slider_value_label(index);
}

void set_active_channel(uint8_t index) {
  s_cached_active_channel = index % 3;
  set_channel_selected(s_cached_active_channel);
}

void set_rgb_state(uint8_t red, uint8_t green, uint8_t blue, uint8_t active_index) {
  set_rgb_channel_value(0, red);
  set_rgb_channel_value(1, green);
  set_rgb_channel_value(2, blue);
  set_active_channel(active_index);
}

void update_arc_progress(int progress_percent) {
  lvgl_port::update_arc_loader(s_arc_loader, progress_percent);
}

}  // namespace side1




