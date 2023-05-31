/*Estructura para guardar configuración*/
typedef struct
{

    char MAC[13];        // almacena la mac del esp
    int hum_sup;        // limite superior de humedad
    int hum_inf;        // Limite inferior de humedad                
    int control_riego;   // Controla el riego automatico
    char * time_zone; 

}config_data;

void recibe_confg_hum(char str[], config_data *cfg);
void bytesToHex(const unsigned char* bytes, int size, char* hexString); 
void ultimoRiego(void);