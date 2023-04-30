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
#include "bh1750.h" 
#include "pomp.h"

#define POWER_CTRL 4
#define ERASED     35 

static const char* TAG = "ISR";
static const char* TAGQ = "Queue"; 


SemaphoreHandle_t semaphoreWifiConection = NULL;
SemaphoreHandle_t semaphoreMqttConection = NULL;
SemaphoreHandle_t semaphoreRTC = NULL;

QueueHandle_t blufi_queue; 



dht DHT_DATA;
soil SOIL_DATA;
/*Estructura para manipular configuración*/
typedef struct
{
    char UUID[14];      // debe almacenar el identificador recibido en custom message
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

    char message[140];
    if (xSemaphoreTake(semaphoreMqttConection, portMAX_DELAY)) // establecida la conexión con el broker
    {   
        if(xQueueReceive(blufi_queue, &configuration.UUID, pdMS_TO_TICKS(100)))
        {
            NVS_write("queue", &configuration.UUID);        

        }

        while (true)
        {   
            vTaskDelay(pdMS_TO_TICKS(60000)); // espera 1 minuto y envía
            time_t now = 0;
            struct tm timeinfo = {0};
            time(&now);
            localtime_r(&now, &timeinfo);
            char strftime_buf[64]; 
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            sprintf(message, "Temp: %.1f °C Hum: %i%% Soil: %i%%  Time: %s", 
            DHT_DATA.temperature, DHT_DATA.humidity, SOIL_DATA.humidity,
             strftime_buf);
            /*aca tengo que chequear que el UUID sea correcto*/
            ESP_LOGI(TAGQ, "Topic creado: %s", configuration.UUID);
            enviar_mensaje_mqtt(configuration.UUID, message);

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
        vTaskDelay(5000/portTICK_PERIOD_MS);
        while(true)
        {
            DHTerrorHandler(readDHT());
            humidity(); 
            vTaskDelay(pdMS_TO_TICKS(50000)); // mide cada 50 seg.
        }
    }
}


void erased_nvs(void *params)
{
    gpio_config_t isr_config;
    isr_config.pin_bit_mask = (1ULL << ERASED);
    isr_config.mode = GPIO_MODE_INPUT;
    isr_config.pull_up_en = GPIO_PULLUP_ENABLE;
    isr_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    isr_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&isr_config);

    while(true)
    {
        int button_state = gpio_get_level(ERASED);
        if(button_state == 0)
        {
            vTaskDelay(3000/portTICK_PERIOD_MS);
            button_state = gpio_get_level(ERASED);
            if(button_state == 0)
            {
                ESP_LOGE(TAG, "Borrando NVS...");
                vTaskDelay(1000/portTICK_PERIOD_MS);
                nvs_flash_erase();
                esp_restart();

            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void light_meter(void * params)
{
    bh1750_init();
    while(true)
    {
        bh1750_read();
        vTaskDelay(20000/portTICK_PERIOD_MS);
    }
}


void pomp(void * params)
{
    riego_config();
    while(true)
    {
        if(SOIL_DATA.humidity < configuration.hum_inf)
        {
            // frenar medicion de humidity() para no entrar en conflicto
            while(SOIL_DATA.humidity < configuration.hum_sup)
            {
                regar();
                humidity();
                vTaskDelay(20/portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreMqttConection = xSemaphoreCreateBinary();
    semaphoreRTC           = xSemaphoreCreateBinary();

    blufi_start();

    blufi_queue = xQueueCreate(10, sizeof(char[13]));

    NVS_read("queue", configuration.UUID);

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

    xTaskCreate(&erased_nvs,
                "Habilita borrado de NVS",
                2048,
                NULL,
                1,
                NULL);

    xTaskCreate(&light_meter,
                "Inicia medicion de luz ambiente",
                2048,
                NULL,
                1,
                NULL);

    xTaskCreate(&pomp,
                "Inicia control de riego",
                2048,
                NULL,
                1,
                NULL);


}

