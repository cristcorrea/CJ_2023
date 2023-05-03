#include "header.h"
#include <string.h>


void read_config(char str[], config_data *cfg)
{
    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    int posR = strcspn(str, "R");
    char hum_H[posL-posH+1];
    char hum_L[posR-posL+1];
    char riego[2];
    memset(hum_H, 0, posL-posH+1);
    memcpy(hum_H, str+posH+1, posL-posH);
    memset(hum_L, 0, posR-posL+1);
    memcpy(hum_L, str+posL+1, posR-posL);
    memset(riego, 0, 2);
    memcpy(riego, str+posR+1, 1);
    cfg->hum_sup = atoi(hum_H);
    cfg->hum_inf = atoi(hum_L);
    cfg->regar = atoi(riego);
}