#include "wifi_component.h"
#include "semaphore.h"

static SemaphoreHandle_t wifi_scan_sem;
static const char *TAG = "wifi_component";

static uint16_t list_size = WIFI_SCAN_LIST_NUM;
static uint16_t wifi_ap_count = 0;

void wifi_init(void) {
    wifi_scan_sem = xSemaphoreCreateBinary();
    if (wifi_scan_sem == NULL) {
        ESP_LOGE(TAG, "wifi_scan semaphore create fail");
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

wifi_ap_record_t wifi_ap_info[WIFI_SCAN_LIST_NUM];

void wifi_scan_task(void *param) {

}
void wifi_scan() {
    wifi_init();

    esp_wifi_scan_start(NULL, true);


    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&wifi_ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&list_size, wifi_ap_info));
    // xTaskCreate(wifi_scan_task,"wifi_scan_task",4096,NULL,5,NULL);
}
void wifi_connect(wifi_config_t wifi_config) {
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Wifi Start, is connecting...");


}