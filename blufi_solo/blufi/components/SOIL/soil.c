#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"


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
}

int  humidity(adc_channel_t sensor)
{
    int adc_reading = 0; 
    int result = 0; 
 
    for(int i = 0; i < 100; i++)
    {
        adc_oneshot_read(adc1_handle, sensor, &adc_reading);
        result += adc_reading; 
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    result /= 100;

    result = -0.0526 * result + 184;
    
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



