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
#include "esp_blufi.h"
#include "time.h"
#include "driver/gpio.h"
/* Librerias componentes */
#include "ntp.h"
#include "mqtt.h"
#include "dht.h"
#include "soil.h"
#include "blufi.h"
#include "storage.h"

#define POWER_CTRL 4


SemaphoreHandle_t semaphoreWifiConection = NULL;
SemaphoreHandle_t semaphoreMqttConection = NULL;
SemaphoreHandle_t semaphoreRTC = NULL;


dht DHT_DATA;
soil SOIL_DATA;
/*Estructura para manipular configuración*/
typedef struct
{
    char *UUDI;         // debe almacenar el identificador recibido en custom message
    uint8_t hum_sup;    // limite superior de humedad
    uint8_t hum_inf;    // Limite inferior de humedad     


}config_data;

config_data configuration; 

void mqttServerConection(void *params)
{

    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)) // establecida la conexión WiFi
        {
            ESP_LOGI("Main Task", "Realiza conexión con broker HiveMQ");
            adjust_time();
            mqtt_start();
            xSemaphoreGive(semaphoreRTC);
        }
    }
}

void mqttSendMessage(void *params)
{

    char message[120];
    if (xSemaphoreTake(semaphoreMqttConection, portMAX_DELAY)) // establecida la conexión con el broker
    {
        while (true)
        {   
            vTaskDelay(pdMS_TO_TICKS(60000)); // espera 1 minuto y envía
            time_t now = 0;
            struct tm timeinfo = {0};
            time(&now);
            localtime_r(&now, &timeinfo);
            char strftime_buf[64]; 
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            sprintf(message, "Temp: %.1f °C Hum: %i%% Soil: %i Salt: %i Time: %s", 
            DHT_DATA.temperature, DHT_DATA.humidity, SOIL_DATA.humidity,
             SOIL_DATA.salinity, strftime_buf);
            enviar_mensaje_mqtt("sensores/temperatura", message);
        }
    }
}

void sensorsMeasurement(void *params)
{
    gpio_set_direction( POWER_CTRL, GPIO_MODE_OUTPUT );
	gpio_set_level(POWER_CTRL, 1);
    soilConfig();
    if(xSemaphoreTake(semaphoreRTC, portMAX_DELAY))
    {   
        vTaskDelay(2000/portTICK_PERIOD_MS);
        while(true)
        {
            DHTerrorHandler(readDHT());
            humidity(); 
            vTaskDelay(pdMS_TO_TICKS(30000)); // mide cada 30 seg.
        }
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreMqttConection = xSemaphoreCreateBinary();
    semaphoreRTC           = xSemaphoreCreateBinary();

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

    xTaskCreate(&sensorsMeasurement,
                "Comenzando mediciones de sensores",
                4096,
                NULL,
                1,
                NULL);
}
