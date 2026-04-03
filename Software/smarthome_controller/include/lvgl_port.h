#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace lvgl_port {

void init();
void task_handler();

// Create an arc loader widget showing progress (0-100%)
// Returns the arc object
lv_obj_t *create_arc_loader(lv_obj_t *parent, int size);

// Update arc loader progress (0-100)
void update_arc_loader(lv_obj_t *arc, int progress);

}  // namespace lvgl_port
