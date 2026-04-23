/**
 * @file app_controls.cpp
 * @brief Hardware input/output control implementation for lamp and RGB handling.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_controls.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_timer.h>

namespace app_controls {

namespace {

constexpr int redPwmPin = 14;
constexpr int greenPwmPin = 12;
constexpr int bluePwmPin = 13;

// Use an ADC1 pin for joystick axis because ADC2 pins are blocked when WiFi is active on ESP32.
constexpr int joystickXPin = 34;
constexpr int joystickSwitchPin = 26;
constexpr int lampButtonPin = 25;

constexpr uint8_t channelCount = 3;
constexpr uint32_t buttonDebounceMs = 60;
constexpr uint32_t joystickUpdateMs = 30;
constexpr int joystickDeadzone = 220;
constexpr int joystickStrongTilt = 900;
constexpr uint32_t pwmFrequencyHz = 5000;
constexpr uint8_t pwmResolutionBits = 8;

constexpr uint8_t ledcChannelRed = 0;
constexpr uint8_t ledcChannelGreen = 1;
constexpr uint8_t ledcChannelBlue = 2;
constexpr char lampRelayBaseUrl[] = "http://192.168.1.15/relay/0?turn=";

uint8_t rgb_values[channelCount] = {0, 0, 0};
uint8_t active_channel = 0;
bool lamp_on = false;

SemaphoreHandle_t s_led_state_mutex = nullptr;

int last_joystick_switch_state = HIGH;
int last_lamp_button_state = HIGH;

uint32_t last_joystick_update_ms = 0;
uint32_t last_joystick_switch_ms = 0;
uint32_t last_lamp_button_ms = 0;
uint32_t last_interaction_ms = 0;

int joystick_center_x = 2048;
QueueHandle_t s_lamp_command_queue = nullptr;

TaskHandle_t s_lamp_task_handle = nullptr;
TaskHandle_t s_led_task_handle = nullptr;

esp_timer_handle_t s_joystick_timer = nullptr;

void apply_pwm() {
  ledcWrite(ledcChannelRed, rgb_values[0]);
  ledcWrite(ledcChannelGreen, rgb_values[1]);
  ledcWrite(ledcChannelBlue, rgb_values[2]);
}

void notify_task_from_isr(TaskHandle_t task_handle) {
  if (task_handle == nullptr) {
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(task_handle, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void IRAM_ATTR lamp_button_isr() {
  notify_task_from_isr(s_lamp_task_handle);
}

void IRAM_ATTR joystick_button_isr() {
  notify_task_from_isr(s_led_task_handle);
}

void joystick_timer_callback(void *arg) {
  (void)arg;
  if (s_led_task_handle != nullptr) {
    xTaskNotifyGive(s_led_task_handle);
  }
}

bool send_lamp_http_request(bool on) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LAMP] Skipped HTTP request: WLAN not connected");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = lampRelayBaseUrl;
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
  if (s_led_state_mutex != nullptr) {
    xSemaphoreTake(s_led_state_mutex, portMAX_DELAY);
  }
  active_channel = static_cast<uint8_t>((active_channel + 1) % channelCount);
  if (s_led_state_mutex != nullptr) {
    xSemaphoreGive(s_led_state_mutex);
  }
  last_interaction_ms = millis();
}

void adjust_active_channel(int delta) {
  if (s_led_state_mutex != nullptr) {
    xSemaphoreTake(s_led_state_mutex, portMAX_DELAY);
  }

  const int next_value = static_cast<int>(rgb_values[active_channel]) + delta;
  const uint8_t clamped_value = static_cast<uint8_t>(constrain(next_value, 0, 255));

  if (clamped_value == rgb_values[active_channel]) {
    if (s_led_state_mutex != nullptr) {
      xSemaphoreGive(s_led_state_mutex);
    }
    return;
  }

  rgb_values[active_channel] = clamped_value;
  apply_pwm();

  if (s_led_state_mutex != nullptr) {
    xSemaphoreGive(s_led_state_mutex);
  }

  last_interaction_ms = millis();
}

bool update_joystick_axis() {
  const uint32_t now = millis();
  if (now - last_joystick_update_ms < joystickUpdateMs) {
    return false;
  }

  last_joystick_update_ms = now;

  const int raw_x = analogRead(joystickXPin);
  const int delta = raw_x - joystick_center_x;
  const int magnitude = abs(delta);

  if (magnitude <= joystickDeadzone) {
    return false;
  }

  int step = 1;
  if (magnitude > joystickStrongTilt) {
    step = 4;
  } else if (magnitude > joystickStrongTilt / 2) {
    step = 2;
  }

  if (delta > 0) {
    adjust_active_channel(step);
  } else {
    adjust_active_channel(-step);
  }

  return true;
}

void calibrate_joystick_center() {
  int total = 0;
  constexpr int sampleCount = 16;

  for (int i = 0; i < sampleCount; i++) {
    total += analogRead(joystickXPin);
    delay(2);
  }

  joystick_center_x = total / sampleCount;
}

bool update_joystick_switch() {
  const int switch_state = digitalRead(joystickSwitchPin);
  if (switch_state == last_joystick_switch_state) {
    return false;
  }

  const uint32_t now = millis();
  if (now - last_joystick_switch_ms < buttonDebounceMs) {
    last_joystick_switch_state = switch_state;
    return false;
  }

  last_joystick_switch_ms = now;
  last_joystick_switch_state = switch_state;

  if (switch_state == LOW) {
    select_next_channel();
    return true;
  }

  return false;
}

bool update_lamp_button() {
  if (lampButtonPin < 0) {
    return false;
  }

  const int button_state = digitalRead(lampButtonPin);
  if (button_state == last_lamp_button_state) {
    return false;
  }

  const uint32_t now = millis();
  if (now - last_lamp_button_ms < buttonDebounceMs) {
    last_lamp_button_state = button_state;
    return false;
  }

  last_lamp_button_ms = now;
  last_lamp_button_state = button_state;

  if (button_state == LOW) {
    toggle_lamp();
    return true;
  }

  return false;
}

}  // namespace

void init() {
  pinMode(redPwmPin, OUTPUT);
  pinMode(greenPwmPin, OUTPUT);
  pinMode(bluePwmPin, OUTPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(joystickXPin, ADC_11db);

  ledcSetup(ledcChannelRed, pwmFrequencyHz, pwmResolutionBits);
  ledcSetup(ledcChannelGreen, pwmFrequencyHz, pwmResolutionBits);
  ledcSetup(ledcChannelBlue, pwmFrequencyHz, pwmResolutionBits);

  ledcAttachPin(redPwmPin, ledcChannelRed);
  ledcAttachPin(greenPwmPin, ledcChannelGreen);
  ledcAttachPin(bluePwmPin, ledcChannelBlue);

  pinMode(joystickSwitchPin, INPUT_PULLUP);

  if (lampButtonPin >= 0) {
    pinMode(lampButtonPin, INPUT_PULLUP);
    last_lamp_button_state = digitalRead(lampButtonPin);
  }

  last_joystick_switch_state = digitalRead(joystickSwitchPin);
  calibrate_joystick_center();
  last_joystick_update_ms = millis();
  last_interaction_ms = millis();

  if (s_lamp_command_queue == nullptr) {
    s_lamp_command_queue = xQueueCreate(1, sizeof(bool));
  }

  if (s_led_state_mutex == nullptr) {
    s_led_state_mutex = xSemaphoreCreateMutex();
  }

  attachInterrupt(digitalPinToInterrupt(joystickSwitchPin), joystick_button_isr, CHANGE);

  if (lampButtonPin >= 0) {
    attachInterrupt(digitalPinToInterrupt(lampButtonPin), lamp_button_isr, CHANGE);
  }

  if (s_joystick_timer == nullptr) {
    const esp_timer_create_args_t timer_args = {
        .callback = &joystick_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "joy_poll"};

    if (esp_timer_create(&timer_args, &s_joystick_timer) == ESP_OK) {
      esp_timer_start_periodic(s_joystick_timer, static_cast<uint64_t>(joystickUpdateMs) * 1000ULL);
    }
  }

  apply_pwm();
}

void register_task_handles(TaskHandle_t lamp_task, TaskHandle_t led_task) {
  s_lamp_task_handle = lamp_task;
  s_led_task_handle = led_task;
}

bool process_lamp_button_event() {
  return update_lamp_button();
}

bool process_joystick_button_event() {
  return update_joystick_switch();
}

bool process_joystick_axis_event() {
  return update_joystick_axis();
}

void toggle_lamp() {
  set_lamp(!lamp_on);
}

bool set_lamp(bool on) {
  if (lamp_on == on) {
    last_interaction_ms = millis();
    return true;
  }

  lamp_on = on;
  last_interaction_ms = millis();

  if (s_lamp_command_queue != nullptr) {
    xQueueOverwrite(s_lamp_command_queue, &on);
  }

  return true;
}

bool get_lamp_state() {
  return lamp_on;
}

void get_led_state(uint8_t &red, uint8_t &green, uint8_t &blue, uint8_t &channel) {
  if (s_led_state_mutex != nullptr) {
    xSemaphoreTake(s_led_state_mutex, portMAX_DELAY);
  }

  red = rgb_values[0];
  green = rgb_values[1];
  blue = rgb_values[2];
  channel = active_channel;

  if (s_led_state_mutex != nullptr) {
    xSemaphoreGive(s_led_state_mutex);
  }
}

bool receive_lamp_command(bool &desired_on, TickType_t wait_ticks) {
  if (s_lamp_command_queue == nullptr) {
    return false;
  }

  return xQueueReceive(s_lamp_command_queue, &desired_on, wait_ticks) == pdTRUE;
}

bool send_lamp_http(bool on) {
  return send_lamp_http_request(on);
}

uint32_t get_ms_since_last_interaction() {
  return millis() - last_interaction_ms;
}

void reset_interaction_timer() {
  last_interaction_ms = millis();
}

}  // namespace app_controls