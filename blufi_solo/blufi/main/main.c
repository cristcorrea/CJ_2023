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
#include "header.h"
#include "ota.h"

#define POWER_CTRL 4
#define ERASED     35 

static const char* TAG = "Button press";

SemaphoreHandle_t semaphoreWifiConection = NULL; 
SemaphoreHandle_t semaphoreSensorConfig = NULL;
//SemaphoreHandle_t semaphoreLux = NULL;
SemaphoreHandle_t semaphoreOta = NULL; // en ntp.c


TaskHandle_t xHandle = NULL;

config_data configuration;

void mqttServerConection(void *params)
{   
    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)) // establecida la conexión WiFi
        {
            adjust_time(configuration.time_zone);
            mqtt_start();
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
    timer_config();
    while(true)
    {
        int button_state = gpio_get_level(ERASED);
        if(button_state == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(3000));
            button_state = gpio_get_level(ERASED);
            if(button_state == 0)
            {
                ESP_LOGE(TAG, "Borrando NVS...");
                nvs_flash_erase();
                esp_restart();

            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void riego_auto(void * params)
{
    riego_config();
    while(true)
    {
        ESP_LOGI("RIEGO AUTO", "Entra a tarea de riego automático\n");
        
        if(humidity() < configuration.hum_inf)
        {
            int cant_riegos = 0;

            while(humidity() < configuration.hum_sup && cant_riegos < 10)
            {
                regar(0.3);
                vTaskDelay(pdMS_TO_TICKS(2000));
                cant_riegos += 1;
            }
            ultimoRiego();
        }
        
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}


void ota_update(void * params)
{
    if(xSemaphoreTake(semaphoreOta, portMAX_DELAY))
    {
        while(true)
        {   
            update_ota();
            vTaskDelay(pdMS_TO_TICKS(36000000));
        }
    }
   
}

void sensorCofig(void * params){

    if(xSemaphoreTake(semaphoreSensorConfig, portMAX_DELAY)){

        bh1750_init();
        vTaskDelete(NULL);
    }

}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta           = xSemaphoreCreateBinary();
    semaphoreSensorConfig  = xSemaphoreCreateBinary();
    gpio_set_direction( POWER_CTRL, GPIO_MODE_OUTPUT );
    gpio_set_level(POWER_CTRL, 1);
    soilConfig();
    if(init_irs()!=ESP_OK){
        ESP_LOGE("GPIO", "Falla configuración de irs\n");
    }
    blufi_start();

    if(NVS_read("MAC", configuration.MAC) == ESP_OK)
    {
        if(true)//NVS_read("time_zone", configuration.time_zone) != ESP_OK
        {   
            configuration.time_zone = (char *) malloc(strlen("CET-1CEST,M3.5.0,M10.5.0/3") + 1);
            strcpy(configuration.time_zone, "CET-1CEST,M3.5.0,M10.5.0/3"); 
            
        }

        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage2", NVS_READWRITE, &my_handle);


        if(err != ESP_OK)
        {
            ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }else{
            int result = 0; 
            if(nvs_get_i32(my_handle, "control_riego", (int32_t*)&result) != ESP_OK)
            {
                configuration.control_riego = 0;
            }else{
                configuration.control_riego = result; 
            }
            if(nvs_get_i32(my_handle, "hum_sup", (int32_t*)&result))
            {
                configuration.hum_sup = 60;
            }else{
                configuration.hum_sup = result; 
            }
            if(nvs_get_i32(my_handle, "hum_inf", (int32_t*)&result))
            {
                configuration.hum_inf = 20; 
            }else{
                configuration.hum_inf = result;
            }
            nvs_close(my_handle);
        }

    }

    xTaskCreate(&mqttServerConection,
                "Conectando con HiveMQ Broker",
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

    xTaskCreate(&riego_auto,
                "Inicia control de riego",
                2048,
                NULL,
                1,
                &xHandle);

    xTaskCreate(&ota_update,
                "Instala nueva versión de firmware",
                8048,
                NULL,
                5,
                NULL);
        
    xTaskCreate(&sensorCofig,
                "Inicia configuracion de sensores",
                2048,
                NULL,
                2,
                NULL);
    
    vTaskSuspend(xHandle);

}

