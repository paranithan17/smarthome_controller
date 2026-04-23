/**
 * @file app_tasks.h
 * @brief Declares all FreeRTOS task and timer callback entry points for the application.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

namespace app_tasks {

void lamp_control(void *arg);
void led_control(void *arg);
void display_ui(void *arg);
void time_task(void *arg);
void weather_task(void *arg);
void weather_timer_callback(TimerHandle_t timer_handle);

}  // namespace app_tasks
