typedef struct
{
    int humedad_amb; 
    float temperatura_amb; 
    int humedad_suelo; 
    int intensidad_luz; 

}sensor_data;

/*Estructura para guardar configuración*/
typedef struct
{
    char UUID[18];      // debe almacenar el identificador recibido en custom message
    char MAC[7];        // almacena la mac del esp
    int hum_sup;        // limite superior de humedad
    int hum_inf;        // Limite inferior de humedad     
    int regar;          // Acciona riego manual 
    int control_riego;   // Controla el riego automatico

}config_data;

void recibe_confg_hum(char str[], config_data *cfg);