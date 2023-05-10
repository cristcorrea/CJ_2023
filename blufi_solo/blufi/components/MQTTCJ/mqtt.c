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

#define TAG "MQTT"

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t hivemq_certificate_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t hivemq_certificate_pem_start[]   asm("_binary_hivemq_certificate_pem_start");
#endif
extern const uint8_t hivemq_certificate_pem_end[]   asm("_binary_hivemq_certificate_pem_end");

extern SemaphoreHandle_t semaphoreRTC;

extern config_data configuration;
extern sensor_data mediciones; 
extern TaskHandle_t xHandle;

esp_mqtt_client_handle_t client; 

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED\n");
        char topic_sus[18];
        memset(topic_sus, 0, 18);
        memcpy(topic_sus, configuration.UUID, 17);
        strcat(topic_sus, "R");
        suscribirse(topic_sus);
        ESP_LOGI(TAG, "Suscrito al topic: %s\n", topic_sus);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        xSemaphoreGive(semaphoreRTC);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:

        // Primero tengo que filtrar si el tamaño de lo que arriba es menor a 12
        if(event->data_len < 12)
        {
            // Si es menor, entonces me solicita configuración
            // Chequeo que el caracter recibido sea la C  
            char consulta[2];
            memset(consulta, 0, sizeof(char) * 2);
            memcpy(consulta, event->data, sizeof(char));
            if(consulta[0] == 'C')
            {
            
            // Enviar datos de la consulta
            char message[140];
            sprintf(message, "%sS%iH%iT%.1fL%iM%iI%iU%sA%i",
            configuration.MAC, mediciones.humedad_suelo,
            mediciones.humedad_amb, mediciones.temperatura_amb,
            mediciones.intensidad_luz, configuration.hum_sup, configuration.hum_inf, 
            mediciones.ultimo_riego, configuration.control_riego);
            enviar_mensaje_mqtt(configuration.UUID, message);

            }
        }else{
            
            char mac[7];
            memset(mac, 0, sizeof(char) * 7);
            memcpy(mac, esp_bt_dev_get_address(), sizeof(char) * 6);  // Guardo la mac del esp en "mac"
            
            char rec_mac[13];                               
            memset(rec_mac, 0, sizeof(char) * 13);
            memcpy(rec_mac, event->data, sizeof(char) * 12);          // mac recibida por mqtt
            /*
            char mac_final[7];                                        // pasa de un array de 12 a 6
            memset(mac_final, 0, sizeof(char) * 7);
            int i, j; 
            for(i = 0, j = 0; i < 12; i += 2, j++)
            {
                char hex[3];
                strncpy(hex, rec_mac + i, 2);
                hex[2] = '\0';
                mac_final[j] = strtol(hex, NULL, 16);
            }
            */

            ESP_LOGI(TAG, "Rec: %s Loc: %s\n", rec_mac, configuration.MAC);
            

            int result = memcmp(configuration.MAC, rec_mac, sizeof(char)*12);

            if(result == 0)           // compara las dos mac  
            {
                
                char confg_recibida[event->data_len];
                memset(confg_recibida, 0, sizeof(char) * event->data_len);
                memcpy(confg_recibida, event->data, sizeof(char) * event->data_len);
                char letra = confg_recibida[12];
                int err; 
                
                switch (letra)
                {
                case 'R':
                    // Enciende el riego manual
                    regar();
                    ultimo_riego();           
                    break;
                
                case 'A':
                    // Activa/ desactiva el control automatico de riego DEBO GUARDAR EN MEMORIA
                    if(confg_recibida[13] == '1')
                    {
                        configuration.control_riego = 1;
                        vTaskResume(xHandle);
                    }else{
                        configuration.control_riego = 0;
                        vTaskSuspend(xHandle);
                    }
                    err = NVS_write_i8("control_riego", configuration.control_riego);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}else{
                        ESP_LOGI(TAG, "control_riego almacenado\n");

                    }
                    break;
                
                case 'H':
                    // Recibo configuraciones de humedad DEBO GUARDAR EN MEMORIA funciona ok!
                    recibe_confg_hum(event->data, &configuration);
                    err = NVS_write_i8("hum_sup", configuration.hum_sup);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}
                    err = NVS_write_i8("hum_inf", configuration.hum_inf);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf\n");}else{
                        ESP_LOGI(TAG, "Datos de riego almacenados");
                    }

                    break; 
                }
            }else{
                ESP_LOGI(TAG, "Falla la comparacion. Result: %i\n", result);
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