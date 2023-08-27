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
SemaphoreHandle_t semaphoreSensorConfig = NULL;     // en mqttcj.c
//SemaphoreHandle_t semaphoreLux = NULL;
SemaphoreHandle_t semaphoreOta = NULL;              // en ntp.c


TaskHandle_t xHandle = NULL;

config_data configuration;

void touchConfig(void);


void mqttServerConection(void *params)
{   
    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)) // espera la conexión WiFi
        {
            adjust_time(configuration.time_zone);
            mqtt_start();
        }
    }
}


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


void riego_auto(void * params)
{

    while(true)
    {
        if(configuration.control_riego_1)
        {
            if(humidity(SENSOR1) < configuration.hum_inf_1)
            {
                int cant_riegos_1 = 0;

                while(humidity(SENSOR1) < configuration.hum_sup_1 && cant_riegos_1 < 10)
                {
                    regar(0.3, VALVE1); // actualizada
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    cant_riegos_1 += 1;
                }
                const char *prefijo_1 = "S1: ";
                ultimoRiego(prefijo_1);  
            }
        }
        if(configuration.control_riego_2)
        {
            if(humidity(SENSOR2) < configuration.hum_inf_2)
            {
                int cant_riegos_2 = 0;

                while(humidity(SENSOR2) < configuration.hum_sup_2 && cant_riegos_2 < 10)
                {
                    regar(0.3, VALVE2);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    cant_riegos_2 += 1;
                }
                const char *prefijo_2 = "S2: ";
                ultimoRiego(prefijo_2);  
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
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

void sensorCofig(void * params){  // espera a que se suscriba al topic 

    if(xSemaphoreTake(semaphoreSensorConfig, portMAX_DELAY)){

        bh1750_init();
        vTaskDelete(NULL);
    }

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



void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta           = xSemaphoreCreateBinary();
    semaphoreSensorConfig  = xSemaphoreCreateBinary();

    
    soilConfig();

    if(init_irs()!=ESP_OK){
        ESP_LOGE("GPIO", "Falla configuración de irs\n");
    }
    

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
    
    //vTaskSuspend(xHandle);

}

