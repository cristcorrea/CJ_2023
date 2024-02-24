
void DHTerrorHandler(int response); 

uint8_t *readDHT();

/**
 * @brief Obtiene la duración de un nivel de señal específico en el pin GPIO.
 *
 * Esta función mide la duración de un nivel de señal en el pin GPIO especificado.
 * El bucle espera hasta que el nivel de señal en el pin coincide con el estado
 * deseado o hasta que se alcanza el tiempo de espera especificado.
 *
 * @param usTimeOut Tiempo máximo de espera en microsegundos.
 * @param state Estado de la señal a medir (true para alto, false para bajo).
 * @return La duración del nivel de señal en microsegundos, o -1 si se alcanza el tiempo de espera.
 */
int getSignalLevel( int usTimeOut, bool state ); 

float getTemp(uint8_t* datos); 

int getHumidity(uint8_t *datos); 
