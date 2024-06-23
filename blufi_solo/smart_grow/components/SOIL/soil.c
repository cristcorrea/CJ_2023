#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "soil.h"

#include "soil.h"
#include "header.h"

#define TAG "Soil"
#define WINDOW_SIZE 5

#define GPIO_INPUT_PIN_SEL  ((1ULL << GPIO_NUM_17) | (1ULL << GPIO_NUM_23) | (1ULL << GPIO_NUM_25) | (1ULL << GPIO_NUM_26))
#define ESP_INTR_FLAG_DEFAULT 0


// Create an ADC Unit Handle 
adc_oneshot_unit_handle_t adc1_handle; 
extern SemaphoreHandle_t    semaphoreSensorSuelo; 

// Callback para el servicio de interrupciones
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    int pin_num = (int)arg;
    uint8_t alarma = 0; 
    if(pin_num == 17 || pin_num == 25 || pin_num == 26)
    {
        alarma = 4; //falla riego 
    }
    else if(pin_num == 23)
    {
        alarma = 2; // falla sensores humedad suelo
    }
    if(alarma != 0)
    {
        enviarAlarma(alarma); 
    }
}


void soilConfig(void)
{

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // Interrupción en ambos flancos
    io_conf.mode = GPIO_MODE_INPUT;         // Modo de entrada
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL; // Seleccionar pines
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;  // Desactivar pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Desactivar pull-down
    gpio_config(&io_conf);

    // Instalación del servicio de interrupciones
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // Añadir manejador de interrupciones para cada pin
    gpio_isr_handler_add(GPIO_NUM_17, gpio_isr_handler, (void*)GPIO_NUM_17); // Short circuit flow sensor
    gpio_isr_handler_add(GPIO_NUM_23, gpio_isr_handler, (void*)GPIO_NUM_23); // Short circuit humidity sensors 
    gpio_isr_handler_add(GPIO_NUM_25, gpio_isr_handler, (void*)GPIO_NUM_25); // Valves fault indicator
    gpio_isr_handler_add(GPIO_NUM_26, gpio_isr_handler, (void*)GPIO_NUM_26); // Pomp fault indicator
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));


    // Configure channels 

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_6, 
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SENSOR1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SENSOR2, &config));

    gpio_config_t s1_state;
    s1_state.pin_bit_mask = (1ULL << S1_STATE);
    s1_state.mode = GPIO_MODE_INPUT;
    s1_state.pull_up_en = GPIO_PULLUP_DISABLE;
    s1_state.pull_down_en = GPIO_PULLDOWN_ENABLE;
    s1_state.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&s1_state);

    gpio_config_t s2_state;
    s2_state.pin_bit_mask = (1ULL << S2_STATE);
    s2_state.mode = GPIO_MODE_INPUT;
    s2_state.pull_up_en = GPIO_PULLUP_DISABLE;
    s2_state.pull_down_en = GPIO_PULLDOWN_ENABLE;
    s2_state.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&s2_state);

    gpio_config_t sensors_ena;
    sensors_ena.pin_bit_mask = (1ULL << SENSORS_ENABLE);
    sensors_ena.mode = GPIO_MODE_OUTPUT;
    sensors_ena.pull_up_en = GPIO_PULLUP_DISABLE;
    sensors_ena.pull_down_en = GPIO_PULLDOWN_ENABLE;
    sensors_ena.intr_type = GPIO_INTR_DISABLE; 
    gpio_config(&sensors_ena);
}


int read_humidity(adc_channel_t channel)
{
    uint16_t array_size = 500; 
    int values[array_size];
    uint8_t samples[4096] = {0};  // Inicializa el histograma a 0

    // Lee valores del sensor ADC y acumúlalos
    for (int i = 0; i < array_size; i++)
    {
        adc_oneshot_read(adc1_handle, channel, &values[i]);
        vTaskDelay(1);
    }

    // Llena el histograma
    for (int j = 0; j < array_size; j++)
    {
        samples[values[j]]++;
    }

    // Encuentra la moda
    int max_count = 0;
    int mode_value = 0;

    // Recorre el histograma para encontrar la moda
    for (int k = 0; k < 4096; k++)
    {
        if (samples[k] > max_count)
        {
            max_count = samples[k];
            mode_value = k;
        }
    }

    mode_value = (4095 - mode_value)/6.17; 

    if(mode_value > 100)
    {
        mode_value = 100;
    }
    if(mode_value < 0)
    {
        mode_value = 0; 
    }
    ESP_LOGI("Lee humedad", " %i ", mode_value); 

    return mode_value;
}


int sensorConectado(adc_channel_t sensor)
{   
    return gpio_get_level(sensor); 
 
}

int  humidity(adc_channel_t sensor)
{
    int value = 0; 

    if(sensor == SENSOR1)
    {   
        if(sensorConectado(S1_STATE))
        {
            value = read_humidity(SENSOR1);
        }
    }else{
        if(sensorConectado(S2_STATE))
        {
            value = read_humidity(SENSOR2);
        }
    }

    return value; 
}

void habilitarSensorSuelo(uint16_t time)
{   
    xSemaphoreTake(semaphoreSensorSuelo, portMAX_DELAY); 
    ESP_LOGI("SOIL", "Toma sensores ");
    gpio_set_level(SENSORS_ENABLE, 0);
    vTaskDelay(pdMS_TO_TICKS(time));
}
    
void desHabilitarSensorSuelo(void)
{
    gpio_set_level(SENSORS_ENABLE, 1);
    xSemaphoreGive(semaphoreSensorSuelo);
    ESP_LOGI("SOIL", "Libera sensores ");
}
