#include <stdio.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/semphr.h"

#include "wifi.h"
#include "mqtt.h"
#include "dht.h"
#include "mqtt_client.h"
#include "bh1750.h"

//static const char* TAG = "DHT11";


xSemaphoreHandle conexionWifiSemaforo; // Semaforo para habilitar inicio MQTT
xSemaphoreHandle conexionMQTTSemaforo; // Semaforo para habilitar inicio mediciones


void conexionMqtt(void* params)
{
  while(true)
  {
    if(xSemaphoreTake(conexionWifiSemaforo, portMAX_DELAY))
    {
      ESP_LOGI("Main Task", "Realiza conexion MQTT");
      mqtt_start(); 
    }
  }
}

void sensores_start(void* params)
{
    char mensaje[50];
    if(xSemaphoreTake(conexionMQTTSemaforo, portMAX_DELAY))
    {
      while(true)
      {
      DHTerrorHandler(readDHT());
      sprintf(mensaje, "Temp: %.1f °C \n Hum: %i%%", dht_data.temperature, dht_data.humidity);
      enviar_mensaje_mqtt("sensores/temperatura", mensaje);
      //ESP_LOGI(TAG, "Temperature : %.1f °C ", dht_data.temperature);
      //ESP_LOGI(TAG, "Humidity : %i %%", dht_data.humidity);
      vTaskDelay(pdMS_TO_TICKS(3000));
      }
    }
}


void app_main(void)
{
  //Inicializa memoria no volátil NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  //---------------------------------
  conexionWifiSemaforo = xSemaphoreCreateBinary(); 
  conexionMQTTSemaforo = xSemaphoreCreateBinary(); 

  xTaskCreate(&conexionMqtt,
             "Conecta con el broker MQTT",
             4096, 
             NULL,
             1,
             NULL);

  xTaskCreate(&sensores_start,
            "Inicia mediciones de sensores",
            4096, 
            NULL,
            1,
            NULL);

  wifi_start(); 
}
