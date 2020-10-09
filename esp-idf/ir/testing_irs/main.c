#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#define GPIO_INPUT_IR1          GPIO_NUM_12
#define GPIO_INPUT_IR2          GPIO_NUM_14
#define GPIO_INPUT_PIN_SEL      ((1ULL<<GPIO_INPUT_IR1) | (1ULL<<GPIO_INPUT_IR2))
#define ESP_INTR_FLAG_DEFAULT   0

#define WIFI_SSID       "Hotspot-LATITUDE-E6420"
#define WIFI_PASS       "31121998"
#define MQTT_SERVER     "mqtt://192.168.137.1"

#define ROOM_NUM        "101"
#define SUB_TOPIC       "hotel/" ROOM_NUM "/admin"
#define PUB_TOPIC       "hotel/" ROOM_NUM "/ir"

bool mqtt_en = false;
int in_out_state = 0, ir1 = 0, ir2 = 0;
static const char *TAG = "MQTT_IR";
static xQueueHandle gpio_evt_queue = NULL;
esp_mqtt_client_handle_t client;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_irs(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY) && mqtt_en) {
            // Handling sensors state
            ir1 = gpio_get_level(GPIO_INPUT_IR1);
            ir2 = gpio_get_level(GPIO_INPUT_IR2);

            if (ir1 && ir2) {
                if (in_out_state == 3) {
                    printf("in\n");
                    esp_mqtt_client_publish(client, PUB_TOPIC, "in", 0, 0, 0);
                }
                else if (in_out_state == -3) {
                    printf("out\n");
                    esp_mqtt_client_publish(client, PUB_TOPIC, "out", 0, 0, 0);
                }
                if (in_out_state != 0) {
                    in_out_state = 0;
                }
            }
            else if (!ir1 && ir2) {
                if (in_out_state == -2) {
                    in_out_state = -3;
                }
                else if ((in_out_state == 0) || (in_out_state == 2)) {
                    in_out_state = 1;
                }
            }
            else if (!ir1 && !ir2) {
                if ((in_out_state == 1) || (in_out_state == 3)) {
                    in_out_state = 2;
                }
                else if ((in_out_state == -1) || (in_out_state == -3)) {
                    in_out_state = -2;
                }
            }
            else {
                if (in_out_state == 2) {
                    in_out_state = 3;
                }
                else if ((in_out_state == 0) || (in_out_state == -2)) {
                    in_out_state = -1;
                }
            }
        }
    }
}

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
            if (strcmp(_data, "IR_CHECK") == 0) {
                esp_mqtt_client_publish(client, PUB_TOPIC, ROOM_NUM "_ir_ready", 0, 0, 0);
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
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    xTaskCreate(gpio_task_irs, "gpio_task_irs", 2048, NULL, 10, NULL);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IR1, gpio_isr_handler, (void*) GPIO_INPUT_IR1);
    gpio_isr_handler_add(GPIO_INPUT_IR2, gpio_isr_handler, (void*) GPIO_INPUT_IR2);
}

void app_main() {
    nvs_flash_erase();
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_sta();
    gpioConf();
}