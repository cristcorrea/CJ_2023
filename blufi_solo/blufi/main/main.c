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
#include "blufi.h"
#include "esp_blufi.h"

#include "mqtt.h"
#include "dht.h"
#include "soil.h"
#include "rtc.h"

SemaphoreHandle_t semaphoreWifiConection = NULL;
SemaphoreHandle_t semaphoreMqttConection = NULL;


dht DHT_DATA;
soil SOIL_DATA;

void mqttServerConection(void *params)
{

    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY))
        {
            ESP_LOGI("Main Task", "Realiza conexión con broker HiveMQ");
            mqtt_start();
        }
    }
}

void mqttSendMessage(void *params)
{

    char message[50];
    if (xSemaphoreTake(semaphoreMqttConection, portMAX_DELAY))
    {
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(60000)); // envia cada 1 min
            sprintf(message, "Temp: %.1f °C Hum: %i%% Soil: %i Salt: %i", 
            DHT_DATA.temperature, DHT_DATA.humidity, SOIL_DATA.humidity, SOIL_DATA.salinity);
            enviar_mensaje_mqtt("sensores/temperatura", message);
        }
    }
}

void airSensorMeasurement(void *params)
{

    while(true)
    {
        DHTerrorHandler(readDHT());
        vTaskDelay(pdMS_TO_TICKS(30000)); // mide cada 30 seg.
    }

}

void soilSensorMeasurement(void *params)
{
    soilConfig();
    while(true)
    {
        humidity(); 
        vTaskDelay(pdMS_TO_TICKS(30000));
    }

}


void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreMqttConection = xSemaphoreCreateBinary();

    blufi_start();

    xTaskCreate(&mqttServerConection,
                "Conectando con HiveMQ Broker",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(&mqttSendMessage,
                "Comienza envio de datos MQTT",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(&airSensorMeasurement,
                "Comenzando mediciones de sensores",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(&soilSensorMeasurement,
                "Comenzando mediciones de sensores",
                4096,
                NULL,
                1,
                NULL);

    //dataUpdate();


}
