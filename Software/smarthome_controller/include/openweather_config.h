#pragma once

namespace openweather_config {

// Replace with your OpenWeather API key.
constexpr char kApiKey[] = "fb7fe045a1a44f88c65e8e23f022e1dc";

// Location coordinates for weather lookup.
constexpr float kLatitude = 46.786169F;
constexpr float kLongitude = 7.604566F;

// Timezone offsets (seconds) used by NTP time sync.
constexpr long kGmtOffsetSec = 3600;
constexpr int kDaylightOffsetSec = 3600;

}  // namespace openweather_config