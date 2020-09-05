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

#define GPIO_RELAY      GPIO_NUM_12
#define GPIO_SEL_PIN    1ULL<<GPIO_RELAY

#define WIFI_SSID       "Hotspot-LATITUDE-E6420"
#define WIFI_PASS       "31121998"
#define MQTT_SERVER     "mqtt://192.168.137.1"

#define ROOM_NUM        "101"
#define SUB_TOPIC       "hotel/" ROOM_NUM "/#"
#define PUB_TOPIC       "hotel/" ROOM_NUM "/relay"

bool mqtt_en = false;
int human_cnt = 0;

static const char *TAG = "MQTT_RELAY";

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
            char _topic[1000], _data[1000];
            memcpy(_topic, event->topic, event->topic_len);
            _topic[event->topic_len] = '\0';
            memcpy(_data, event->data, event->data_len);
            _data[event->data_len] = '\0';
            // _topic: Topic string
            // _data: Data string
            if (strcmp(_topic, "hotel/" ROOM_NUM "/admin") == 0) {
                if (strcmp(_data, "RELAY_STAT") == 0) {
                    esp_mqtt_client_publish(client, PUB_TOPIC, (gpio_get_level(GPIO_RELAY) == 0)?"1":"0", 0, 0, 0);
                } else if (strcmp(_data, "RELAY_ON") == 0) {
                    gpio_set_level(GPIO_RELAY, 0);
                    esp_mqtt_client_publish(client, PUB_TOPIC, (gpio_get_level(GPIO_RELAY) == 0)?"1":"0", 0, 0, 0);
                } else if (strcmp(_data, "RELAY_OFF") == 0) {
                    gpio_set_level(GPIO_RELAY, 1);
                    esp_mqtt_client_publish(client, PUB_TOPIC, (gpio_get_level(GPIO_RELAY) == 0)?"1":"0", 0, 0, 0);
                } else if (strcmp(_data, "RELAY_CHECK") == 0) {
                    esp_mqtt_client_publish(client, PUB_TOPIC, ROOM_NUM "_relay_ready", 0, 0, 0);
                } else if (strcmp(_data, "ADMIN_RESET") == 0) {
                    human_cnt = 0;
                    printf("Count: %d\n" ,human_cnt);
                }
            } else if (strcmp(_topic, "hotel/" ROOM_NUM "/ir") == 0) {
                if (strcmp(_data, "in") == 0) {
                    human_cnt++;
                } else if (strcmp(_data, "out") == 0) {
                    if (human_cnt > 0){
                        human_cnt--;
                    }
                }
                (human_cnt > 0) ? gpio_set_level(GPIO_RELAY, 0) : gpio_set_level(GPIO_RELAY, 1);;
                printf("Count: %d\n" ,human_cnt);
            }
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

void gpioConf() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = GPIO_SEL_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

void app_main() {
    nvs_flash_erase();
    nvs_flash_init();
    tcpip_adapter_init();
    esp_event_loop_create_default();
    wifi_init_sta();
    gpioConf();
    gpio_set_level(GPIO_RELAY, 1);
}