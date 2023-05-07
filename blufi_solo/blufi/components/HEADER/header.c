#include "header.h"
#include <string.h>
#include <stdint.h>


void recibe_confg_hum(char str[], config_data *cfg)
{
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
}