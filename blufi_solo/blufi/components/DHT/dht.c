#include <stdio.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/types.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dht.h"
#include "header.h"

#define MAXdhtData 5	// to complete 40 = 5*8 Bits

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

#define gpio_num 16
#define INTENTOS_MAX 10

static const char* TAG = "DHT_start";

int intentos = 0;


int getSignalLevel( int usTimeOut, bool state )
{

	int uSec = 0;
	while( gpio_get_level(gpio_num)==state ) {

		if( uSec > usTimeOut )
			return -1;

		++uSec;
		esp_rom_delay_us(1);//ets_delay_us(1);		// uSec delay
	}

	return uSec;
}

void DHTerrorHandler(int response)
{
	switch(response) {

		case DHT_TIMEOUT_ERROR :
			intentos++;
			ESP_LOGE(TAG,"Sensor Timeout. Repitiendo medición %i.\n", intentos);
			break;

		case DHT_CHECKSUM_ERROR:
			ESP_LOGE(TAG,"Sensor CHECKSUM. Repitiendo medición %i.\n", intentos);
			break;

		case DHT_OK:
			intentos = 0; 
			break;

		default :
			ESP_LOGE(TAG,"Sensor Unknown. Repitiendo medición %i.\n", intentos);
	}
}


uint8_t* readDHT()
{
	int uSec = 0;
	uint8_t * dhtData = malloc(5);
	for (int k = 0; k < MAXdhtData; k++)
	{
		dhtData[k] = 0;
	}
	gpio_set_direction( gpio_num, GPIO_MODE_OUTPUT );
	gpio_set_level(gpio_num, 0);
	esp_rom_delay_us( 20000 );
	gpio_set_level( gpio_num, 1 );
	esp_rom_delay_us( 25 );
	gpio_set_direction( gpio_num, GPIO_MODE_INPUT );		
	uSec = getSignalLevel( 85, 0 );

	if( uSec<0 ){
		return NULL; 
	} 

	uSec = getSignalLevel( 85, 1 );
	if( uSec<0 ) {
		return NULL;
	}

	for( int k = 0; k < 40; k++ ) 
	{

		uSec = getSignalLevel( 56, 0 );
		if( uSec<0 ){
			return NULL;
		}

		uSec = getSignalLevel( 75, 1 );
		if( uSec<0 ){
			return NULL;
		}

		if (uSec > 30){
			dhtData[k/8] |= (1 << (7-(k%8))); // 1 << 7, 1 << 6, 1 << 5, ..., 1 << 0
		}

	}

	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF)){
		ESP_LOGI(TAG, "Envio dhtDatos exitoso. Intentos: %i\n", intentos);
		intentos = 0; 
		return dhtData;
	}else{
		intentos++;
		if(intentos < INTENTOS_MAX){
			return readDHT();
		}else{
			return NULL; 
		}
	}
}

/*
float getTemp(uint8_t* datos){
	
    if (datos == NULL) {
        // Manejo del puntero nulo
        return 0.0;
    }

    if (datos[2] > 127) {
        // Temperatura negativa
        int8_t temp = -(~datos[2] + 1);
        return temp - (datos[3] / 10.0);
    } else {
        // Temperatura positiva
        return datos[2] + (datos[3] / 10.0);
    }
}
*/

float getTemp(uint8_t *dhtData)
{
    if (dhtData == NULL)
    {
        // Manejo del puntero nulo
        return 0.0;
    }

    // Combina los bytes de temperatura
    int16_t temperature = dhtData[2] & 0x7F; // Elimina el bit de signo

    // Desplaza 8 bits hacia la izquierda y agrega los bits de temperatura
    temperature *= 0x100; // equivalente a << 8 en términos de bits
    temperature += dhtData[3];

    // Divide para obtener la temperatura con decimales
    temperature /= 10.0;

    // Verifica el bit de signo para temperaturas negativas
    if (dhtData[2] & 0x80)
    {
        // Temperatura negativa, multiplica por -1
        temperature *= -1;
    }

    return temperature;
}


int getHumidity(uint8_t *datos)
{
    if (datos == NULL)
    {
        // Manejo del puntero nulo
        return 0;
    }

    // Combina los bytes de humedad
    uint16_t combinedHumidity = datos[0] * 0x100 + datos[1];

    // Divide para obtener la humedad con decimales
    return combinedHumidity / 10.0;
}