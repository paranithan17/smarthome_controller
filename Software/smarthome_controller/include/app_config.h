/**
 * @file app_config.h
 * @brief Central runtime configuration for WLAN, weather API, and timezone settings.
 * @author Paranithan Paramalingam (BFH-TI)
 */
#pragma once

namespace app_config {

// WLAN credentials.
constexpr char wlanSsid[] = "dil-84400";
constexpr char wlanPassword[] = "Rhyana240215";

// OpenWeather API settings.
constexpr char openWeatherApiKey[] = "fb7fe045a1a44f88c65e8e23f022e1dc";
constexpr float latitude = 46.786169F;
constexpr float longitude = 7.604566F;

// Timezone offsets (seconds) used by NTP synchronization.
constexpr long gmtOffsetSec = 3600;
constexpr int daylightOffsetSec = 3600;

}  // namespace app_config
