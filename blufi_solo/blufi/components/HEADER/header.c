#include <stdio.h>
#include "header.h"
#include <string.h>
#include <stdint.h>
#include "time.h"
#include "storage.h"
#include "mqtt.h"
#include "ntp.h"

extern config_data configuration; 

void recibe_confg_hum(char str[], config_data *cfg)
{

    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    char *hum_H = str + posH + 1;
    char *hum_L = str + posL + 1;
    cfg->hum_sup = strtol(hum_H, NULL, 10);
    cfg->hum_inf = strtol(hum_L, NULL, 10);
}


void bytesToHex(const unsigned char* bytes, int size, char* hexString) 
{
    for (int i = 0; i < size; i++) {
        sprintf(hexString + (i * 2), "%02X", bytes[i]);
    }
    hexString[size * 2] = '\0';
}

void ultimoRiego(){

    char * hora = queHoraEs();
    size_t message_size = snprintf(NULL, 0, "%s", hora) + 1;
    char *message = (char *)malloc(message_size);
    if(message != NULL){              
        strcpy(message, hora);           
        enviar_mensaje_mqtt(configuration.MAC, message);
        free(message);
    }
}