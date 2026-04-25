/**
 * @file app_tasks.h
 * @brief Declares all FreeRTOS task and timer callback entry points for the application.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

namespace app_tasks {

// Task priorities from highest to lowest across control and background work.
constexpr UBaseType_t priorityControl = 5;
constexpr UBaseType_t priorityDisplay = 4;
constexpr UBaseType_t priorityTime = 3;
constexpr UBaseType_t priorityWeather = 2;
constexpr UBaseType_t priorityRuntimeStats = 1;

// Per-task stack sizes tuned for display and networking workloads.
constexpr uint32_t stackLampControl = 4096;
constexpr uint32_t stackLedControl = 4096;
constexpr uint32_t stackDisplayUi = 8192;
constexpr uint32_t stackTimeTask = 4096;
constexpr uint32_t stackWeatherTask = 6144;
constexpr uint32_t stackRuntimeStatsTask = 4096;

/**
 * @brief Task handle bundle used to pass runtime task references together.
 */
struct RuntimeStatsTaskParams {
	/** @brief Lamp control task handle. */
	TaskHandle_t lamp_task;
	/** @brief LED control task handle. */
	TaskHandle_t led_task;
	/** @brief Display/UI task handle. */
	TaskHandle_t display_task;
	/** @brief Time task handle. */
	TaskHandle_t time_task;
	/** @brief Weather task handle. */
	TaskHandle_t weather_task;
};

/**
 * @brief FreeRTOS task entry point for lamp control.
 *
 * @param arg Opaque task parameter, unused.
 */
void lamp_control(void *arg);
/**
 * @brief FreeRTOS task entry point for joystick-driven LED control.
 *
 * @param arg Opaque task parameter, unused.
 */
void led_control(void *arg);
/**
 * @brief FreeRTOS task entry point owning all LVGL display updates.
 *
 * @param arg Opaque task parameter, unused.
 */
void display_ui(void *arg);
/**
 * @brief FreeRTOS task entry point that publishes time data to the UI.
 *
 * @param arg Opaque task parameter, unused.
 */
void time_task(void *arg);
/**
 * @brief FreeRTOS task entry point that fetches and stores weather data.
 *
 * @param arg Opaque task parameter, unused.
 */
void weather_task(void *arg);
/**
 * @brief FreeRTOS task entry point for runtime statistics collection.
 *
 * @param arg Pointer to RuntimeStatsTaskParams, or nullptr if unused.
 */
void runtime_stats_task(void *arg);
/**
 * @brief Timer callback used to trigger weather polling.
 *
 * @param timer_handle FreeRTOS software timer handle.
 */
void weather_timer_callback(TimerHandle_t timer_handle);

}  // namespace app_tasks
