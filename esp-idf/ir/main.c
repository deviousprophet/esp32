#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#define GPIO_INPUT_IR1          GPIO_NUM_12
#define GPIO_INPUT_IR2          GPIO_NUM_14
#define GPIO_INPUT_PIN_SEL      ((1ULL<<GPIO_INPUT_IR1) | (1ULL<<GPIO_INPUT_IR2))

#define WIFI_SSID   "Hotspot-LATITUDE-E6420"
#define WIFI_PASS   "31121998"
#define MQTT_SERVER "mqtt://192.168.137.1"
#define SUB_TOPIC   "hotel/101/admin"
#define PUB_TOPIC   "hotel/101/ir"

bool mqtt_en = false;

static const char *TAG = "MQTT_IR";

esp_mqtt_client_handle_t client;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, SUB_TOPIC, 0);
            mqtt_en = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            if (mqtt_en) {
                mqtt_en = false;
            }
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            esp_mqtt_client_reconnect(client);
            ESP_LOGI(TAG, "Reconnecting to MQTT broker...");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_SERVER,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to Wifi...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retrying to connect to Wifi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        mqtt_app_start();
    }
}

void wifi_init_sta() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

void read_ir() {
    int in_out_stat = 0;
    
    while(1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (mqtt_en) {
            int ir1 = gpio_get_level(GPIO_INPUT_IR1);
            int ir2 = gpio_get_level(GPIO_INPUT_IR2);

            if (ir1 && ir2) {
                if (in_out_stat == 3) {
                    printf("in\n");
                    esp_mqtt_client_publish(client, PUB_TOPIC, "in", 0, 0, 0);
                } else if (in_out_stat == -3) {
                    printf("out\n");
                    esp_mqtt_client_publish(client, PUB_TOPIC, "out", 0, 0, 0);
                }
                in_out_stat = 0;
            } else if (!ir1 && ir2) {
                if (in_out_stat == -2) {
                    in_out_stat = -3;
                } else if ((in_out_stat == 0) || (in_out_stat == 2)) {
                    in_out_stat = 1;
                }
            } else if (!ir1 && !ir2) {
                if ((in_out_stat == 1) || (in_out_stat == 3)) {
                    in_out_stat = 2;
                } else if ((in_out_stat == -1) || (in_out_stat == -3)) {
                    in_out_stat = -2;
                }
            } else {
                if (in_out_stat == 2) {
                    in_out_stat = 3;
                } else if ((in_out_stat == 0) || (in_out_stat == -2)) {
                    in_out_stat = -1;
                }
            }
        }
    }
}

void gpioConf() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

void app_main() {
    nvs_flash_erase();
    nvs_flash_init();
    tcpip_adapter_init();
    esp_event_loop_create_default();
    wifi_init_sta();
    gpioConf();
    read_ir();
}