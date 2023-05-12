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
        //char topic_sus[18];
        //memset(topic_sus, 0, 18);
        //memcpy(topic_sus, configuration.UUID, 17);
        //strcat(topic_sus, "R");
        //suscribirse(topic_sus);
        suscribirse(strcat(configuration.UUID,"R"));
        ESP_LOGI(TAG, "Suscrito al topic: %s\n", strcat(configuration.UUID, "R"));
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

        if(event->data[0] == 'C')
        {   
 
            char *message = malloc(140);

            if (message == NULL) {
                ESP_LOGI(TAG, "Error para asignar memoria dinamica\n");
            } else {
  
                snprintf(message, 140, "%sS%iH%iT%.1fL%iM%iI%iU%sA%i",
                        configuration.MAC, mediciones.humedad_suelo,
                        mediciones.humedad_amb, mediciones.temperatura_amb,
                        mediciones.intensidad_luz, configuration.hum_sup,
                        configuration.hum_inf, mediciones.ultimo_riego,
                        configuration.control_riego);
                
                enviar_mensaje_mqtt(configuration.UUID, message);
                // Liberar la memoria del buffer dinámico
                free(message);
            }
        }else{

            if(memcmp(configuration.MAC, event->data, sizeof(char)*12) == 0)  
            {

                char letra = event->data[12];

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
                    if(event->data[13] == '1')
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
                    ESP_LOGI(TAG, "Entra a H\n");
                    recibe_confg_hum(event->data, &configuration);
                    ESP_LOGI(TAG, "Entra a H 2\n");
                    err = NVS_write_i8("hum_sup", configuration.hum_sup);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_sup\n");}
                    err = NVS_write_i8("hum_inf", configuration.hum_inf);
                    if(err != 0){ESP_LOGI(TAG, "No pudo grabarse hum_inf\n");}else{
                        ESP_LOGI(TAG, "Datos de riego almacenados\n");
                    }

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