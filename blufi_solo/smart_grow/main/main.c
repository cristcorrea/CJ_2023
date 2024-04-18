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
#define TOUCH_VALUE_MIN   330//348

#define TAG1 "CREACION"

SemaphoreHandle_t semaphoreWifiConection = NULL;    // en blufi.c
SemaphoreHandle_t semaphoreOta = NULL;              // en ntp.c
SemaphoreHandle_t semaphoreRiego = NULL;            // Controla los recursos del riego
SemaphoreHandle_t semaphoreFecha = NULL;            // En mqtt, habilita tarea de poner en hora 

TaskHandle_t msjTaskHandle;
TaskHandle_t riegoHasta1Handle; 
TaskHandle_t riegoHasta2Handle;
TaskHandle_t riegoAuto1Handle; 
TaskHandle_t riegoAuto2Handle;
TaskHandle_t reconexionHandle; 
QueueHandle_t riegoQueue; 
config_data configuration;

bool primerEnvio = true; 
bool primerConexion = true; 

/*
    @brief TASK: Una vez conectado a internet realiza la conexion MQTT.
*/
void mqttServerConection(void *params)
{   
    while (true)
    {
        if (xSemaphoreTake(semaphoreWifiConection, portMAX_DELAY)) // espera la conexión WiFi
        {
            mqtt_start();
            configuration.semaforoWifiState = true; 
        }
    }
}

/*
    @brief TASK: actualización por OTA 
*/
void ota_update(void * params)  // espera a que se ponga en hora 
{
    if(xSemaphoreTake(semaphoreOta, portMAX_DELAY))
    {
        while(true)
        {   
            if(primerConexion)
            {
                enviarVersion();
            }else{
                primerConexion = false; 
            }
            update_ota();
            vTaskDelay(pdMS_TO_TICKS(36000000));
        }
    }
}


/*
    @brief TASK: Configuración del sensor de luz BH1750. Se ejecuta sólo al inicio.  
*/
void sensorCofig(void * params){  

    bh1750_init();
    vTaskDelete(NULL);

}

/*
    @brief Configuración sensor touch. 
*/
void touchConfig(void)
{   
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V);
    touch_pad_config(TOUCH, -200);
}


