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

## Setup

1. Set the appropriate COM ports in platformio.ini 

2. (Optional - necessary for web interface) Add wifi.txt to an SD or spiffs to connect to the network. First line is the SSID, second is the network password

3. Configure include/config.h for the count of alarms and all the associated pins

4. (Linux only) set chmod +x to build_react.sh

5. Upload Firmware to control

6. Upload Firmware to slave


## Web interface
The QR Link provides a QR code to get to the website. The JSON/REST api is located at ./api at the same location

a JSON example:

`http://192.168.50.14/api`

Where the IP is replaced with the local network IP of the Core2

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

The HTTP responses in the ESP-IDF code were generated using Vite and [ClASP](https://github.com/codewitch-honey-crisis/clasp)
