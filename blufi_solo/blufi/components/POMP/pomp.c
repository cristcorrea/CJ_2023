#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "pomp.h"
#include "header.h"
#include "mqtt.h"


#define TAG             "RIEGO"

volatile int flow_frequency = 0;

const unsigned long TIEMPO_MAX  = 15000000;

gptimer_handle_t gptimer = NULL; 

extern config_data configuration; 
 
void flow_sensor_isr(void* arg)
{
    flow_frequency++;  // Incrementar la frecuencia cuando se produce una interrupción
}


esp_err_t init_irs(void){

    esp_err_t err = ESP_OK; 
    gpio_config_t flow_sensor_config;
    flow_sensor_config.intr_type = GPIO_INTR_POSEDGE;
    flow_sensor_config.mode = GPIO_MODE_INPUT;
    flow_sensor_config.pin_bit_mask = (1ULL << FLOW_SENSOR_PIN);
    flow_sensor_config.pull_up_en = GPIO_PULLDOWN_DISABLE;
    flow_sensor_config.pull_down_en = GPIO_PULLUP_ENABLE;

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
    enable_config.pin_bit_mask = (1ULL << ENABLE_BOM);
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

void encender_bomba()
{
    gpio_set_level(ENABLE_BOM, 1);
    gpio_set_level(BOMBA, 1); 
}

void apagar_bomba()
{
    gpio_set_level(BOMBA, 0);
    gpio_set_level(ENABLE_BOM, 0);  
}

void abrir_valvula(gpio_num_t valvula)
{
    gpio_set_level(valvula, 1);
}

void cerrar_valvula(gpio_num_t valvula)
{
    gpio_set_level(valvula, 0);
}




void timer_config(){

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
}


void regar(float lts_final, gpio_num_t valve){

    float lts_actual = 0.0;
    
    gptimer_enable(gptimer);
    gptimer_start(gptimer);
    uint64_t tiempo_inicial = 0;
    uint64_t tiempo_final = 0; 

    encender_bomba();
    abrir_valvula(valve);

    gptimer_get_raw_count(gptimer, &tiempo_inicial);
    gptimer_get_raw_count(gptimer, &tiempo_final);

    while((lts_actual <= lts_final) && ((tiempo_final - tiempo_inicial) < TIEMPO_MAX)){ 

        ESP_LOGI(TAG, "Cantidad regada: %.1f ml. Flow frequency: %i lts_final: %.1f tiempo_final:%" PRIu64 "\n", lts_actual,
            flow_frequency, lts_final, tiempo_final);
        float freq = flow_frequency / 0.1f;
        if((tiempo_final - tiempo_inicial) >= TIEMPO_MAX/3 && flow_frequency == 0){
            ESP_LOGE("Watering", "No hay agua");
        } 
        float flow_rate = (freq * 1000.0f) / (98.0f * 60.0f);
        float flow_rate_with_err = flow_rate * 0.02f + flow_rate;
        lts_actual += flow_rate_with_err * 0.1f; 
        gptimer_get_raw_count(gptimer, &tiempo_final);
        flow_frequency = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Total riego: %.2f", lts_actual);

    gptimer_set_raw_count(gptimer, 0);
    gptimer_stop(gptimer);
    gptimer_disable(gptimer);

    apagar_bomba();
    cerrar_valvula(valve);

}