/*
    @brief TASK: Control del sensor touch. 
*/
void touchSensor(void *params)
{
    uint16_t touch_value;

    while(true)
    {   
        touch_pad_read(TOUCH, &touch_value);
        //printf("T%d:[%4"PRIu16"] ", TOUCH, touch_value);
        //printf("\n");
        int contador = 0;
        if(touch_value < TOUCH_VALUE_MIN)
        {   
            encenderLedTouch();//gpio_set_level(TOUCH_LED, 1);

            while(touch_value < TOUCH_VALUE_MIN)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                touch_pad_read(TOUCH, &touch_value);
                contador++; 
                if(contador >= 30)
                {
                    nvs_flash_erase();
                    esp_restart();
                }
            }
            apagarLedTouch();//gpio_set_level(TOUCH_LED, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/*
    @brief TASK: Inicia riego automático del sensor 1. 
    Pone en marcha a riegoHasta1. 
*/
void riegoAuto1(void *params)
{
    while(true)
    {    
         if(configuration.control_riego_1)
        {
            habilitarSensorSuelo(250);
            if(sensorConectado(S1_STATE))
            {
                ESP_LOGI("RIEGO AUTO 1", "ENTRA A RIEGO AUTO 1");
                if(humidity(SENSOR1) < configuration.hum_inf_1) 
                {
                    vTaskResume(riegoHasta1Handle);
                    vTaskSuspend(riegoAuto1Handle);
                }
            }
            desHabilitarSensorSuelo();
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

/*
    @brief TASK: Inicia riego automático del sensor 2. 
    Pone en marcha a riegoHasta2. 
*/
void riegoAuto2(void *params) 
{   

    while(true)
    {
        if(configuration.control_riego_2)
        {
            habilitarSensorSuelo(250);
            if(sensorConectado(S2_STATE))
            {
                ESP_LOGI("RIEGO AUTO 2", "ENTRA A RIEGO AUTO 2");
                if(humidity(SENSOR2) < configuration.hum_inf_2)
                {
                    vTaskResume(riegoHasta2Handle);
                    vTaskSuspend(riegoAuto2Handle);
                }
            }
            desHabilitarSensorSuelo();
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

/* @brief TASK: Recive la habilitacion de riegoAuto1.  
   Continúa regando hasta que se supera el limite superior
*/
void riegaHasta1(void * params)
{
    mensajeRiego riego1;
    riego1.cantidad = 50; 
    riego1.valvula = VALVE1;
    while(true)
    {
        habilitarSensorSuelo(250);
        if(sensorConectado(S1_STATE) && configuration.control_riego_1 
            && (humidity(SENSOR1) < configuration.hum_sup_1))
        {
            xQueueSend(riegoQueue, &riego1, portMAX_DELAY);
        }else{
            vTaskResume(riegoAuto1Handle);
            vTaskSuspend(riegoHasta1Handle);
        }
        desHabilitarSensorSuelo();
        vTaskDelay(pdMS_TO_TICKS(100000));
    }
    
}

/* @brief TASK: Recive la habilitacion de riegoAuto2.  
   Continúa regando hasta que se supera el limite superior.
*/
void riegaHasta2(void * params)
{
    mensajeRiego riego2;
    riego2.cantidad = 50; 
    riego2.valvula = VALVE2;
    while(true)
    {
        habilitarSensorSuelo(250);
        if(sensorConectado(S2_STATE) && configuration.control_riego_2 
            && (humidity(SENSOR2) < configuration.hum_sup_2))
        {
            ESP_LOGI("RIEGO HASTA 2", "ENVIA PETICION DE RIEGO");
            xQueueSend(riegoQueue, &riego2, portMAX_DELAY);
        }else{
            vTaskResume(riegoAuto2Handle);
            vTaskSuspend(riegoHasta2Handle);
        }
        desHabilitarSensorSuelo();
        vTaskDelay(pdMS_TO_TICKS(100000));
    }
    
}

/* 
    @brief TASK: Recibe la Queue, manda a regar y se bloquea hasta que finalice el riego.
*/
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

/*
    @brief TASK: Pone en hora al iniciar el sistema. 
    Ok  -> Borra Task

*/
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


/*
    @ brief Envía datos al servidor (cardIdC) y a la app cada 1 hora. 
*/
void envioDatos(void *params)
{
    while(true)
    {   

        enviarDatos(configuration.cardId, false);

        if(primerEnvio) // Retrasa el primer envio de datos a la espera de que finalice la inicialización
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            enviarDatos(configuration.cardIdC, true);
            primerEnvio = false; 
        }else{
            enviarDatos(configuration.cardIdC, true);
        }
        
        vTaskDelay(pdMS_TO_TICKS(3600000));
    }
}

void reconexionWifi(void *params)
{
    while(true)
    {   
        ESP_LOGI("TASK RECONEXION", "INTENTA RECONECTAR");
        wifi_connect();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta           = xSemaphoreCreateBinary();
    semaphoreFecha         = xSemaphoreCreateBinary();
    semaphoreRiego         = xSemaphoreCreateMutex();
    riegoQueue             = xQueueCreate(20, sizeof(mensajeRiego));
    configuration.semaforoWifiState = false; 
    
    init_irs(); 
    init_nFault();
    soilConfig();
    touchConfig();
    touchLedConfig();
    wifiLedConfig();
    riego_config();
    blufi_start();
  
    
    if(NVS_read("cardId", &configuration.cardId) == ESP_OK)
    {
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage2", NVS_READWRITE, &my_handle);

        if(err != ESP_OK)
        {
            ESP_LOGI("nvs","Error (%s) opening NVS handle!\n", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }else{
            int result = 0;
            if(nvs_get_i32(my_handle, "time_zone", (int32_t*)&result) != ESP_OK)
            {
                configuration.time_zone = 100;
            }else{
                configuration.time_zone = result;
            }
            ESP_LOGI("NVS", "time_zone: %i", configuration.time_zone);
            if(nvs_get_i32(my_handle, "control_riego_1", (int32_t*)&result) != ESP_OK)
            {
                configuration.control_riego_1 = 0;
            }else{
                configuration.control_riego_1 = result; 
            }
            ESP_LOGI("CONTROL RIEGO", "");
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

            configuration.first_connection = false; 
   
            nvs_close(my_handle);
        }
    }else{
        configuration.first_connection = true; 
    }

    xTaskCreate(mqttServerConection,
                "Conectando con HiveMQ Broker",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(ota_update,
                "Instala nueva versión de firmware",
                8048,
                NULL,
                1,
                NULL);
    

    xTaskCreate(sensorCofig,
                "Inicia configuracion de sensores",
                2048,
                NULL,
                1,
                NULL);
    
    xTaskCreate(touchSensor,
                "Sensor touch",
                2048,
                NULL,
                1,
                NULL);
    
    xTaskCreate(riegoAuto1,
                "Riego automatico 1",
                2048,
                NULL,
                1,
                &riegoAuto1Handle);

    xTaskCreate(riegoAuto2,
                "Riego automatico 2",
                2048,
                NULL,
                1,
                &riegoAuto2Handle);

    xTaskCreate(controlRiego,
                "Maneja la cola de riego",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(ajusteFecha,
                "Ajusta la hora y la fecha",
                2048,
                NULL,
                1,
                NULL);

    if(xTaskCreate(reconexionWifi,
                "Gestiona la reconexion Wi-Fi",
                2048,
                NULL,
                1,
                &reconexionHandle) != pdPASS)
    {
        ESP_LOGE(TAG1, "FALLA AL CREAR reconexionWifi");
    }else{
        vTaskSuspend(reconexionHandle);
    }

    if(xTaskCreate(envioDatos,
                "Envia datos cada una hora",
                4096,
                NULL,
                1,
                &msjTaskHandle) != pdPASS)
    {
        ESP_LOGE(TAG1, "FALLA AL CREAR envioDatos");
    }else{
        vTaskSuspend(msjTaskHandle);
    }

    if(xTaskCreate(riegaHasta1,
                "Riego automatico 1",
                2048,
                NULL,
                1,
                &riegoHasta1Handle) != pdPASS)
    {
        ESP_LOGE(TAG1, "FALLA AL CREAR riegoHasta1");
    }else{
        vTaskSuspend(riegoHasta1Handle);
    }
    

    if(xTaskCreate(riegaHasta2,
                "Riego automatico 2",
                2048,
                NULL,
                1,
                &riegoHasta2Handle) != pdPASS)
    {
        ESP_LOGE(TAG1, "FALLA AL CREAR riegoHasta2");
    }else{
        vTaskSuspend(riegoHasta2Handle);
    }

    if (eTaskGetState(msjTaskHandle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "envioDatos SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "envioDatos NO SE SUSPENDIÓ");
    }

    if (eTaskGetState(riegoHasta1Handle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "riegaHasta1 SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "riegaHasta1 NO SE SUSPENDIÓ");
    }

    if (eTaskGetState(riegoHasta2Handle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "riegaHasta2 SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "riegaHasta2 NO SE SUSPENDIÓ");
    }
    
    if (eTaskGetState(reconexionHandle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "riegaHasta2 SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "riegaHasta2 NO SE SUSPENDIÓ");
    }


    if (eTaskGetState(msjTaskHandle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "envioDatos SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "envioDatos NO SE SUSPENDIÓ");
    }

    if (eTaskGetState(riegoHasta1Handle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "riegaHasta1 SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "riegaHasta1 NO SE SUSPENDIÓ");
    }

    if (eTaskGetState(riegoHasta2Handle) == eSuspended) {
        ESP_LOGI("ARRANQUE", "riegaHasta2 SUSPENDIDA");
    } else {
        ESP_LOGE("ARRANQUE", "riegaHasta2 NO SE SUSPENDIÓ");
    }

}



