#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

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

#define TAG "MQTT_HANDLER"

extern const uint8_t hivemq_certificate_pem_start[]   asm("_binary_hivemq_certificate_pem_start");

extern const uint8_t hivemq_certificate_pem_end[]   asm("_binary_hivemq_certificate_pem_end");


extern config_data configuration;
extern QueueHandle_t riegoQueue; 
extern SemaphoreHandle_t semaphoreFecha;
extern TaskHandle_t humidityMeasureHandle; 

esp_mqtt_client_handle_t client; 


void enviarDatos(char * topic, bool fecha)
{

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
        humedad = getHumidity(datos);
    }

    size_t message_size; 
    char *hora = queHoraEs();
    if(fecha)
    {
        message_size = snprintf(NULL, 0, "%i,%i,%i,%.1f,%i,%s",
            configuration.soilHumidity1, configuration.soilHumidity2,  humedad, temperatura_amb, lux_rounded, hora) + 1;
    }else{
        message_size = snprintf(NULL, 0, "%i,%i,%i,%.1f,%i",
            configuration.soilHumidity1, configuration.soilHumidity2,  humedad, temperatura_amb, lux_rounded) + 1;
    }

    char *message = (char *)malloc(message_size);

    if(message != NULL){              
        snprintf(message, message_size , "%i,%i,%i,%.1f,%i,%s",
                configuration.soilHumidity1, configuration.soilHumidity2, humedad, temperatura_amb, lux_rounded, hora);           
        enviar_mensaje_mqtt(topic, message);
        free(message);
        message = NULL;
    }

    free(hora);
    free(datos); 
    revisarTemperatura(temperatura_amb); 

}   

static void handle_mqtt_connected() {
    char topic_sus[10] = {0};
    memcpy(topic_sus, configuration.cardId, 8);
    strcat(topic_sus, "B");
    suscribirse(topic_sus);
    ESP_LOGI(TAG, "SUSCRITO AL TOPIC: %s", topic_sus);

    topic_sus[8] = 'C';
    memcpy(configuration.cardIdC, topic_sus, 9);
    xSemaphoreGive(semaphoreFecha);
    vTaskResume(humidityMeasureHandle);
    encenderLedWifi();
}

static void handle_manual_irrigation(esp_mqtt_event_handle_t event) {
    int data_len = event->data_len;
    if (data_len > 2) {
        char* mililitros = malloc(data_len - 1);
        strncpy(mililitros, event->data + 2, data_len - 2);
        mililitros[data_len - 2] = '\0';

        int ml = strtol(mililitros, NULL, 10);
        ESP_LOGI(TAG, "mililitros: %s | ml: %i | data_len: %i", mililitros, ml, data_len);

        mensajeRiego mensaje;
        mensaje.valvula = (event->data[1] == '1') ? VALVE1 : VALVE2;
        mensaje.cantidad = ml;
        xQueueSend(riegoQueue, &mensaje, portMAX_DELAY);

        free(mililitros);
    }
}

static void handle_automatic_control(esp_mqtt_event_handle_t event) {
    int err;
    int valve = (event->data[1] == '1') ? 1 : 2;
    bool enable = (event->data[2] == '1');

    if (enable) {
        habilitarSensorSuelo(10);
        if (sensorConectado(valve == 1 ? S1_STATE : S2_STATE)) {
            desHabilitarSensorSuelo();
            recibe_confg_hum(event->data, &configuration, valve);
            err = NVS_write_i8(valve == 1 ? "control_riego_1" : "control_riego_2", valve == 1 ? configuration.control_riego_1 : configuration.control_riego_2);
            if (err != 0) {
                ESP_LOGI(TAG, "Error al guardar configuración de riego");
            } else {
                ESP_LOGI(TAG, "Configuración de riego guardada correctamente");
            }
            enviarEstadoAutomatico(valve, true);
        } else {
            enviarEstadoAutomatico(valve, false);
        }
    } else {
        configuration.control_riego_1 = 0;
        NVS_write_i8(valve == 1 ? "control_riego_1" : "control_riego_2", 0);
    }
}

static void handle_purge(esp_mqtt_event_handle_t event) {
    int valve = (event->data[1] == '1') ? VALVE1 : VALVE2;
    regar(50, valve);
}


static void handle_mqtt_data(esp_mqtt_event_handle_t event) {
    ESP_LOGI("DEBUG MQTT EVENT DATA", "Datos recibidos: %s", event->data);
    char clave1 = event->data[0];
    switch (clave1) {
        case 'S':
            enviarDatos(configuration.cardId, false);
            break;
        case 'R':
            handle_manual_irrigation(event);
            break;
        case 'A':
            handle_automatic_control(event);
            break;
        case 'P':
            handle_purge(event);
            break;
        case 'T':
            stopRiego();
            break;
        default:
            break;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:
            handle_mqtt_connected();
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
            handle_mqtt_data(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
             if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Error en el transporte TCP");
            esp_wifi_disconnect();
            }
            break;
        default:
            ESP_LOGI(TAG, "Evento MQTT no identificado, id:%d", event->event_id);
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

int enviar_mensaje_mqtt(char * topic, char * mensaje)
{
    int mensaje_id = esp_mqtt_client_publish(client, topic, mensaje, 0, 1, 0);
    ESP_LOGI("ENVIO MSJ MQTT", "Mensaje enviado, ID: %d al topic: %s", mensaje_id, topic);
    
    return mensaje_id; 
}

void suscribirse(char * topic)
{
    esp_mqtt_client_subscribe(client, topic, 1);
}