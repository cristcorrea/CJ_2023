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
    char * cardId;              // topic tiempo real
    char cardIdC[10];             // topic para enviar a la nube
    int time_zone; 

}config_data;

typedef struct
{
    gpio_num_t valvula; 
    int cantidad; 
}mensajeRiego;

void recibe_confg_hum(char str[], config_data *cfg, int sensor);
void bytesToHex(const unsigned char* bytes, int size, char* hexString); 
void ultimoRiego(const char *prefijo, int ml);

void touchLedConfig(void);
void apagarLedTouch(void);
void encenderLedTouch(void);

void wifiLedConfig(void);
void apagarLedWifi(void);
void encenderLedWifi(void);
