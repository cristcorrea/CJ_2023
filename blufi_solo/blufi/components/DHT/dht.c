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
	uint8_t dhtData[MAXdhtData];
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
		intentos = 0; 
		return dhtData;
	}else{
		if(intentos < INTENTOS_MAX){
			return readDHT();
		}else{
			return NULL; 
		}
	}
}

float getTemp(uint8_t* datos){
	
	float result = datos[2] * 10 + datos[3];
	result /= 10;

	if(datos[2] & 0x80){
		result *= -1; 
	}

	return result; 
}

/*

	mediciones.humedad_amb = dhtData[0];			

	mediciones.temperatura_amb = dhtData[2];
	mediciones.temperatura_amb *= 10;
	mediciones.temperatura_amb += dhtData[3];
	mediciones.temperatura_amb /= 10; 

	
	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
			mediciones.temperatura_amb *= -1;
*/