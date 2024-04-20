#include "hal/adc_types.h"

#define SENSOR1 ADC_CHANNEL_4
#define SENSOR2 ADC_CHANNEL_5

void soilConfig(void); 

int humidity(adc_channel_t sensor);
#define S1_STATE GPIO_NUM_36
#define S2_STATE GPIO_NUM_39
#define SENSORS_ENABLE  GPIO_NUM_15    

/*
    @brief Verifica si un sensor de humedad de suelo se encuentra conectado.
    @param sensor Sensor a verificar - S1_STATE/S2_STATE. 
*/
int sensorConectado(adc_channel_t sensor);


/*
    @brief Realiza la lectura de la humedad de un sensor de suelo.
    @param sensor Sensor a medir - SENSOR1/SENSOR2.
*/
int read_humidity(adc_channel_t sensor);

int adc_read(adc_channel_t channel);

/*
    @brief Habilita la alimentación de los sensores de humedad de suelo y espera 250ms. 
*/
void habilitarSensorSuelo(uint16_t time);

/*
    @brief Deshabilita la alimentación de los sensores de humedad de suelo.
*/
void desHabilitarSensorSuelo(void);

