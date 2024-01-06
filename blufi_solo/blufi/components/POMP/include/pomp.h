#include "driver/gpio.h"

#define BOMBA           GPIO_NUM_5      
#define ENABLE_DRV      GPIO_NUM_14     
#define VALVE1          GPIO_NUM_18     
#define VALVE2          GPIO_NUM_19     
#define FLOW_SENSOR_PIN GPIO_NUM_27     
#define nFAULT_PIN      GPIO_NUM_26  


void riego_config(void);
void encender_bomba(void);
void apagar_bomba(void);
void abrir_valvula(gpio_num_t valvula);
void cerrar_valvula(gpio_num_t valvula);
void regar(int lts_final, gpio_num_t valve);
esp_err_t init_irs(void); 
esp_err_t init_nFault(void);
void getUpDriver(void);
void sleepDriver(void);
void stopRiego(void);
void set_pwm_duty(void);