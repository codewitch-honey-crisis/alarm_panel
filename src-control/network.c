#include "network.h"
#include <stdio.h>
#include <memory.h>
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "fs.h"
static esp_ip4_addr_t wifi_ip;
static size_t wifi_retry_count = 0;
static const EventBits_t wifi_connected_bit = BIT0;
static const EventBits_t wifi_fail_bit = BIT1;
static EventGroupHandle_t wifi_event_group = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_count < 3) {
            esp_wifi_connect();
            ++wifi_retry_count;
        } else {
            xEventGroupSetBits(wifi_event_group, wifi_fail_bit);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_retry_count = 0;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        memcpy(&wifi_ip, &event->ip_info.ip, sizeof(wifi_ip));
        xEventGroupSetBits(wifi_event_group, wifi_connected_bit);
    }
}
int net_wifi_load(const char* path, char* ssid, char* pass) {
    FILE* file = fopen(path, "r");
    if (file != NULL) {
        // parse the file
        fgets(ssid, 64, file);
        char* sv = strchr(ssid, '\n');
        if (sv != NULL) *sv = '\0';
        sv = strchr(ssid, '\r');
        if (sv != NULL) *sv = '\0';
        fgets(pass, 128, file);
        fclose(file);
        sv = strchr(pass, '\n');
        if (sv != NULL) *sv = '\0';
        sv = strchr(pass, '\r');
        if (sv != NULL) *sv = '\0';
        return 0;
    }
    return -1;
}

int net_init() {
    fs_internal_init();
    char wifi_ssid[65]={0};
    char wifi_pass[129]={0};    
    bool loaded = false;
    if (0==fs_external_init()) {
        puts("SD card found, looking for wifi.txt creds");
        loaded = 0==net_wifi_load("/sdcard/wifi.txt", wifi_ssid, wifi_pass);
    }
    if (!loaded) {
        puts("Looking for wifi.txt creds on internal flash");
        loaded = 0==net_wifi_load("/spiffs/wifi.txt", wifi_ssid, wifi_pass);
    }
    if (loaded) {
        printf("Initializing WiFi connection to %s\n", wifi_ssid);
    } else {
        puts("Network credentials not found");
        return -1;
    }
    if(wifi_event_group!=NULL) {
        return 0;
    }
    if(ESP_OK!=nvs_flash_init()) {
        return -1;
    }
    wifi_event_group = xEventGroupCreate();
    if(wifi_event_group==NULL) {
        return -1;
    }
    if(ESP_OK!=esp_netif_init()) {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group=NULL;
        return -1;
    }

    if(ESP_OK!=esp_event_loop_create_default()) {
        esp_netif_deinit();
        vEventGroupDelete(wifi_event_group);
        wifi_event_group=NULL;
        return -1;
    }
    if(NULL==esp_netif_create_default_wifi_sta()) {
        esp_netif_deinit();
        vEventGroupDelete(wifi_event_group);
        wifi_event_group=NULL;
        return -1;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if(ESP_OK!=esp_wifi_init(&cfg)) {
        esp_netif_deinit();
        vEventGroupDelete(wifi_event_group);
        wifi_event_group=NULL;
        return -1;
    }
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    if(ESP_OK!=esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
        &instance_any_id)) {
        goto error;
    }
    if(ESP_OK!=esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
        &instance_got_ip)) {
        goto error;
        
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, strlen(wifi_ssid) + 1);
    memcpy(wifi_config.sta.password, wifi_pass, strlen(wifi_pass) + 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    // wifi_config.sta.sae_h2e_identifier[0]=0;
    if(ESP_OK!=esp_wifi_set_mode(WIFI_MODE_STA)) {
        goto error;
    }
    if(ESP_OK!=esp_wifi_set_config(WIFI_IF_STA, &wifi_config)) {
        goto error;
    }
    if(ESP_OK!=esp_wifi_start()) {
        goto error;
    }
    return 0;
error:
    esp_wifi_deinit();
    esp_netif_deinit();
    vEventGroupDelete(wifi_event_group);
    wifi_event_group=NULL;
    return -1;
}

net_status_t net_status() {
    if (wifi_event_group == NULL) {
        return NET_WAITING  ;
    }
    EventBits_t bits = xEventGroupGetBits(wifi_event_group) &
                       (wifi_connected_bit | wifi_fail_bit);
    if (bits == wifi_connected_bit) {
        return NET_CONNECTED;
    } else if (bits == wifi_fail_bit) {
        return NET_CONNECT_FAILED;
    }
    return NET_WAITING;
}
void net_end() {
    if(wifi_event_group==NULL) {
        return;
    }
    esp_wifi_deinit();
    esp_netif_deinit();
    vEventGroupDelete(wifi_event_group);
    wifi_event_group=NULL;
}
int net_address(char* out_address,size_t out_address_length) {
    if(net_status()!=NET_CONNECTED) {
        return -1;
    }
     snprintf(out_address, out_address_length, IPSTR,
                     IP2STR(&wifi_ip));
    return 0;
}