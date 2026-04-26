# Smart Home Controller

An ESP32-based smart home controller for a 480x320 touch display, built with Arduino, FreeRTOS, LVGL, and PlatformIO. The project was created as part of the RTOS course work at Bern University of Applied Sciences and combines local hardware control with WLAN-backed home-automation features.

The current codebase implements two active UI pages:

1. Lamp and RGB control
2. Time and weather

The original project report also describes a humidity/safety page. That page is not present in this code snapshot yet, so the README documents the implemented behavior and clearly marks the report-only requirements separately.

## Hardware

- ESP32 WROOM-32D
- ILI9844 touch display, 480x320
- RGB LED with PWM control
- Lamp controlled via a Shelly relay over HTTP
- Joystick and push buttons for local input

## Features

### Page 1: Lamp and RGB control

- Lamp toggle button for Shelly relay control via HTTP
- Three RGB sliders for the red, green, and blue channels
- Per-channel enable switches so a slider only affects the LED when its switch is active
- Live numeric value display next to each slider
- Footer WLAN status and activity arc

### Page 3: Time and weather

- Current time shown with seconds
- Date shown with day, month, and year
- Current weather condition and temperature from OpenWeather
- WLAN status in the footer
- Automatic NTP-based time synchronization

### RTOS behavior

- High-priority tasks handle lamp and LED control
- Medium-priority tasks handle time and weather refresh
- Lower-priority tasks collect runtime statistics and support UI updates
- Shared RTOS primitives are used for queues, event groups, mutexes, and task notifications

## Current runtime behavior

- The app starts on page 1.
- If the display detects a user interaction, it returns to page 1.
- After 10 seconds without interaction, automatic page cycling is enabled.
- While auto-cycling is active, the UI switches between the implemented pages every 5 seconds.
- Time is updated once per second.
- NTP resynchronization is attempted every 180 minutes.
- Weather fetches are triggered by a periodic timer every 30 minutes.

## Repository layout

- [Software/smarthome_controller/src](Software/smarthome_controller/src) - application source code
- [Software/smarthome_controller/include](Software/smarthome_controller/include) - shared headers and configuration
- [Software/smarthome_controller/platformio.ini](Software/smarthome_controller/platformio.ini) - PlatformIO project configuration
- [Softwaredesign](Softwaredesign) - design diagrams and mockups

## Pin mapping

### RGB LED and input controls

- Blue PWM - GPIO13
- Green PWM - GPIO12
- Red PWM - GPIO14
- Joystick VRX - GPIO34
- Joystick switch - GPIO26
- Lamp button - GPIO25

### Notes on the input layer

- The joystick axis is read from ADC1 so WiFi can remain active.
- The joystick switch cycles the active RGB channel.
- The lamp button toggles the cached lamp state and can trigger the Shelly HTTP command.

## Configuration

The current implementation keeps WLAN, OpenWeather, and timezone values in [Software/smarthome_controller/include/app_config.h](Software/smarthome_controller/include/app_config.h).

Update these values before building for your own setup:

- WLAN SSID and password
- OpenWeather API key
- Latitude and longitude for the target location
- GMT offset and daylight-saving offset

Important: the repository currently contains example credentials in `app_config.h`. Replace them before flashing the firmware.

If you want to use a different Shelly device, change the relay URL in [Software/smarthome_controller/src/app_controls.cpp](Software/smarthome_controller/src/app_controls.cpp).

## Build and flash

This project uses PlatformIO with the `esp32dev` environment.

1. Open [Software/smarthome_controller](Software/smarthome_controller) in VS Code with the PlatformIO extension installed.
2. Make sure the serial port in [Software/smarthome_controller/platformio.ini](Software/smarthome_controller/platformio.ini) matches your machine.
3. Build the firmware.
4. Upload it to the ESP32.
5. Open the serial monitor at 115200 baud if you want runtime statistics and debug output.

If PlatformIO is not on your PATH on Windows, use the PlatformIO extension commands or the installed `platformio.exe` from your user profile.

## How the UI works

### Page 1

The first page is the main control page. It contains the lamp toggle and the RGB controls.

- The lamp button sends an HTTP request to the Shelly relay.
- The RGB sliders change the PWM output on GPIO13, GPIO12, and GPIO14.
- A slider only changes the LED when its matching switch is enabled.
- The UI keeps the displayed values synchronized with the internal FreeRTOS state.

### Page 3

The third page shows time and weather information.

- Time is sourced from NTP and incremented locally every second.
- Weather data is fetched from OpenWeather and cached for the display.
- When no valid weather data is available, the screen shows a disconnected state.

## FreeRTOS task overview

- `lamp_control` handles lamp button events and sends Shelly HTTP commands.
- `led_control` handles joystick input and updates the RGB output.
- `display_ui` owns all LVGL display updates and page switching.
- `time_task` syncs time over NTP and publishes clock updates.
- `weather_task` fetches OpenWeather data on timer notifications.
- `runtime_stats_task` prints stack and system statistics over Serial.

## Report-only requirements

The project report describes additional goals that are useful to keep in mind:

- A third functional page for room temperature and humidity
- A humidity sensor update interval of 15 seconds
- A safety lockout when temperature drops below 15°C or rises above 30°C
- Full UI status wording for WLAN connection stages

Those items are not all implemented in the current source tree, so treat them as project requirements or future work rather than finished behavior.

## Troubleshooting

- If the lamp does not react, verify that the Shelly IP address in `app_controls.cpp` matches your device.
- If time stays at `--:--:--`, check WLAN connectivity and the NTP settings in `app_config.h`.
- If weather stays disconnected, confirm the OpenWeather API key and coordinates.
- If the display never auto-switches pages, check whether the screen is receiving touch events that reset the idle timer.

## License and attribution

This project was authored by Paranithan Paramalingam as part of the BFH-TI RTOS coursework. Add a license here if you want to publish the repository publicly.
