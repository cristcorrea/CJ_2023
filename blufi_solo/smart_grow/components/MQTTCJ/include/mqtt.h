void mqtt_start(); 

int enviar_mensaje_mqtt(char * topic, char * mensaje);

void suscribirse(char * topic); 

/*
    @brief Envia los datos de las mediciones a un topic.
    @param topic Topic a enviar los datos.
    @param fecha Determina si agregar la fecha o no al conjunto de datos.  
*/
void enviarDatos(char * topic, bool fecha);