#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
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
#define ERASED  35
#define WIFI_RETRY_INTERVAL_MS (10000)

static const char* TAG = "Button press";

SemaphoreHandle_t semaphoreWifiConection = NULL;    // en blufi.c
SemaphoreHandle_t semaphoreOta = NULL;              // en ntp.c
SemaphoreHandle_t semaphoreRiego = NULL;            // Controla los recursos del riego
SemaphoreHandle_t semaphoreFecha = NULL;            // En mqtt, habilita tarea de poner en hora 

QueueHandle_t riegoQueue; 

config_data configuration;


int sensor1_on = 0;
int sensor2_on = 0; 

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

/*
Creo una tarea que realice el ajuste de hora cada x cantidad de tiempo
Esta tarea se habilita con un semaforo luego de que se haya efectuado la conexion mqtt
Si el año se establece correctamente, la tarea se elimina, sino continua hasta ajustar la hora.
*/

void erased_nvs(void *params)  // esta pasa a ser funcion del boton de multiples usos 
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
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

}

void riegoAuto1(void *params)
{
    mensajeRiego riego1;
    riego1.cantidad = 100; 
    riego1.valvula = VALVE1;  
    while(true)
    {
         if(configuration.control_riego_1)
        {
            if(humidity(SENSOR1) < configuration.hum_inf_1 || humidity(SENSOR1) > configuration.hum_sup_1)
            {
                xQueueSend(riegoQueue, &riego1, portMAX_DELAY);
                ESP_LOGE("Queue", "Error al poner en cola");
                //const char *prefijo_1 = "S1: ";
                //ultimoRiego(prefijo_1, 150);  
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

void riegoAuto2(void *params) // colocar xhandle para pausar tarea en ambos riegos automaticos
{   
    mensajeRiego riego2;
    riego2.cantidad = 100; 
    riego2.valvula = VALVE2; 
    while(true)
    {
        if(configuration.control_riego_2)
        {
            if(humidity(SENSOR2) < configuration.hum_inf_2 || humidity(SENSOR2) > configuration.hum_sup_2)
            {
                xQueueSend(riegoQueue, &riego2, portMAX_DELAY);
                //const char *prefijo_1 = "S1: ";
                //ultimoRiego(prefijo_1, 150);  
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
                vTaskDelete(NULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta           = xSemaphoreCreateBinary();
    semaphoreFecha         = xSemaphoreCreateBinary();
    semaphoreRiego         = xSemaphoreCreateMutex();
    riegoQueue             = xQueueCreate(20, sizeof(mensajeRiego));
    
    
    if(init_irs()!=ESP_OK){
        ESP_LOGE("GPIO", "Falla configuración de irs\n");
    }
    
    soilConfig();
    touchConfig();
    timer_config();
    riego_config();

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

/*
    xTaskCreate(&erased_nvs,
                "Habilita borrado de NVS",
                2048,
                NULL,
                1,
                NULL);


    xTaskCreate(&ota_update,
                "Instala nueva versión de firmware",
                8048,
                NULL,
                5,
                NULL);
    
    */
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
}

