#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "pomp.h"
#include "header.h"


#define BOMBA           GPIO_NUM_18
#define FLOW_SENSOR_PIN GPIO_NUM_2
#define TIEMPO_MAX      10000000    

volatile int flow_frequency = 0;

gptimer_handle_t gptimer = NULL; 

void flow_sensor_isr(void* arg)
{
    flow_frequency++;  // Incrementar la frecuencia cuando se produce una interrupción
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

    gpio_config_t flow_sensor_config = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << FLOW_SENSOR_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    gpio_config(&flow_sensor_config);
    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    gpio_isr_handler_add(FLOW_SENSOR_PIN, flow_sensor_isr, NULL);

}

void encender_bomba()
{
    gpio_set_level(BOMBA, 1); 
}

void apagar_bomba()
{
    gpio_set_level(BOMBA, 0);  
}


void timer_config(){

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
}


void regar(float lts_final){

    float lts_actual = 0.0;
    gptimer_enable(gptimer);
    gptimer_start(gptimer);
    uint64_t count = 0;
    encender_bomba();

    while(lts_actual < lts_final && count < TIEMPO_MAX){

        float flow_rate = flow_frequency / 98.0;
        float error = 0.02 * flow_rate;
        float flow_rate_with_error = flow_rate + error; 
        lts_actual += flow_rate_with_error /60.0;
        gptimer_get_raw_count(gptimer, &count);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    gptimer_stop(gptimer);
    gptimer_disable(gptimer);
    apagar_bomba();

}
