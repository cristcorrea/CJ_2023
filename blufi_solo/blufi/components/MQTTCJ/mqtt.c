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


#define TAG "MQTT"

#define MOSQUITTO_URI "mqtt://207.46.13.212:1883"
#define MOSQUITTO_ID "esp32_client"

extern const uint8_t hivemq_certificate_pem_start[]   asm("_binary_hivemq_certificate_pem_start");

extern const uint8_t hivemq_certificate_pem_end[]   asm("_binary_hivemq_certificate_pem_end");


extern config_data configuration;
extern TaskHandle_t xHandle;
extern SemaphoreHandle_t semaphoreSensorConfig;
extern bool wifi_status; 

esp_mqtt_client_handle_t client; 

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:

        char * topic_sus = (char *)malloc(13); 
        memset(topic_sus, 0, 13);
        memcpy(topic_sus, configuration.MAC, sizeof(char) * 12);
        strcat(topic_sus, "R");
        suscribirse(topic_sus);
        ESP_LOGI(TAG, "Suscrito al topic: %s\n", topic_sus);
        free(topic_sus);
        topic_sus = NULL; 
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        xSemaphoreGive(semaphoreSensorConfig);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:

        if(event->data[0] == 'C'){   
 
            int humedad_suelo = humidity(SENSOR1);
            float temperatura_amb; 
            float lux; 
            uint8_t* datos = readDHT();
            if(datos != NULL){

                temperatura_amb = getTemp(datos);
                lux = bh1750_read();
                int lux_rounded = (int)(lux + 0.5);
                size_t message_size = snprintf(NULL, 0, "%iH%iT%.1fL%i",
                    humedad_suelo, datos[0], temperatura_amb, lux_rounded) + 1;

                char *message = (char *)malloc(message_size);
                if(message != NULL){              
                    snprintf(message, message_size , "%iH%iT%.1fL%i",
                            humedad_suelo, datos[0], temperatura_amb, lux_rounded);           
                    enviar_mensaje_mqtt(configuration.MAC, message);
                    free(message);
                }
            
            }

        }else{

            if(memcmp(configuration.MAC, event->data, sizeof(char)*12) == 0)  
            {

                char letra = event->data[12];
                int err; 
                
                switch (letra)
                {
                case 'R':
                    // Enciende el riego manual. 
                    char sensor = event->data[13];

                    if(sensor == '1')
                    {
                        regar(150, VALVE1);
                        const char *prefijo = "S1";
                        ultimoRiego(prefijo);
                    }
                    else
                    {
                        regar(150, VALVE2);
                        const char *prefijo = "S2";
                        ultimoRiego(prefijo);
                    }
                    break;
                
                case 'A':
                    /*
                    Activa/Desactiva el riego automático. Luego de la letra "A" debe venir un 1 o un 2, 
                    depende del sensor que se quiera poner en automático. 
                    */
                    if(event->data[13] == '1')
                    {   
                        if(configuration.control_riego_1 == 1)
                        {
                            configuration.control_riego_1 = 0;
                            vTaskSuspend(xHandle); 
                        }else{
                            configuration.control_riego_1 = 1;
                            vTaskResume(xHandle); 
                        }    
                        err = NVS_write_i8("control_riego_1", configuration.control_riego_1);    
                    }else{
                        if(configuration.control_riego_2 == 1)
                        {
                            configuration.control_riego_2 = 0;
                            vTaskSuspend(xHandle); 
                        }else{
                            configuration.control_riego_2 = 1;
                            vTaskResume(xHandle); 
                        }
                        err = NVS_write_i8("control_riego_2", configuration.control_riego_2);
                    }
                    
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}else{
                        ESP_LOGI(TAG, "control_riego almacenado\n");

                    }
                    break;
                
                case 'H':
                    const char num = event->data[13];
                    if(num == '1')
                    {
                        recibe_confg_hum(event->data, &configuration, 1);
                        err = NVS_write_i8("hum_sup_1", configuration.hum_sup_1);
                        if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup_1\n");}
                        err = NVS_write_i8("hum_inf_1", configuration.hum_inf_1);
                        if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf_1\n");}else{
                        ESP_LOGI(TAG, "Datos de riego almacenados\n");
                        }
                        recibe_confg_hum(event->data, &configuration, 2);
                        err = NVS_write_i8("hum_sup_2", configuration.hum_sup_2);
                        if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup_2\n");}
                        err = NVS_write_i8("hum_inf_2", configuration.hum_inf_2);
                        if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf_2\n");}else{
                        ESP_LOGI(TAG, "Datos de riego almacenados\n");
                    }
                    }


                    // letra = "A" o cambiar conf. control riego a automatico

                    break; 
                }
            }  
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