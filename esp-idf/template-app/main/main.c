#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

void app_main(void)
{
    printf("Hello world!\n");
    for (int i = 10; i >= 0; i--) {
        printf("Stop in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Stopped.\n");
}