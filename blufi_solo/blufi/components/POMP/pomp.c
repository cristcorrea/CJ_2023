#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "pomp.h"
#include "header.h"
#include "mqtt.h"


#define TAG   "RIEGO"

volatile int flow_frequency = 0;
bool stop = false; 
const unsigned long TIEMPO_MAX  = 60000000;

gptimer_handle_t gptimer = NULL; 

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
    gpio_config_t riego_config;
    riego_config.pin_bit_mask = (1ULL << BOMBA);
    riego_config.mode = GPIO_MODE_OUTPUT;
    riego_config.pull_up_en = GPIO_PULLUP_DISABLE;
    riego_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    riego_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&riego_config);

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
    gpio_set_level(BOMBA, 1); 
}

void apagar_bomba()
{
    gpio_set_level(BOMBA, 0);
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



void timer_config(){

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
}


void regar(int lts_final, gpio_num_t valve){

    int lts_actual = 0;
    int contador = 0; 
    stop = true;
    //int pulsos_total = lts_final * 4.825580 + 4.988814; // sensor anterior
    int pulsos_total = (2.0636f * lts_final) - 3.8293f; // sensor actual
    gptimer_enable(gptimer);
    gptimer_start(gptimer);
    uint64_t tiempo_inicial = 0;
    uint64_t tiempo_final = 0; 

    getUpDriver();
    vTaskDelay(pdMS_TO_TICKS(50));
    abrir_valvula(valve);
    vTaskDelay(pdMS_TO_TICKS(100));
    encender_bomba();
    
    //flow_frequency = 0;

    gptimer_get_raw_count(gptimer, &tiempo_inicial);
    gptimer_get_raw_count(gptimer, &tiempo_final);


    while((contador < pulsos_total) && ((tiempo_final - tiempo_inicial) < TIEMPO_MAX) && stop){ 


        if((tiempo_final - tiempo_inicial) >= TIEMPO_MAX/3 && flow_frequency == 0){
            ESP_LOGE("Watering", "No hay agua");
            stopRiego(); 
        } 

        contador += flow_frequency;
        gptimer_get_raw_count(gptimer, &tiempo_final);

        ESP_LOGI(TAG, " Contador: %i |Pulsos total: %i |Frecuencia: %i|Tiempo final:%" PRIu64 "\n",
         contador, pulsos_total, flow_frequency, tiempo_final);
            

        flow_frequency = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    gptimer_set_raw_count(gptimer, 0);
    gptimer_stop(gptimer);
    gptimer_disable(gptimer);

    apagar_bomba();
    cerrar_valvula(valve);
    sleepDriver();

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
void regar(int lts_final, gpio_num_t valve){

    float lts_actual = 0;
    int contador = 0; // variable para contar la totalidad de impulsos 
    gptimer_enable(gptimer);
    gptimer_start(gptimer);
    uint64_t tiempo_inicial = 0;
    uint64_t tiempo_final = 0; 

    encender_bomba();
    abrir_valvula(valve);
    flow_frequency = 0;
    gptimer_get_raw_count(gptimer, &tiempo_inicial);
    gptimer_get_raw_count(gptimer, &tiempo_final);
    
    while((lts_actual <= lts_final) && ((tiempo_final - tiempo_inicial) < TIEMPO_MAX)){ 

        float freq = (float)flow_frequency / 0.1f;
        contador += flow_frequency;  // subo frecuencia parcial a contador 

        if((tiempo_final - tiempo_inicial) >= TIEMPO_MAX/3 && flow_frequency == 0){
            ESP_LOGE("Watering", "No hay agua");
        } 

        float flow_rate = (freq * 1000.0f) / (98.0f * 60.0f);
        float flow_rate_with_err = flow_rate * 0.02f + flow_rate;
        lts_actual += flow_rate_with_err * 0.1f;

        gptimer_get_raw_count(gptimer, &tiempo_final);
        
        ESP_LOGI(TAG, "Cantidad regada: %.1f ml. | Freq: %f |Contador: %i| tiempo_final:%" PRIu64 "\n", lts_actual,
            freq, contador, tiempo_final);

        flow_frequency = 0;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    gptimer_set_raw_count(gptimer, 0);
    gptimer_stop(gptimer);
    gptimer_disable(gptimer);

    apagar_bomba();
    cerrar_valvula(valve);
    const char *prefijo;
    if(valve == VALVE1)
    {
        prefijo = "S1,";
    }else{
        prefijo = "S2,";
    }
    ultimoRiego(prefijo, lts_actual);
    xSemaphoreGive(semaphoreRiego);
}

*/

