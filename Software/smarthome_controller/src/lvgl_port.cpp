/**
 * @file lvgl_port.cpp
 * @brief LVGL display backend for TFT_eSPI rendering and periodic handler execution.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "lvgl_port.h"

#include <TFT_eSPI.h>
#include <lvgl.h>

namespace lvgl_port {

namespace {

TFT_eSPI tft = TFT_eSPI();
lv_display_t *display = nullptr;

constexpr uint16_t screenWidth = 480;
constexpr uint16_t screenHeight = 320;
constexpr uint16_t bufferLines = 4;

static lv_color_t draw_buf_1[screenWidth * bufferLines];

uint32_t last_tick_ms = 0;

void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t *>(px_map), width * height, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

}  // namespace

void init() {
  lv_init();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(display, display_flush_cb);
  lv_display_set_buffers(display, draw_buf_1, nullptr, sizeof(draw_buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  last_tick_ms = millis();
}

void task_handler() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - last_tick_ms;

  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    last_tick_ms = now;
  }

  lv_timer_handler();
}

lv_obj_t *create_arc_loader(lv_obj_t *parent, int size) {
  if (parent == nullptr) {
    return nullptr;
  }

  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, size, 18);
  lv_arc_set_range(arc, 0, 100);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_arc_set_value(arc, 0);
  lv_arc_set_bg_angles(arc, 0, 360);

  // Style the arc
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);

  return arc;
}

void update_arc_loader(lv_obj_t *arc, int progress) {
  if (arc == nullptr) {
    return;
  }

  // Clamp progress to 0-100
  const int clamped = constrain(progress, 0, 100);
  lv_arc_set_value(arc, clamped);
}

}  // namespace lvgl_port

