#include "wifi_component.h"


static const char* TAG="wifi_component";

static uint16_t list_size=WIFI_SCAN_LIST_NUM;
static uint16_t wifi_ap_count=0;

void wifi_init(void){
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

wifi_ap_record_t wifi_ap_info[WIFI_SCAN_LIST_NUM];

void wifi_scan(void){
    wifi_init();   
    esp_wifi_scan_start(NULL,true);

    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&wifi_ap_count));
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&list_size,wifi_ap_info));
    
}
void wifi_connect(wifi_config_t wifi_config){
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
    ESP_LOGI(TAG,"Wifi Start, is connecting...");


}