#include "app_controls.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "side1_ui.h"

namespace app_controls {

namespace {

constexpr int kRedPwmPin = 14;
constexpr int kGreenPwmPin = 12;
constexpr int kBluePwmPin = 13;

// Use an ADC1 pin for joystick axis because ADC2 pins are blocked when WiFi is active on ESP32.
constexpr int kJoystickXPin = 34;
constexpr int kJoystickSwitchPin = 26;
constexpr int kLampButtonPin = 25;

constexpr uint8_t kChannelCount = 3;
constexpr uint32_t kButtonDebounceMs = 60;
constexpr uint32_t kJoystickUpdateMs = 30;
constexpr int kJoystickDeadzone = 220;
constexpr int kJoystickStrongTilt = 900;
constexpr uint32_t kPwmFrequencyHz = 5000;
constexpr uint8_t kPwmResolutionBits = 8;

constexpr uint8_t kLedcChannelRed = 0;
constexpr uint8_t kLedcChannelGreen = 1;
constexpr uint8_t kLedcChannelBlue = 2;
constexpr char kLampRelayBaseUrl[] = "http://192.168.1.15/relay/0?turn=";

uint8_t rgb_values[kChannelCount] = {0, 0, 0};
uint8_t active_channel = 0;
bool lamp_on = false;

int last_joystick_switch_state = HIGH;
int last_lamp_button_state = HIGH;

uint32_t last_joystick_update_ms = 0;
uint32_t last_joystick_switch_ms = 0;
uint32_t last_lamp_button_ms = 0;
uint32_t last_interaction_ms = 0;

int joystick_center_x = 2048;

void apply_pwm() {
  ledcWrite(kLedcChannelRed, rgb_values[0]);
  ledcWrite(kLedcChannelGreen, rgb_values[1]);
  ledcWrite(kLedcChannelBlue, rgb_values[2]);
}

void sync_ui() {
  side1::set_rgb_state(rgb_values[0], rgb_values[1], rgb_values[2], active_channel);
  side1::set_lamp_state(lamp_on);
}

bool send_lamp_http_request(bool on) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LAMP] Skipped HTTP request: WLAN not connected");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = kLampRelayBaseUrl;
  url += on ? "on" : "off";

  if (!http.begin(client, url)) {
    Serial.printf("[LAMP] Failed to begin HTTP request: %s\n", url.c_str());
    return false;
  }

  const int http_code = http.GET();
  if (http_code <= 0) {
    Serial.printf("[LAMP] HTTP request failed: %s, code=%d\n", url.c_str(), http_code);
    http.end();
    return false;
  }

  const String response = http.getString();
  http.end();

  Serial.printf("[LAMP] Relay %s request sent, code=%d\n", on ? "ON" : "OFF", http_code);
  if (response.length() > 0) {
    Serial.printf("[LAMP] Response: %s\n", response.c_str());
  }

  return true;
}

void select_next_channel() {
  active_channel = static_cast<uint8_t>((active_channel + 1) % kChannelCount);
  last_interaction_ms = millis();
  side1::set_active_channel(active_channel);
}

void adjust_active_channel(int delta) {
  const int next_value = static_cast<int>(rgb_values[active_channel]) + delta;
  const uint8_t clamped_value = static_cast<uint8_t>(constrain(next_value, 0, 255));

  if (clamped_value == rgb_values[active_channel]) {
    return;
  }

  rgb_values[active_channel] = clamped_value;
  last_interaction_ms = millis();
  apply_pwm();
  side1::set_rgb_channel_value(active_channel, clamped_value);
}

void update_joystick_axis() {
  const uint32_t now = millis();
  if (now - last_joystick_update_ms < kJoystickUpdateMs) {
    return;
  }

  last_joystick_update_ms = now;

  const int raw_x = analogRead(kJoystickXPin);
  const int delta = raw_x - joystick_center_x;
  const int magnitude = abs(delta);

  if (magnitude <= kJoystickDeadzone) {
    return;
  }

  int step = 1;
  if (magnitude > kJoystickStrongTilt) {
    step = 4;
  } else if (magnitude > kJoystickStrongTilt / 2) {
    step = 2;
  }

  if (delta > 0) {
    adjust_active_channel(step);
  } else {
    adjust_active_channel(-step);
  }
}

void calibrate_joystick_center() {
  int total = 0;
  constexpr int kSampleCount = 16;

  for (int i = 0; i < kSampleCount; i++) {
    total += analogRead(kJoystickXPin);
    delay(2);
  }

  joystick_center_x = total / kSampleCount;
}

void update_joystick_switch() {
  const int switch_state = digitalRead(kJoystickSwitchPin);
  if (switch_state == last_joystick_switch_state) {
    return;
  }

  const uint32_t now = millis();
  if (now - last_joystick_switch_ms < kButtonDebounceMs) {
    last_joystick_switch_state = switch_state;
    return;
  }

  last_joystick_switch_ms = now;
  last_joystick_switch_state = switch_state;

  if (switch_state == LOW) {
    select_next_channel();
  }
}

void update_lamp_button() {
  if (kLampButtonPin < 0) {
    return;
  }

  const int button_state = digitalRead(kLampButtonPin);
  if (button_state == last_lamp_button_state) {
    return;
  }

  const uint32_t now = millis();
  if (now - last_lamp_button_ms < kButtonDebounceMs) {
    last_lamp_button_state = button_state;
    return;
  }

  last_lamp_button_ms = now;
  last_lamp_button_state = button_state;

  if (button_state == LOW) {
    toggle_lamp();
  }
}

}  // namespace

void init() {
  pinMode(kRedPwmPin, OUTPUT);
  pinMode(kGreenPwmPin, OUTPUT);
  pinMode(kBluePwmPin, OUTPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(kJoystickXPin, ADC_11db);

  ledcSetup(kLedcChannelRed, kPwmFrequencyHz, kPwmResolutionBits);
  ledcSetup(kLedcChannelGreen, kPwmFrequencyHz, kPwmResolutionBits);
  ledcSetup(kLedcChannelBlue, kPwmFrequencyHz, kPwmResolutionBits);

  ledcAttachPin(kRedPwmPin, kLedcChannelRed);
  ledcAttachPin(kGreenPwmPin, kLedcChannelGreen);
  ledcAttachPin(kBluePwmPin, kLedcChannelBlue);

  pinMode(kJoystickSwitchPin, INPUT_PULLUP);

  if (kLampButtonPin >= 0) {
    pinMode(kLampButtonPin, INPUT_PULLUP);
    last_lamp_button_state = digitalRead(kLampButtonPin);
  }

  last_joystick_switch_state = digitalRead(kJoystickSwitchPin);
  calibrate_joystick_center();
  last_joystick_update_ms = millis();

  apply_pwm();
  sync_ui();
}

void toggle_lamp() {
  set_lamp(!lamp_on);
}

bool set_lamp(bool on) {
  if (lamp_on == on) {
    last_interaction_ms = millis();
    side1::set_lamp_state(lamp_on);
    return true;
  }

  if (!send_lamp_http_request(on)) {
    return false;
  }

  lamp_on = on;
  last_interaction_ms = millis();
  side1::set_lamp_state(lamp_on);
  return true;
}

void sync_state_to_ui() {
  sync_ui();
}

uint32_t get_ms_since_last_interaction() {
  return millis() - last_interaction_ms;
}

void reset_interaction_timer() {
  last_interaction_ms = millis();
}

void update() {
  update_joystick_axis();
  update_joystick_switch();
  update_lamp_button();
}

}  // namespace app_controls