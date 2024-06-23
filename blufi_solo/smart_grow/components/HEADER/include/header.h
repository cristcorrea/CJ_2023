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
    uint8_t first_connection;
    uint8_t semaforoWifiState;   
    uint8_t soilHumidity1;
    uint8_t soilHumidity2; 
    uint16_t intentosReconectar; 

}config_data;

typedef struct
{
    gpio_num_t valvula; 
    int cantidad; 
}mensajeRiego;


/*
    @brief Almacena la configuración del sensor de humedad. 
    @param str Datos recibidos por MQTT (event->data).
    @param cfg Puntero a la variable donde se almacenará. 
    @param sensor Sensor al que corresponden los datos.  
*/
void recibe_confg_hum(char str[], config_data *cfg, int sensor);

// REVISAR BYTESTOHEX PORQUE CREO QUE NO SE UTILIZA 
void bytesToHex(const unsigned char* bytes, int size, char* hexString); 

/*
    @brief Envia datos del ultimo riego a servidor y app.
    @param prefijo Iniciales del sensor de humedad (S1 o S2).  
    @param ml Cantidad de ml que fueron irrigados. 
*/
void ultimoRiego(const char *prefijo, int ml);


/*
    @brief Configuracion del GPIO del led del sensor touch (verde). 
*/
void touchLedConfig(void);

/*
    @brief Apaga led del sensor touch (verde). 
*/
void apagarLedTouch(void);

/*
    @brief Enciende led del sensor touch (verde). 
*/
void encenderLedTouch(void);

/*
    @brief Configuracion del GPIO del led de estado Wifi (azul). 
*/
void wifiLedConfig(void);

/*
    @brief Apaga led del estado Wifi (azul). 
*/
void apagarLedWifi(void);

/*
    @brief Enciende led del estado Wifi (azul). 
*/
void encenderLedWifi(void);

/*
    @brief Realiza un parpadeo del led Touch (verde). 
*/
void parpadeo(void);

/*
    @brief Envia la version del firmware a la app en cada intento de actualizacion OTA. 

*/
void enviarVersion(void);


void enviarAlarma(uint8_t numAlarma); 

void enviarEstadoAutomatico(uint8_t numAlarma, uint8_t estado); 

void revisarTemperatura(float);
