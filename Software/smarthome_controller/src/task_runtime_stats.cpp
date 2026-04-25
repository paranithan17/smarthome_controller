/**
 * @file task_runtime_stats.cpp
 * @brief FreeRTOS task that periodically prints runtime monitoring statistics.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#include "app_tasks.h"

#include <Arduino.h>

#include <freertos/portable.h>

namespace app_tasks {

namespace {

constexpr TickType_t printIntervalTicks = pdMS_TO_TICKS(10000);

struct TaskSnapshot {
  const char *name;
  TaskHandle_t handle;
  UBaseType_t priority;
  uint32_t allocated_stack_words;
};

float calculate_stack_usage_percent(uint32_t allocated_words, uint32_t free_words_high_water) {
  if (allocated_words == 0U) {
    return 0.0F;
  }

  const uint32_t bounded_free_words = (free_words_high_water > allocated_words) ? allocated_words : free_words_high_water;
  const uint32_t used_words = allocated_words - bounded_free_words;
  return (static_cast<float>(used_words) * 100.0F) / static_cast<float>(allocated_words);
}

void print_task_header() {
  Serial.println("Runtime Statistics");
  Serial.println("Task Name        Priority   Stack Allocated   Stack Free   Stack Usage %");
}

#if (configUSE_TRACE_FACILITY == 1)
float get_task_cpu_usage_percent(TaskHandle_t handle,
                                 const TaskStatus_t *status_array,
                                 UBaseType_t status_count,
                                 uint32_t total_runtime) {
  if (handle == nullptr || status_array == nullptr || status_count == 0U || total_runtime == 0U) {
    return -1.0F;
  }

  for (UBaseType_t i = 0; i < status_count; i++) {
    if (status_array[i].xHandle == handle) {
      return (static_cast<float>(status_array[i].ulRunTimeCounter) * 100.0F) / static_cast<float>(total_runtime);
    }
  }

  return -1.0F;
}
#endif

void print_task_row(const TaskSnapshot &snapshot
#if (configUSE_TRACE_FACILITY == 1)
                    , const TaskStatus_t *status_array,
                    UBaseType_t status_count,
                    uint32_t total_runtime
#endif
                    ) {
  const uint32_t free_words = uxTaskGetStackHighWaterMark(snapshot.handle);
  const float usage_percent = calculate_stack_usage_percent(snapshot.allocated_stack_words, free_words);

  // High-water mark means the minimum free stack observed so far. Lower values indicate tighter stack margin.
  Serial.printf("%-15s %-10u %-16u %-12u %6.1f%%",
                snapshot.name,
                static_cast<unsigned>(snapshot.priority),
                static_cast<unsigned>(snapshot.allocated_stack_words),
                static_cast<unsigned>(free_words),
                static_cast<double>(usage_percent));

#if (configUSE_TRACE_FACILITY == 1)
  const float cpu_percent = get_task_cpu_usage_percent(snapshot.handle, status_array, status_count, total_runtime);
  if (cpu_percent >= 0.0F) {
    Serial.printf("   CPU %6.2f%%", static_cast<double>(cpu_percent));
  } else {
    Serial.print("   CPU n/a");
  }
#endif

  Serial.println();
}

}  // namespace

void runtime_stats_task(void *arg) {
  const RuntimeStatsTaskParams *params = static_cast<const RuntimeStatsTaskParams *>(arg);
  if (params == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  const TaskSnapshot monitored_tasks[] = {
      {"lamp_control", params->lamp_task, priorityControl, stackLampControl},
      {"LED_control", params->led_task, priorityControl, stackLedControl},
      {"display_UI", params->display_task, priorityDisplay, stackDisplayUi},
      {"time_task", params->time_task, priorityTime, stackTimeTask},
      {"weather_task", params->weather_task, priorityWeather, stackWeatherTask},
      {"runtime_stats", xTaskGetCurrentTaskHandle(), priorityRuntimeStats, stackRuntimeStatsTask},
  };

  for (;;) {
    const uint32_t uptime_sec = millis() / 1000U;
    Serial.println();
    Serial.printf("[RUNTIME] Uptime: %lu s\n", static_cast<unsigned long>(uptime_sec));
    print_task_header();

#if (configUSE_TRACE_FACILITY == 1)
    const UBaseType_t task_count_now = uxTaskGetNumberOfTasks();
    TaskStatus_t *status_array = static_cast<TaskStatus_t *>(pvPortMalloc(task_count_now * sizeof(TaskStatus_t)));
    UBaseType_t status_count = 0;
    uint32_t total_runtime = 0;
    if (status_array != nullptr) {
      status_count = uxTaskGetSystemState(status_array, task_count_now, &total_runtime);
    }
#endif

    for (size_t i = 0; i < (sizeof(monitored_tasks) / sizeof(monitored_tasks[0])); i++) {
      print_task_row(monitored_tasks[i]
#if (configUSE_TRACE_FACILITY == 1)
                     , status_array,
                     status_count,
                     total_runtime
#endif
      );
    }

#if (configUSE_TRACE_FACILITY == 1)
    if (status_array != nullptr) {
      vPortFree(status_array);
    }
#endif

    Serial.println();
    Serial.println("System Statistics");
    Serial.printf("Number of tasks: %u\n", static_cast<unsigned>(uxTaskGetNumberOfTasks()));
    Serial.printf("Free heap: %u bytes\n", static_cast<unsigned>(xPortGetFreeHeapSize()));

#if (configUSE_TRACE_FACILITY != 1)
    Serial.println("CPU runtime statistics: unavailable (configUSE_TRACE_FACILITY is disabled)");
#endif

    vTaskDelay(printIntervalTicks);
  }
}

}  // namespace app_tasks
