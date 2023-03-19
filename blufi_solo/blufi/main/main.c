#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_blufi_api.h"
#include "blufi_example.h"

#include "esp_blufi.h"

#include "mqtt.h"
#include "dht.h"

SemaphoreHandle_t semaphoreWifiConection = NULL;
SemaphoreHandle_t semaphoreMqttConection = NULL; 

dht DHT_DATA;

void mqttServerConection(void* params){

    while(true)
    {
        if(xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)){
            ESP_LOGI("Main Task", "Realiza conexión con broker HiveMQ");
            mqtt_start(); 
        }
    }
}

void sensorStartMeasurement(void* params){

    char message[50]; 
    if(xSemaphoreTake(semaphoreMqttConection, portMAX_DELAY)){
        while(true)
        {
            DHTerrorHandler(readDHT());
            sprintf(message, "Temp: %.1f °C Hum: %i%%", DHT_DATA.temperature, DHT_DATA.humidity);
            enviar_mensaje_mqtt("sensores/temperatura", message);
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
        
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary(); 
    semaphoreMqttConection = xSemaphoreCreateBinary(); 

    xTaskCreate(&mqttServerConection,
                "Conectando con HiveMQ Broker",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(&sensorStartMeasurement,
                "Comenzando mediciones de Temp. & Hum.",
                4096,
                NULL,
                1,
                NULL);

    blufi_start();

}
