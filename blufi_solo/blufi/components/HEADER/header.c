#include <stdio.h>
#include "header.h"
#include <string.h>
#include <stdint.h>
#include "time.h"
#include "storage.h"
#include "mqtt.h"
#include "ntp.h"

extern config_data configuration; 

/*
@brief 
Almacena las configuraciones de hum min y max
@param str[] event->data
@param *cfg  puntero a la variable donde se almacenará
@param sensor sensor al que corresponden los datos
*/
void recibe_confg_hum(char str[], config_data *cfg, int sensor)
{
    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    char *hum_H = str + posH + 1;
    char *hum_L = str + posL + 1;

    if(sensor == 1)
    {
        cfg->hum_sup_1 = strtol(hum_H, NULL, 10);
        cfg->hum_inf_1 = strtol(hum_L, NULL, 10);
    }else{
        cfg->hum_sup_2 = strtol(hum_H, NULL, 10);
        cfg->hum_inf_2 = strtol(hum_L, NULL, 10);
    }
}


void bytesToHex(const unsigned char* bytes, int size, char* hexString) 
{
    for (int i = 0; i < size; i++) {
        sprintf(hexString + (i * 2), "%02X", bytes[i]);
    }
    hexString[size * 2] = '\0';
}



void ultimoRiego(const char *prefijo, int ml) {

    char *hora = queHoraEs();
    size_t message_size = snprintf(NULL, 0, "%s%s, %d", prefijo, hora, ml) + 1;
    char *message = (char *)malloc(message_size);
    
    if (message != NULL) {
        snprintf(message, message_size, "%s%s, %d", prefijo, hora, ml);
        enviar_mensaje_mqtt(configuration.MAC, message);
        free(message);
    }
}
