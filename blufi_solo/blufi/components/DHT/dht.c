#include <stdio.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/types.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

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
			if(intentos < INTENTOS_MAX)
			{
				ESP_LOGI("DHT", "Intento: %i", intentos);
				readDHT();
			}else{
				ESP_LOGI("DHT", "Error en el sensor de temperatura.");
			}
			break;

		case DHT_CHECKSUM_ERROR:
			intentos++;
			if(intentos < INTENTOS_MAX)
			{
				ESP_LOGI("DHT", "Intento: %i", intentos);
				readDHT();
			}else{
				ESP_LOGE(TAG,"Sensor CHECKSUM. Repitiendo medición %i.\n", intentos);
			}
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
		ESP_LOGI("DHT", "Falla 1");
		return NULL; 
	} 

	uSec = getSignalLevel( 85, 1 );
	if( uSec<0 ) {
		ESP_LOGI("DHT", "Falla 2");
		return NULL;
	}

	for( int k = 0; k < 40; k++ ) 
	{

		uSec = getSignalLevel( 56, 0 );
		if( uSec<0 ){
			ESP_LOGI("DHT", "Falla 3");
			return NULL;
		}

		uSec = getSignalLevel( 75, 1 );
		if( uSec<0 ){
			ESP_LOGI("DHT", "Falla 4");
			DHTerrorHandler(DHT_TIMEOUT_ERROR);
			//return NULL;
		}

		if (uSec > 30){
			dhtData[k/8] |= (1 << (7-(k%8))); // 1 << 7, 1 << 6, 1 << 5, ..., 1 << 0
		}

	}

	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF)){
		ESP_LOGI("DHT", "Envio dhtDatos exitoso. Intentos: %i\n", intentos);
		DHTerrorHandler(DHT_OK); 
		return dhtData;
	}else{
		intentos++;
		if(intentos < INTENTOS_MAX){
			ESP_LOGI("DHT", "Fallo envio de datos. Intentos: %i", intentos);
			return readDHT();
		}else{
			ESP_LOGI("DHT", "Fallo envio de datos. Intentos limite: %i", intentos);
			return NULL; 
		}
	}
}


float getTemp(uint8_t *dhtData)
{
    if (dhtData == NULL)
    {
        // Manejo del puntero nulo
        return 0.0;
    }

    // Combina los bytes de temperatura
    int16_t rawTemperature = (dhtData[2] << 8) | dhtData[3];

    // Divide para obtener la temperatura con decimales
    float temperature = rawTemperature / 10.0;

    // Verifica el bit de signo para temperaturas negativas
    if (dhtData[2] & 0x80)
    {
        // Temperatura negativa, multiplica por -1
        temperature *= -1;
    }

    // Redondea a un solo decimal
    temperature = roundf(temperature * 10.0) / 10.0;

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