#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_blufi_api.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"

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

#define TOUCH   TOUCH_PAD_NUM5
#define TOUCH_VALUE_MIN 150

SemaphoreHandle_t semaphoreWifiConection = NULL;    // en blufi.c
SemaphoreHandle_t semaphoreOta = NULL;              // en ntp.c
SemaphoreHandle_t semaphoreRiego = NULL;            // Controla los recursos del riego
SemaphoreHandle_t semaphoreFecha = NULL;            // En mqtt, habilita tarea de poner en hora 

QueueHandle_t riegoQueue; 
config_data configuration;

void touchConfig(void);

void mqttServerConection(void *params)
{   
    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)) // espera la conexión WiFi
        {
            mqtt_start();
        }
    }
}


void ota_update(void * params)  // espera a que se ponga en hora 
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

    bh1750_init();
    vTaskDelete(NULL);

}

void touchConfig(void)
{
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V);
    touch_pad_config(TOUCH, -200);
}

void touchSensor(void *params)
{
    uint16_t touch_value;

    while(true)
    {   
        touch_pad_read(TOUCH, &touch_value);
        //printf("T%d:[%4"PRIu16"] ", TOUCH, touch_value);
        //printf("\n");
        
        if(touch_value < TOUCH_VALUE_MIN)
        {
            vTaskDelay(pdMS_TO_TICKS(3000));
            touch_pad_read(TOUCH, &touch_value);
            if(touch_value < TOUCH_VALUE_MIN)
            {
                nvs_flash_erase();
                esp_restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void riegoAuto1(void *params)
{
    mensajeRiego riego1;
    riego1.cantidad = 100; 
    riego1.valvula = VALVE1;  
    while(true)
    {    
         if(configuration.control_riego_1 && sensorConectado(SENSOR1))
        {
            if(humidity(SENSOR1) < configuration.hum_inf_1 || humidity(SENSOR1) > configuration.hum_sup_1)
            {
                xQueueSend(riegoQueue, &riego1, portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

void riegoAuto2(void *params) 
{   
    mensajeRiego riego2;
    riego2.cantidad = 100; 
    riego2.valvula = VALVE2; 
    while(true)
    {
        if(configuration.control_riego_2 && sensorConectado(SENSOR2))
        {
            if(humidity(SENSOR2) < configuration.hum_inf_2 || humidity(SENSOR2) > configuration.hum_sup_2)
            {
                xQueueSend(riegoQueue, &riego2, portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

void controlRiego(void *params)
{
    while(true)
    {
        mensajeRiego mensaje; 
        if(xQueueReceive(riegoQueue, &mensaje, portMAX_DELAY))
        {
            xSemaphoreTake(semaphoreRiego, portMAX_DELAY);
            regar(mensaje.cantidad, mensaje.valvula);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ajusteFecha(void *params)
{
    while(true)
    {
        if(xSemaphoreTake(semaphoreFecha, portMAX_DELAY))
        {
            adjust_time(configuration.time_zone);
            int anio = consultaAnio();
            if(anio != 1970)
            {
                ESP_LOGI("Ajuste de hora", "Año obtenido: %i", anio);
                //free(configuration.time_zone);
                vTaskDelete(NULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void envioDatos(void *params)
{
    while(true)
    {   
        vTaskDelay(pdMS_TO_TICKS(3600000));
        enviarDatos();
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta           = xSemaphoreCreateBinary();
    semaphoreFecha         = xSemaphoreCreateBinary();
    semaphoreRiego         = xSemaphoreCreateMutex();
    riegoQueue             = xQueueCreate(20, sizeof(mensajeRiego));
    
    init_irs(); 
    soilConfig();
    touchConfig();
    timer_config();
    riego_config();
    blufi_start();
    
    if(NVS_read("MAC", &configuration.MAC) == ESP_OK)
    {
        /*
        if(NVS_read("time_zone", &configuration.time_zone) != ESP_OK)
        {   
            configuration.time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";
        }
        */
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage2", NVS_READWRITE, &my_handle);

        if(err != ESP_OK)
        {
            ESP_LOGI("nvs","Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }else{
            int result = 0; 
            if(nvs_get_i32(my_handle, "control_riego_1", (int32_t*)&result) != ESP_OK)
            {
                configuration.control_riego_1 = 0;
            }else{
                configuration.control_riego_1 = result; 
            }
            if(nvs_get_i32(my_handle, "control_riego_2", (int32_t*)&result) != ESP_OK)
            {
                configuration.control_riego_2 = 0;
            }else{
                configuration.control_riego_2 = result; 
            }

            if(nvs_get_i32(my_handle, "hum_sup_1", (int32_t*)&result))
            {
                configuration.hum_sup_1 = 60;
            }else{
                configuration.hum_sup_1 = result; 
            }
            if(nvs_get_i32(my_handle, "hum_inf_1", (int32_t*)&result))
            {
                configuration.hum_inf_1 = 20; 
            }else{
                configuration.hum_inf_1 = result;
            }

            if(nvs_get_i32(my_handle, "hum_sup_2", (int32_t*)&result))
            {
                configuration.hum_sup_2 = 60;
            }else{
                configuration.hum_sup_2 = result; 
            }
            if(nvs_get_i32(my_handle, "hum_inf_2", (int32_t*)&result))
            {
                configuration.hum_inf_2 = 20; 
            }else{
                configuration.hum_inf_2 = result;
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
    
    xTaskCreate(&touchSensor,
                "Sensor touch",
                2048,
                NULL,
                2,
                NULL);
    
    xTaskCreate(&riegoAuto1,
            "Riego automatico 1",
            2048,
            NULL,
            2,
            NULL);

    xTaskCreate(&riegoAuto2,
            "Riego automatico 2",
            2048,
            NULL,
            2,
            NULL);
            
    xTaskCreate(&controlRiego,
            "Maneja la cola de riego",
            2048,
            NULL,
            2,
            NULL);

    xTaskCreate(&ajusteFecha,
            "Ajusta la hora y la fecha",
            2048,
            NULL,
            4,
            NULL);

    xTaskCreate(&envioDatos,
            "Envia datos cada una hora",
            2048,
            NULL,
            1,
            NULL);
}

