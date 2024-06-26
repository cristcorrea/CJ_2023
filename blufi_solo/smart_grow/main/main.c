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
#define TOUCH_VALUE_MIN   200//330
#define WIFI_RECONNECTION_MAXIMUM_RETRY 360


#define TAG1 "CREACION"

SemaphoreHandle_t semaphoreWifiConection = NULL;    // en blufi.c
SemaphoreHandle_t semaphoreOta = NULL;              // en ntp.c
SemaphoreHandle_t semaphoreRiego = NULL;            // Controla los recursos del riego
SemaphoreHandle_t semaphoreFecha = NULL;            // En mqtt, habilita tarea de poner en hora 
SemaphoreHandle_t semaphoreSensorSuelo = NULL;      // Controla la consulta al sensor de suelo     

TaskHandle_t msjTaskHandle;
TaskHandle_t riegoHasta1Handle; 
TaskHandle_t riegoHasta2Handle;
TaskHandle_t riegoAuto1Handle; 
TaskHandle_t riegoAuto2Handle;
TaskHandle_t reconexionHandle; 
TaskHandle_t humidityMeasureHandle; 
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
            ESP_LOGI("RIEGO 1 AUTO", "Entra a riego auto 1"); 
            habilitarSensorSuelo(10);
            if(sensorConectado(S1_STATE))
            {
                if(configuration.soilHumidity1 < configuration.hum_inf_1) 
                {
                    vTaskResume(riegoHasta1Handle);
                }
            }
            desHabilitarSensorSuelo();
            vTaskSuspend(riegoAuto1Handle);

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
            habilitarSensorSuelo(10);
            if(sensorConectado(S2_STATE))
            {
                ESP_LOGI("RIEGO AUTO 2", "ENTRA A RIEGO AUTO 2");
                if(configuration.soilHumidity2 < configuration.hum_inf_2)
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
        habilitarSensorSuelo(10); 
        if(sensorConectado(S1_STATE) && configuration.control_riego_1)
        {
            if(configuration.soilHumidity1 < (configuration.hum_sup_1 - 5))
            {
                xQueueSend(riegoQueue, &riego1, portMAX_DELAY);
            }
            else if ((configuration.hum_sup_1 - 3) < configuration.soilHumidity1 && configuration.soilHumidity1 < (configuration.hum_sup_1 + 3))
            {
                vTaskResume(riegoAuto1Handle);
                vTaskSuspend(riegoHasta1Handle);
            }

        } 
        desHabilitarSensorSuelo(); 
        vTaskDelay(pdMS_TO_TICKS(300000)); // espera 5 minutos
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
        habilitarSensorSuelo(10);
        if(sensorConectado(S2_STATE) && configuration.control_riego_2)
        {
            if(configuration.soilHumidity2 < (configuration.hum_sup_2 - 5))
            {
                xQueueSend(riegoQueue, &riego2, portMAX_DELAY);
            }
            else if ((configuration.hum_sup_2 - 3) < configuration.soilHumidity2 && configuration.soilHumidity2 < (configuration.hum_sup_2 + 3))
            {
                vTaskResume(riegoAuto2Handle);
                vTaskSuspend(riegoHasta2Handle);
            }
        }
        desHabilitarSensorSuelo(); 
        vTaskDelay(pdMS_TO_TICKS(300000));
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
                vTaskResume(humidityMeasureHandle);
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
        if(configuration.intentosReconectar <= WIFI_RECONNECTION_MAXIMUM_RETRY)
        {
            wifi_connect();
            configuration.intentosReconectar++; 
        }else{
            esp_restart(); 
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void humidityMeasure(void *params)
{
    int time; 

    while(true)
    {   
        if(!configuration.control_riego_1 && !configuration.control_riego_2)
        {
            time = 30000; 
        }
        else{
            time = 10000; 
        }
        habilitarSensorSuelo(600);  // habilita sensores y espera 1000 ms
        configuration.soilHumidity1 = humidity(SENSOR1);
        configuration.soilHumidity2 = humidity(SENSOR2);  
        desHabilitarSensorSuelo(); 
        vTaskDelay(pdMS_TO_TICKS(time));
    }
}


void checkMemoryTask(void *pvParameters) {
    while(1) {
        size_t freeHeapSize = esp_get_free_heap_size();
        ESP_LOGI("MEM_CHECK", "Free heap size: %d bytes", freeHeapSize);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 10 segundos
    }
}

void initialize_semaphores() {
    semaphoreWifiConection = xSemaphoreCreateBinary();
    semaphoreOta = xSemaphoreCreateBinary();
    semaphoreFecha = xSemaphoreCreateBinary();
    semaphoreRiego = xSemaphoreCreateMutex();
    semaphoreSensorSuelo = xSemaphoreCreateMutex();
    // Verificar que todos los semáforos fueron creados correctamente
}



/**
 * @brief Crea una tarea y verifica si fue creada exitosamente.
 * 
 * @param task_function Puntero a la función de la tarea.
 * @param task_name Nombre de la tarea para propósitos de depuración.
 * @param stack_depth Profundidad de la pila de la tarea.
 * @param parameters Parámetros pasados a la tarea.
 * @param priority Prioridad de la tarea.
 * @param task_handle Handle de la tarea, puede ser NULL si no es necesario.
 * @return true si la tarea fue creada exitosamente, false en caso contrario.
 */
bool create_and_validate_task(TaskFunction_t task_function, const char* task_name, const uint32_t stack_depth, void *parameters, const UBaseType_t priority, TaskHandle_t *task_handle)
{
    BaseType_t result;

    // Crear la tarea
    result = xTaskCreate(task_function, task_name, stack_depth, parameters, priority, task_handle);

    // Verificar si la tarea fue creada correctamente
    if (result != pdPASS) {
        ESP_LOGE(task_name, "Fallo al crear la tarea");
        return false;
    }

    // Si se proporcionó un handle de tarea, suspenderla inicialmente si es necesario
    if (task_handle && *task_handle) {
        vTaskSuspend(*task_handle);
        ESP_LOGI(task_name, "Tarea creada y suspendida exitosamente");
    } else {
        ESP_LOGI(task_name, "Tarea creada exitosamente");
    }

    return true;
}

void read_and_set_configuration()
{
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
    configuration.semaforoWifiState = false; 
    configuration.intentosReconectar = 0; 
    xSemaphoreGive(semaphoreSensorSuelo); 
}

void start_all_tasks() {
    create_and_validate_task(mqttServerConection, "Conectando con HiveMQ Broker", 4096, NULL, 1, NULL);
    create_and_validate_task(ota_update, "Instala nueva versión de firmware", 8192, NULL, 1, NULL);
    create_and_validate_task(sensorCofig, "Inicia configuracion de sensores", 2048, NULL, 1, NULL);
    create_and_validate_task(touchSensor, "Sensor touch", 2048, NULL, 1, NULL);
    create_and_validate_task(riegoAuto1, "Riego automatico 1", 2048, NULL, 1, &riegoAuto1Handle);
    create_and_validate_task(riegoAuto2, "Riego automatico 2", 2048, NULL, 1, &riegoAuto2Handle);
    create_and_validate_task(controlRiego, "Maneja la cola de riego", 4096, NULL, 1, NULL);
    create_and_validate_task(ajusteFecha, "Ajusta la hora y la fecha", 2048, NULL, 1, NULL);
    create_and_validate_task(reconexionWifi, "Gestiona la reconexion Wi-Fi", 2048, NULL, 2, &reconexionHandle);
    create_and_validate_task(envioDatos, "Envia datos cada una hora", 4096, NULL, 1, &msjTaskHandle);
    create_and_validate_task(riegaHasta1, "Riego automatico 1", 2048, NULL, 1, &riegoHasta1Handle);
    create_and_validate_task(riegaHasta2, "Riego automatico 2", 2048, NULL, 1, &riegoHasta2Handle);
    create_and_validate_task(humidityMeasure, "Mide humedad", 8192, NULL, 1, &humidityMeasureHandle);
}


void app_main(void)
{

    initialize_semaphores();
    riegoQueue             = xQueueCreate(20, sizeof(mensajeRiego));

    init_irs(); 
    init_nFault();
    soilConfig();
    touchConfig();
    touchLedConfig();
    wifiLedConfig();
    riego_config();
    blufi_start();
    read_and_set_configuration();
  
    start_all_tasks();

    //xTaskCreate(&checkMemoryTask, "memory_check_task", 2048, NULL, 1, NULL);


}





