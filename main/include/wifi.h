#ifndef WIFI_H
#define WIFI_H

#include "esp_event.h"  // Adicione esta linha para definir esp_event_base_t

// #define WIFI_SSID "Lappis Wifi"  
// #define WIFI_PASS "<%=lappiscontainer=%>"

#define WIFI_SSID "KALANGOS"
#define WIFI_PASS "unb1fga1"

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void wifi_init(void);

#endif