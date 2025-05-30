# alarm_panel

Alarm Panel is a fire alarm system control panel and fire alarm driver for a ESP32 based M5 Stack Core2 and a slave Arduino AtMega 2560 or ESP32 device. It was made to simulate a commercial fire alarm system, for a friend's kid.

it demonstrates

1. HTTPD use
2. A quick and dirty technique to feed wifi credentials to the device via SD or SPIFFS
3. Managing a WiFi connection
4. Generating and using dynamic web content with ClASP-Tree 
5. Driving a device over serial
6. Using htcw_gfx and htcw_uix with the ESP LCD Panel API to present a user interface.
7. Using React content to drive a website from the ESP32 with automatic rebuild
8. Using a websocket to communicate between the browser and an ESP32
9. Hosting the firmware within a Windows scaffolding application for faster testing and development

## Setup

Prerequisites:

NOTE: Currently this build environment requires Windows.

You need Node.js - specifically Node Version Manager (nvm) and Node Package Manager (npm) installed under the PlatformIO CLI command prompt

Installation and build steps:

1. (First time only) - run install.cmd under the PlatformIO CLI command prompt

2. Set the appropriate COM ports in platformio.ini

3. Add wifi.txt to an SD or spiffs to connect to the network. First line is the SSID, second is the network password

4. Configure include/config.h for the count of alarms and all the associated pins

5. (Optional) Upload filesystem image (you must do this if you're using wifi.txt on SPIFFS)

6. Upload Firmware to control (supported devices: M5Stack Core2, and Freenove ESP32-S3 Development Kit w/ integrated display)

7. Upload Firmware to slave (supported devices: Arduino AtMega2560, or ESP32)

Note that you don't actually need the slave connected to test the UI and web interface.

## Web interface
The QR Link provides a QR code to get to the website. The JSON/REST api is located at ./api at the same location

a JSON example:

`http://192.168.50.14/api`

Where the IP is replaced with the local network IP of the ESP32

```json
{
    "status": [
        true, false, true, true
    ]
}
```
Each boolean value in the status array is the state of a given alarm at the index.

The query string works the same for the web page as it does for the API:

Query string parameters:

- `a` = zero based alarm index) ex: `a=1` (only honored when `set` is specified)
- `set` = the presence of this parameter indicates that all `a` values be set, and any not present be cleared.

Example: `http://192.168.50.14?a=0&a=2&set`

Where the IP is replaced with the local network IP of the Core2

This will set all the fire alarms to off except #1 (zero based index of 0) and #3 (index 2)

The HTTP responses in the ESP-IDF code were generated using [Vite](https://vite.dev/) and [ClASP](https://github.com/codewitch-honey-crisis/clasp)

## Local Web Server and GUI

For quicker testing, there is a win32 C++ project under `./host_win32` which will display the generated web content locally, as well as run the main application logic and user interface.

## Project Structure

The implementation for the control end of the source code is under `./src-control`, but this simply supports the application logic which resides under `./common/src/control.cpp`. The Arduino code for the slaves under `./src-slave`.

- `./boards` contains PIO support for the freenove devkit
- `./build_tools` contains ClASP-Tree which is necessary to embed the generated web content into the firmware source code `./common/include/httpd_content.h`
- `./common` contains the code that is common to both the ESP-IDF and Win32, including the main application logic in `./common/src/control.cpp`
- `./host_win32` is its own project, and hosts the common code under Windows for easier testing and development
- `./react-web` is where all the web content goes:
   - `./react-web/src` contains React content accessible from the website
   - `./react-web/public` contains static content accessible from the website, and dynamic ClASP SSR content (accessible from the website unless preceded with `.` in the filename)
   - `./react-web/dist` contains the end generated React content (produced by Vite) that will end up being embedded with ClASP-Tree.
   

## Implementation errata

### Websocket and website interaction

The web page communicates to the control firmware using a websocket exposed at `http://<address>/socket`. Each time an alarm is set or cleared, the alarm values are packed into 5 bytes:

The first byte is the byte count. The remaining 4 bytes are represented as a big endian `uint32_t` where each bit is one alarm's state. It is then sent to the websocket.

The website may also send a 1 byte message (the payload is ignored) to get the webserver to send back a 5 byte message (packed as above) containing the alarm status bits.

Furthermore, any alarm changes from control get broadcast to all connected websockets.

### Win32 Host

The Win32 host uses DirectX/Direct2D to present a draw surface suitable for a library like htcw_uix (or LVGL) to draw to the display. It is intended to be compiled with Microsoft's C++ compiler so you should have Visual Studio 2022 installed. The app also exposes an http server and websocket endpoint at `localhost:8080` for testing the code. In order to use this host, your code must not use platform specific or compiler specific constructs. If you wish to expose platform specific functionality, do so by making a common C ABI for it, putting the header in `./common/include` and then implement it separately for each of the two projects (ESP32 control vs Win32 host)

### Slave logic

The slave code is simple, and handles the basic fire alarm operation. When it receives a signal indicating a thrown switch it communicates that to control. Control then sends a throw command back to the slave in order to trip the alarm. Unthrowing the switch will NOT turn the alarm off. First the switch must be unthrown, and then the alarm must be turned off at the control. All this logic is effectively handled by the slave.

