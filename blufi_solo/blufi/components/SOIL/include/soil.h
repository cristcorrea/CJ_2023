#include "hal/adc_types.h"

#define SENSOR1 ADC_CHANNEL_4
#define SENSOR2 ADC_CHANNEL_5

void soilConfig(void); 

int humidity(adc_channel_t sensor);
int read_humidity(adc_channel_t sensor);
