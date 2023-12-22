
void DHTerrorHandler(int response); 

uint8_t *readDHT();

int getSignalLevel( int usTimeOut, bool state ); 

float getTemp(uint8_t* datos); 

int getHumidity(uint8_t *datos); 
