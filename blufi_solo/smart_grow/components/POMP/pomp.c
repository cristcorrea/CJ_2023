#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pomp.h"
#include "header.h"
#include "mqtt.h"

/* Modificacion a PWM*/
#include "driver/ledc.h"

int duty_bomba = 0; 

//////////////////////

#define TAG   "RIEGO"

volatile int flow_frequency = 0;
bool stop = false; 
const unsigned long TIEMPO_MAX_SIN_PULSOS  = 10000;

extern config_data configuration; 
extern SemaphoreHandle_t semaphoreRiego; 
 
void flow_sensor_isr(void* arg)
{
    flow_frequency++;  // Incrementar la frecuencia cuando se produce una interrupción
}


esp_err_t init_irs(void){

    esp_err_t err = ESP_OK; 
    gpio_config_t flow_sensor_config;
    flow_sensor_config.intr_type = GPIO_INTR_NEGEDGE;
    flow_sensor_config.mode = GPIO_MODE_INPUT;
    flow_sensor_config.pin_bit_mask = (1ULL << FLOW_SENSOR_PIN);
    flow_sensor_config.pull_up_en = GPIO_PULLUP_DISABLE;
    flow_sensor_config.pull_down_en = GPIO_PULLDOWN_ENABLE;

    err = gpio_config(&flow_sensor_config);
    if(err != ESP_OK){
        return err; 
    }
    err = gpio_install_isr_service(0);
    if(err != ESP_OK){
        return err; 
    }
    err = gpio_isr_handler_add(FLOW_SENSOR_PIN, flow_sensor_isr, NULL);
    if(err != ESP_OK){
        return err; 
    }
    esp_log_level_set("gpio", ESP_LOG_INFO);
    return err;
}

void nFault_isr(void* arg)
{
    for(int i = 0; i < 2; i++)
    {
        encenderLedTouch();
        vTaskDelay(pdMS_TO_TICKS(500));
        apagarLedTouch();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
}

esp_err_t init_nFault(void){

    esp_err_t err = ESP_OK; 
    gpio_config_t nFault_config;
    nFault_config.intr_type = GPIO_INTR_NEGEDGE;
    nFault_config.mode = GPIO_MODE_INPUT;
    nFault_config.pin_bit_mask = (1ULL << nFAULT_PIN);
    nFault_config.pull_up_en = GPIO_PULLUP_DISABLE;
    nFault_config.pull_down_en = GPIO_PULLDOWN_DISABLE;

    err = gpio_config(&nFault_config);
    if(err != ESP_OK){
        return err; 
    }
    err = gpio_install_isr_service(0);
    if(err != ESP_OK){
        return err; 
    }
    err = gpio_isr_handler_add(nFAULT_PIN, nFault_isr, NULL);
    if(err != ESP_OK){
        return err; 
    }
    esp_log_level_set("gpio", ESP_LOG_INFO);
    return err;
}

void riego_config()
{

    ledc_channel_config_t bomba_config = {0};
    bomba_config.gpio_num = BOMBA; 
    bomba_config.speed_mode = 1;
    bomba_config.channel = LEDC_CHANNEL_0;  
    bomba_config.intr_type = LEDC_INTR_DISABLE;
    bomba_config.timer_sel = LEDC_TIMER_0; 
    bomba_config.duty = 0; 
    ledc_channel_config(&bomba_config);

    ledc_timer_config_t bomba_timer_config = {0};
    bomba_timer_config.speed_mode = 1;
    bomba_timer_config.duty_resolution = LEDC_TIMER_10_BIT;
    bomba_timer_config.timer_num = LEDC_TIMER_0; 
    bomba_timer_config.freq_hz = 20000; 
    ledc_timer_config(&bomba_timer_config);


    gpio_config_t enable_config;
    enable_config.pin_bit_mask = (1ULL << ENABLE_DRV);
    enable_config.mode = GPIO_MODE_OUTPUT;
    enable_config.pull_up_en = GPIO_PULLUP_DISABLE;
    enable_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    enable_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&enable_config);

    
    gpio_config_t valve1_config;
    valve1_config.pin_bit_mask = (1ULL << VALVE1);
    valve1_config.mode = GPIO_MODE_OUTPUT;
    valve1_config.pull_up_en = GPIO_PULLUP_DISABLE;
    valve1_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    valve1_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&valve1_config);

    gpio_config_t valve2_config;
    valve2_config.pin_bit_mask = (1ULL << VALVE2);
    valve2_config.mode = GPIO_MODE_OUTPUT;
    valve2_config.pull_up_en = GPIO_PULLUP_DISABLE;
    valve2_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    valve2_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&valve2_config);

    gpio_config_t sensorF_ena;
    sensorF_ena.pin_bit_mask = (1ULL << SENSORF_ENABLE);
    sensorF_ena.mode = GPIO_MODE_OUTPUT;
    sensorF_ena.pull_up_en = GPIO_PULLUP_DISABLE;
    sensorF_ena.pull_down_en = GPIO_PULLDOWN_ENABLE;
    sensorF_ena.intr_type = GPIO_INTR_DISABLE; 
    gpio_config(&sensorF_ena);

}

