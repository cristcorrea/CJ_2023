#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#include "soil.h"

#define TAG "Soil"


// Create an ADC Unit Handle 
adc_oneshot_unit_handle_t adc1_handle; 


static void countingSort(int  muestras[])
{
    int contador[4096];
    for(int i = 0; i < 4096; i++)
    {
        contador[i] = 0;
    }
    for(int j = 0; j < 24; j++)
    {
        contador[muestras[j]]++;
    }
    int l = 0;
    for(int k = 0; k < 4096; k++)
    {
        while(contador[k] != 0) 
        {
            muestras[l] = k;
            contador[k]--;
            l++;
        }
    }
}

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

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));
}

void humidity(void)
{
    int adc_reading = 0; 
    for(int i = 0; i < 100; i++)
    {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &adc_reading);
        SOIL_DATA.humidity += adc_reading; 
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    SOIL_DATA.humidity /= 100;
}

void salt(void)
{
    int muestras[120];
    int valor = 0;  
    int res = 0; 
    for(int i = 0; i < 120; i++)
    {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &valor);
        muestras[i] = valor; 
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    countingSort(muestras);
    for(int j = 0; j < 119; j++)
    {
        res += muestras[j];
    }
    res /= 118; 
    SOIL_DATA.salinity = res; 
}



