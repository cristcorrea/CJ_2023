#include <stdio.h>
#include "header.h"
#include <string.h>
#include <stdint.h>
#include "time.h"
#include "storage.h"


extern sensor_data mediciones;


void recibe_confg_hum(char str[], config_data *cfg)
{
    /*
    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    int posF = strcspn(str, "F");
    char hum_H[posL-posH+1];
    char hum_L[posF-posL+1];
    memset(hum_H, 0, posL-posH+1);
    memcpy(hum_H, str+posH+1, posL-posH);
    memset(hum_L, 0, posF-posL+1);
    memcpy(hum_L, str+posL+1, posF-posL);
    cfg->hum_sup = atoi(hum_H);
    cfg->hum_inf = atoi(hum_L);
    */
    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    int posF = strcspn(str, "F");
    char *hum_H = str + posH + 1;
    char *hum_L = str + posL + 1;
    cfg->hum_sup = strtol(hum_H, NULL, 10);
    cfg->hum_inf = strtol(hum_L, NULL, 10);
}

void ultimo_riego()
{
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo); 
    strftime(mediciones.ultimo_riego, sizeof(mediciones.ultimo_riego), "%c", &timeinfo);
    NVS_write("ultimo_riego", mediciones.ultimo_riego);

}

void bytesToHex(unsigned char* bytes, int size, char* hexString) 
{
    for (int i = 0; i < size; i++) {
        sprintf(hexString + (i * 2), "%02X", bytes[i]);
    }
    hexString[size * 2] = '\0';
}