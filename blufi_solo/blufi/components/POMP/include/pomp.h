#include "driver/gpio.h"

void riego_config(void);
void regar(float lts_final);
void timer_config(void);
void encender_bomba(void);
void apagar_bomba(void);
void abrir_valvula(gpio_num_t valvula);
void cerrar_valvula(gpio_num_t valvula);
//void regar(float lts_final, gpio_num_t valve);
esp_err_t init_irs(void); 