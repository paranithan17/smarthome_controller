Hardware: - ESP32 WROOM-32D - ILI9844 Touch Display 480x320 - Arduino Humidity Sensor - RGD-LED - Humidity Sensor - Lamp, connected with Shelly

The touchscreen has three different UI-Sides.

Side1 has: - One Toggle Button (ON/OFF) to controll the Lamp with Shelly over http - Below Toggle Button, three Slider with the range from 10 - 100% - Right next to the sliders there are the status indicator, which shows the current value of the chosen slider. - Right next to the status indicator is a switch

The three sliders controlls the RGB LED with PWM on the GPIOS. Only if the switch next to the slider is actviated, then the slider can send the value to the RGB LED. Otherwise it will not adapt the output value.

Side2 has: - In upper Center the room temperature - Below that the Humidity.
The humidity sensor should send actual values every 15 seconds.
If the sensor senses a temp below 15°C or above 30°C the entire system is blocked and on the display is Warning. That the Temp is to low/high and that the System is blocked.

Side3 has: - The time - Date with day - Temperature at current location
The date time an the weather is synchronized over a API from Openweather

All three sides have a footer with WLAN connection status. The UI now shows the real WLAN name the ESP32 is connecting to (`SSID` was only a placeholder): - "Searching for <WLAN_NAME>" - "<WLAN_NAME> found!" - "Connecting to <WLAN_NAME>" - "<WLAN_NAME>: Connected"

WLAN configuration:

- Set your WLAN name and password in `Software/smarthome_controller/include/wifi_credentials.h`.
- Replace `YOUR_WLAN_NAME` and `YOUR_WLAN_PASSWORD` with your real credentials.

Side 3 time and weather configuration:

- Set OpenWeather API key and coordinates in `Software/smarthome_controller/include/openweather_config.h`.
- Replace `YOUR_OPENWEATHER_API_KEY` with your key.
- Set `kLatitude` and `kLongitude` for your location.
- Set `kGmtOffsetSec` and `kDaylightOffsetSec` for local time.
- Current time (with seconds) is updated every second over WiFi (NTP).
- OpenWeather current weather and current temperature are refreshed every 60 minutes.

If the display has not detected interaction on the touchscreen for a certain time like 10sec it should change the side every 8 seconds in an loop. If the touchscreen sense an interaction, the program heads directly to the side 1.

This program uses FreeRTOS.
Higher Priorities are: Sensing the humidity, Controll the Lamp and LED.
Medium Priority: Synchronise time and Weather from Openweather
Low Priority: Sensing Display interactions and everything else.

Side 1 Pin Connections (Current) - RGB LED Blue -> D13 (GPIO13, PWM output) - RGB LED Green -> D12 (GPIO12, PWM output) - RGB LED Red -> D14 (GPIO14, PWM output)

    - Joystick VRX   -> D34 (GPIO34, analog left/right input, ADC1 for WiFi compatibility)
    - Joystick SW    -> D26 (GPIO26, channel select button)

    - Lamp Button    -> D25 (GPIO25, toggles lamp ON/OFF state in UI)

Note: - Lamp ON/OFF is currently toggled in UI only. - HTTP control to Shelly will be added in a future step.
