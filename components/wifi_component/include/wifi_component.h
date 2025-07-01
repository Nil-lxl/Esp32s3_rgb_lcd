#pragma once

#include <stdio.h>
#include "esp_wifi.h"
#include "esp_log.h"


#define WIFI_SCAN_LIST_NUM 10
#define WIFI_SSID       "MI14"
#define WIFI_PASSWORD   "456123123"
#define WIFI_RECONNECT_MAX  5

extern wifi_ap_record_t wifi_ap_info[WIFI_SCAN_LIST_NUM];
extern wifi_config_t wifi_config;
/**
 * Initialize the underlying TCP/IP stack and create default event loop.
 */
void wifi_init(void);

void wifi_scan();

void wifi_connect(wifi_config_t wifi_config);

