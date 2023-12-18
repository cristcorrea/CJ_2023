#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt.h"

#include "esp_bt_device.h"
#include "header.h"
#include "storage.h"
#include "time.h"
#include "pomp.h"
#include "soil.h"
#include "dht.h"
#include "bh1750.h"
#include "ntp.h"

#define TAG "MQTT"

extern const uint8_t hivemq_certificate_pem_start[]   asm("_binary_hivemq_certificate_pem_start");

extern const uint8_t hivemq_certificate_pem_end[]   asm("_binary_hivemq_certificate_pem_end");


extern config_data configuration;
extern QueueHandle_t riegoQueue; 
extern SemaphoreHandle_t semaphoreFecha;

esp_mqtt_client_handle_t client; 


/*
    La idea es que reciba un parametro para determinar si debe enviar la fecha o no. 
*/
void enviarDatos(char * topic, bool fecha)
{

    int hum_suelo_1 = humidity(SENSOR1);
    int hum_suelo_2 = humidity(SENSOR2);
    int humedad = -1; 
    float temperatura_amb; 
    float lux; 
    uint8_t* datos = readDHT();

    if(datos == NULL){
        temperatura_amb = -1; 
    }else{
        temperatura_amb = getTemp(datos);
    }
        
    lux = bh1750_read();
    int lux_rounded = (int)(lux + 0.5);

    if(datos != NULL)
    {
        humedad = datos[0];
    }

    size_t message_size; 
    char *hora;

    if(fecha)
    {
        hora = queHoraEs();
        message_size = snprintf(NULL, 0, "%i,%i,%i,%.1f,%i,%s",
            hum_suelo_1, hum_suelo_2,  humedad, temperatura_amb, lux_rounded, hora) + 1;
    }else{
        message_size = snprintf(NULL, 0, "%i,%i,%i,%.1f,%i",
            hum_suelo_1, hum_suelo_2,  humedad, temperatura_amb, lux_rounded) + 1;
    }

    char *message = (char *)malloc(message_size);

    if(message != NULL){              
        snprintf(message, message_size , "%i,%i,%i,%.1f,%i",
                hum_suelo_1, hum_suelo_2, humedad, temperatura_amb, lux_rounded);           
        enviar_mensaje_mqtt(topic, message);
        free(message);
        message = NULL;
    }
    
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = NULL;
    event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:

        char * topic_sus = (char *)malloc(10); 
        memset(topic_sus, 0, 10);
        memcpy(topic_sus, configuration.cardId, sizeof(char) * 8);
        strcat(topic_sus, "B");
        suscribirse(topic_sus);
        ESP_LOGI(TAG, "Suscrito al topic: %s\n", topic_sus);

        topic_sus[8] = 'C';
        ESP_LOGI("DEBUG", "Topic sus: %s", topic_sus);

        memcpy(configuration.cardIdC, topic_sus, sizeof(char) * 9);

        free(topic_sus);
        topic_sus = NULL; 
        xSemaphoreGive(semaphoreFecha);
        encenderLedWifi();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        apagarLedWifi();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:

        ESP_LOGI("DEBUG MQTT EVENT DATA", "Datos recibidos: %s", event->data);
        char clave1 = event->data[0]; 

        switch (clave1)
        {
        case 'S':
            enviarDatos(configuration.cardId, false);
            break;
        
        case 'R':                       // Riego manual
            
            int data_len = event->data_len;

            char* mililitros = (char*)malloc(data_len - 1);
            strncpy(mililitros, event->data + 2, data_len);
            mililitros[data_len - 2] = '\0';

            int ml = 0; 
            ml = strtol(mililitros, NULL, 10);
            ESP_LOGI("DEBUG MQTT", "mililitros: %s | ml: %i | data_len: %i", mililitros, ml, data_len);
            mensajeRiego mensaje; 
            if(event->data[1] == '1')
            {
                mensaje.valvula = VALVE1; 
            }else{
                mensaje.valvula = VALVE2;
            }
            mensaje.cantidad = ml;
            event->data = NULL; 
            xQueueSend(riegoQueue, &mensaje, portMAX_DELAY);
            break;

        case 'A':                      // Automatico 
            int err; 
            if(event->data[2] == '0') //Apagado de automático
            {   
                if (event->data[1] == '1')
                {
                    configuration.control_riego_1 = 0;
                    err = NVS_write_i8("control_riego_1", configuration.control_riego_1); 
                    //vTaskSuspend(xHandle_riego_auto_1);
                }else{
                    configuration.control_riego_2 = 0; 
                    err = NVS_write_i8("control_riego_2", configuration.control_riego_2);
                    //vTaskSuspend(xHandle_riego_auto_2);
                }
                if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}else{
                    ESP_LOGI(TAG, "control riego almacenado\n");}
                
            }else{                      // Encendido de automático
                if(event->data[1] == '1')
                {
                    configuration.control_riego_1 = 1;
                    recibe_confg_hum(event->data, &configuration, 1);
                    err = NVS_write_i8("hum_sup_1", configuration.hum_sup_1);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup_1\n");}
                    err = NVS_write_i8("hum_inf_1", configuration.hum_inf_1);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf_1\n");}else{
                    ESP_LOGI(TAG, "Datos de riego 1 almacenados.L: %i - H: %i\n", configuration.hum_inf_1, configuration.hum_sup_1);}
                    //vTaskResume(xHandle_riego_auto_1); 
                }else{
                    configuration.control_riego_2 = 1; 
                    recibe_confg_hum(event->data, &configuration, 2);
                    err = NVS_write_i8("hum_sup_2", configuration.hum_sup_2);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup_2\n");}
                    err = NVS_write_i8("hum_inf_2", configuration.hum_inf_2);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf_2\n");}else{
                    ESP_LOGI(TAG, "Datos de riego 2 almacenados\n");}
                    //vTaskResume(xHandle_riego_auto_2);
                }
                if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}else{
                    ESP_LOGI(TAG, "control_riego almacenado\n");}
            }
            break; 

        case 'P':                       // Purga
            if(event->data[1] == '1')
            {
                regar(200, 1);
            }else{
                regar(200, 2);
            }
            break;


        default:
            break;
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_start()
{

    esp_mqtt_client_config_t mqtt_config = {
        .broker = {
            .address.uri = "mqtts://036430ed518042ee99dd7a8c48c578a7.s2.eu.hivemq.cloud:8883",
            .verification.certificate = (const char *)hivemq_certificate_pem_start, 
        },
        .credentials = {
            .username = "cjindoors",
        },
        .credentials.authentication = {
            .password = "433603asd",
        },

    };
    client = esp_mqtt_client_init(&mqtt_config); 
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

}

void enviar_mensaje_mqtt(char * topic, char * mensaje)
{
    int mensaje_id = esp_mqtt_client_publish(client, topic, mensaje, 0, 1, 0);
    ESP_LOGI(TAG, "Mensaje enviado, ID: %d al topic: %s", mensaje_id, topic);
}

void suscribirse(char * topic)
{
    esp_mqtt_client_subscribe(client, topic, 1);
}