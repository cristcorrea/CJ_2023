#include "hal/adc_types.h"

#define SENSOR1 ADC_CHANNEL_4
#define SENSOR2 ADC_CHANNEL_5

void soilConfig(void); 

int humidity(adc_channel_t sensor);
#define S1_STATE GPIO_NUM_36
#define S2_STATE GPIO_NUM_39 

int sensorConectado(adc_channel_t sensor);
int read_humidity(adc_channel_t sensor);
int adc_read(adc_channel_t channel);

