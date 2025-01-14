#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Địa chỉ MAC của ESP32 Receiver
uint8_t receiver_mac[] = {0xFC, 0xE8, 0xC0, 0x7C, 0xE3, 0xE0};

// Cấu trúc dữ liệu gửi
typedef struct {
    int node_id;
    float temperature;
    float humidity;
} sensor_data_t;

// Hàm tạo dữ liệu
void generate_data(sensor_data_t *data, int node_id) {
    data->node_id = node_id;
    data->temperature = 20.0 + node_id * 0.5;  // Giả lập nhiệt độ
    data->humidity = 60.0 + node_id * 1.0;     // Giả lập độ ẩm
}

// Callback khi gửi dữ liệu
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    printf("Gửi tới %02X:%02X:%02X:%02X:%02X:%02X: %s\n",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5],
           status == ESP_NOW_SEND_SUCCESS ? "Thành công" : "Thất bại");
}

// Chuyển sang chế độ Deep Sleep
void enter_deep_sleep() {
    printf("Chuyển sang chế độ Deep Sleep. Sẽ đánh thức sau 3 phút.\n");
    esp_sleep_enable_timer_wakeup(180000000); // 3 phút (3 * 60 * 1000000 us)
    esp_deep_sleep_start();
}

// Task gửi dữ liệu
void sender_task(void *param) {
    sensor_data_t data;
    while (1) {
        for (int i = 1; i <= 9; i++) {
            generate_data(&data, i);  // Tạo dữ liệu cho từng node
            esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
            printf("Gửi Node %d: Nhiệt độ=%.2f°C, Độ ẩm=%.2f%%\n",
                   data.node_id, data.temperature, data.humidity);
            vTaskDelay(pdMS_TO_TICKS(200)); // Giãn cách 200ms giữa các gói
        }
        printf("Đã gửi xong dữ liệu từ tất cả các node.\n");
        enter_deep_sleep(); // Chuyển sang chế độ Deep Sleep
    }
}

// Khởi tạo NVS
void initialize_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("Khởi tạo NVS thành công.\n");
}

// Khởi tạo WiFi
void initialize_wifi() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Chuyển sang chế độ Station
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("Khởi tạo WiFi ở chế độ Station thành công.\n");
}

// Hàm chính
void app_main() {
    initialize_nvs();        // Khởi tạo NVS
    initialize_wifi();       // Khởi tạo WiFi
    esp_now_init();          // Khởi tạo ESP-NOW
    esp_now_register_send_cb(on_data_sent); // Đăng ký callback khi gửi dữ liệu

    // Cấu hình thông tin peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    esp_now_add_peer(&peer_info);

    sender_task(NULL); // Gọi task gửi dữ liệu trực tiếp
}