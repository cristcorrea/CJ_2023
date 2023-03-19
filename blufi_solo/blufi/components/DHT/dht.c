#include <stdio.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/types.h>
#include <esp_log.h>
#include "driver/gpio.h"

#include "dht.h"

#define MAXdhtData 5	// to complete 40 = 5*8 Bits

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

#define gpio_num 4
  

static const char* TAG = "DHT_start";


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
			//ESP_LOGI( TAG, "Sensor Timeout\n" );
			printf( "Sensor Timeout\n" );
			break;

		case DHT_CHECKSUM_ERROR:
			//ESP_LOGI( TAG, "CheckSum error\n" );
			printf( "CheckSum error\n" );
			break;

		case DHT_OK:
			break;

		default :
			//ESP_LOGI( TAG, "Unknown error\n" );
			printf( "Unknown error\n" );
	}
}


int readDHT()
{
    int uSec = 0;

    uint8_t dhtData[MAXdhtData];

	for (int k = 0; k < MAXdhtData; k++)
	{
		dhtData[k] = 0;
	}
	// == Send start signal to DHT sensor ===========

	gpio_set_direction( gpio_num, GPIO_MODE_OUTPUT );

	// pull down for 20 ms for a smooth and nice wake up
	gpio_set_level( gpio_num, 0 );
	esp_rom_delay_us( 20000 );

	// pull up for 25 us for a gentile asking for data
	gpio_set_level( gpio_num, 1 );
	esp_rom_delay_us( 25 );

	gpio_set_direction( gpio_num, GPIO_MODE_INPUT );		

	// == DHT will keep the line low for 80 us and then high for 80us ====

	uSec = getSignalLevel( 85, 0 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 )
	{
		ESP_LOGE(TAG, "DHT_TIMEOUT_ERROR 1");
		return DHT_TIMEOUT_ERROR;
	} 

	// -- 80us up ------------------------

	uSec = getSignalLevel( 85, 1 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 ) 
	{
		ESP_LOGE(TAG, "DHT_TIMEOUT_ERROR 2");
		return DHT_TIMEOUT_ERROR;
	}
	// == No errors, read the 40 data bits ================

	for( int k = 0; k < 40; k++ ) 
	{

		// -- starts new data transmission with >50us low signal

		uSec = getSignalLevel( 56, 0 );
		if( uSec<0 )
		{
			ESP_LOGE(TAG, "DHT_TIMEOUT_ERROR 3");
			return DHT_TIMEOUT_ERROR;
		}
		// -- check to see if after >70us rx data is a 0 or a 1

		uSec = getSignalLevel( 75, 1 );
		if( uSec<0 ) 
		{
			ESP_LOGE(TAG, "DHT_TIMEOUT_ERROR 4");
			return DHT_TIMEOUT_ERROR;
		}

		// since all dhtData array where set to 0 at the start,
		// only look for "1" (>28us us)

		if (uSec > 30)
		{
			dhtData[k/8] |= (1 << (7-(k%8))); // 1 << 7, 1 << 6, 1 << 5, ..., 1 << 0
		}

	}

	DHT_DATA.humidity = dhtData[0];					

	DHT_DATA.temperature = dhtData[2];
	DHT_DATA.temperature *= 10;		
	DHT_DATA.temperature += dhtData[3]; 
	DHT_DATA.temperature /= 10; 
	

	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
			DHT_DATA.temperature *= -1;


	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF))
		return DHT_OK;

	else
		return DHT_CHECKSUM_ERROR;
}