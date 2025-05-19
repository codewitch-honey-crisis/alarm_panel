#pragma once
#include "esp_wifi.h"
enum WIFI_STATUS { WIFI_WAITING, WIFI_CONNECTED, WIFI_CONNECT_FAILED };
extern char wifi_ssid[];
extern char wifi_pass[];
extern esp_ip4_addr_t wifi_ip;
void wifi_init(const char* ssid, const char* password);
WIFI_STATUS wifi_status();
bool wifi_load(const char* path, char* ssid, char* pass);