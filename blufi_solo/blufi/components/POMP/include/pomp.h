#include "driver/gpio.h"

#define BOMBA           GPIO_NUM_5      //ok
#define ENABLE_BOM      GPIO_NUM_14     //ok
#define VALVE1          GPIO_NUM_18     //ok
#define VALVE2          GPIO_NUM_19     //ok
#define FLOW_SENSOR_PIN GPIO_NUM_27     //ok

void riego_config(void);
void timer_config(void);
void encender_bomba(void);
void apagar_bomba(void);
void abrir_valvula(gpio_num_t valvula);
void cerrar_valvula(gpio_num_t valvula);
void regar(int lts_final, gpio_num_t valve);
esp_err_t init_irs(void); 