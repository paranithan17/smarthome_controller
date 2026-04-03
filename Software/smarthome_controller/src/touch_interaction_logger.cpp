#include "touch_interaction_logger.h"

#include <Arduino.h>

namespace touch_logger {

void init() {
  Serial.println("[TOUCH] Disabled: display input is now rotary/button based");
}

void update() {
}

}  // namespace touch_logger
