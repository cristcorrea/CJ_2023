#include "driver/gpio.h"

/*Estructura para guardar configuración*/
typedef struct
{

    int hum_sup_1;             // limite superior de humedad
    int hum_inf_1;
    int hum_sup_2;                               
    int hum_inf_2;
    int control_riego_1;       // Controla el riego automatico del sensor 1
    int control_riego_2;       // Controla el riego automatico del sensor 2
    char * MAC;              // almacena la mac del esp
    char * time_zone; 

}config_data;

typedef struct
{
    gpio_num_t valvula; 
    int cantidad; 
}mensajeRiego;

void recibe_confg_hum(char str[], config_data *cfg, int sensor);
void bytesToHex(const unsigned char* bytes, int size, char* hexString); 
void ultimoRiego(const char *prefijo, int ml);