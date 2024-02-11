#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "soil.h"

#include "soil.h"
#include "header.h"

#define TAG "Soil"


// Create an ADC Unit Handle 
adc_oneshot_unit_handle_t adc1_handle; 


void soilConfig(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));


    // Configure channels 

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11, 
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
}

int adc_read(adc_channel_t channel)
{
    int adc_reading = 0; 
    int result = 0; 
 
    for(int i = 0; i < 500; i++)
    {
        adc_oneshot_read(adc1_handle, channel, &adc_reading);
        result += adc_reading; 
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    result /= 500;

    return result;
}


int read_humidity(adc_channel_t sensor)
{
    int result = adc_read(sensor); 
    
    result = ((result-2560)/9) * (-1);
    
    if(result > 100)
    {
        result = 100;
    }
    if(result < 0)
    {
        result = 0; 
    }
    
    return result;
}

int sensorConectado(adc_channel_t sensor)
{
    return gpio_get_level(sensor);

}

int  humidity(adc_channel_t sensor)
{
    if(sensor == SENSOR1)
    {
        if(gpio_get_level(S1_STATE))
        {
            return read_humidity(SENSOR1);
        }else{
            ESP_LOGI("Soil Sensor", "Sensor 1 no conectado");
            return 0; 
        }
    }else{
        if(gpio_get_level(S2_STATE))
        {
            return read_humidity(SENSOR2);
        }else{
            ESP_LOGI("Soil Sensor", "Sensor 2 no conectado");
            return 0; 
        }
    }
}
    