void set_pwm_duty(void)
{
    ledc_set_duty(1, LEDC_CHANNEL_0, duty_bomba);
    ledc_update_duty(1,LEDC_CHANNEL_0);
}

void getUpDriver()
{
    gpio_set_level(ENABLE_DRV, 1);
}

void sleepDriver()
{
    gpio_set_level(ENABLE_DRV, 0);
}


void encender_bomba()
{
    for(int i = 0; i < 1020; i += 10)
    {
        duty_bomba += 10;
        set_pwm_duty();
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
    duty_bomba = 1023;  
    set_pwm_duty();

}

void apagar_bomba()
{
    duty_bomba = 0; 
    set_pwm_duty();
}

void abrir_valvula(gpio_num_t valvula)
{
    gpio_set_level(valvula, 1);
}

void cerrar_valvula(gpio_num_t valvula)
{
    gpio_set_level(valvula, 0);
}

void stopRiego()
{
    stop = false; 
}

void habilitarSensorFlujo(void)
{
    gpio_set_level(SENSORF_ENABLE, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void desHabilitarSensorFlujo(void)
{
    gpio_set_level(SENSORF_ENABLE, 1);
}


void enviarEstadoRiego(gpio_num_t valve, int total, int parcial)
{ 
    int porcentaje = 0;
    porcentaje = ((parcial/2.0636f) * 100) / total; 
    const char * prefijo; 
    if(valve == VALVE1)
    {
        prefijo = "V1";
    }else{
        prefijo = "V2";
    }
    size_t message_size = snprintf(NULL, 0, "%s, %i", prefijo, porcentaje) + 1;
    char *message = (char *)malloc(message_size);
    if (message != NULL) {
        snprintf(message, message_size, "%s, %d", prefijo, porcentaje);
        enviar_mensaje_mqtt(configuration.cardId, message);
        free(message);
    }
}

void regar(int lts_final, gpio_num_t valve){

    int lts_actual = 0;
    int contador = 0; 
    int tiempo_sin_pulsos = 0; 
    stop = true;
    int contador_envio = 0; 
    
    int pulsos_total = (2.0636f * lts_final) - 3.8293f; // Calculo cantidad de pulsos a contar. 
    habilitarSensorFlujo();
    getUpDriver();
    vTaskDelay(pdMS_TO_TICKS(20));
    abrir_valvula(valve);
    encender_bomba();

    flow_frequency = 0; 

    while((contador < pulsos_total) && (tiempo_sin_pulsos < TIEMPO_MAX_SIN_PULSOS) && stop){ 

        contador += flow_frequency;

        if (flow_frequency == 0)
        {
                tiempo_sin_pulsos += 100;
        }else{
                tiempo_sin_pulsos = 0;
        }   
        if(contador_envio < 10)
        {
            contador_envio++; 
        }else{
            enviarEstadoRiego(valve, lts_final, contador);
            contador_envio = 0;    
        }

        flow_frequency = 0;

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    apagar_bomba();
    cerrar_valvula(valve);
    sleepDriver();
    desHabilitarSensorFlujo();

    const char *prefijo;
    if(valve == VALVE1)
    {
        prefijo = "S1,";
    }else{
        prefijo = "S2,";
    }

    //lts_actual = contador * 0.17f; 
    lts_actual = contador/2.0636f;

    ultimoRiego(prefijo, lts_actual);
    xSemaphoreGive(semaphoreRiego);
}

/*
Sensor anterior que contaba distinta cantidad de pulsos por vuelta: 
int pulsos_total = lts_final * 4.825580 + 4.988814; 

*/