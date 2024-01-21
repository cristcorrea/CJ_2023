#include <stdio.h>
#include "header.h"
#include <string.h>
#include <stdint.h>
#include "time.h"
#include "storage.h"
#include "mqtt.h"
#include "ntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota.h"

#define TOUCH_LED GPIO_NUM_4
#define WIFI_LED GPIO_NUM_2

extern config_data configuration; 


void recibe_confg_hum(char str[], config_data *cfg, int sensor)
{
    int posH = strcspn(str, "H");
    int posL = strcspn(str, "L");
    char *hum_H = str + posH + 1;
    char *hum_L = str + posL + 1;

    if(sensor == 1)
    {
        cfg->hum_sup_1 = strtol(hum_H, NULL, 10);
        cfg->hum_inf_1 = strtol(hum_L, NULL, 10);
    }else{
        cfg->hum_sup_2 = strtol(hum_H, NULL, 10);
        cfg->hum_inf_2 = strtol(hum_L, NULL, 10);
    }
}


void bytesToHex(const unsigned char* bytes, int size, char* hexString) 
{
    for (int i = 0; i < size; i++) {
        sprintf(hexString + (i * 2), "%02X", bytes[i]);
    }
    hexString[size * 2] = '\0';
}

void ultimoRiego(const char *prefijo, int ml) {

    char *hora = queHoraEs();
    size_t message_size = snprintf(NULL, 0, "%s%s, %i", prefijo, hora, ml) + 1;
    char *message = (char *)malloc(message_size);
    if (message != NULL) {
        snprintf(message, message_size, "%s%s, %d", prefijo, hora, ml);
        enviar_mensaje_mqtt(configuration.cardIdC, message);
        enviar_mensaje_mqtt(configuration.cardId, message); //queda comentado hasta que se arregle el bug
        free(hora);
        free(message);
    }
}

/*Funciones led touch*/

void touchLedConfig()
{
    gpio_config_t touch_config;
    touch_config.pin_bit_mask = (1ULL << TOUCH_LED);
    touch_config.mode = GPIO_MODE_OUTPUT;
    touch_config.pull_up_en = GPIO_PULLUP_DISABLE;
    touch_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    touch_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&touch_config);
    gpio_set_level(TOUCH_LED, 0);
}


void encenderLedTouch()
{
    gpio_set_level(TOUCH_LED, 1);
}

void apagarLedTouch()
{
    gpio_set_level(TOUCH_LED, 0);
}

/*Funciones led wifi*/

void wifiLedConfig()
{
    gpio_config_t wifi_led_config;
    wifi_led_config.pin_bit_mask = (1ULL << WIFI_LED);
    wifi_led_config.mode = GPIO_MODE_OUTPUT;
    wifi_led_config.pull_up_en = GPIO_PULLUP_DISABLE;
    wifi_led_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    wifi_led_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&wifi_led_config);
    gpio_set_level(WIFI_LED, 0);

}

void apagarLedWifi(void)
{
    gpio_set_level(WIFI_LED, 0);
}

void encenderLedWifi(void)
{
    gpio_set_level(WIFI_LED, 1);

}


void parpadeo(void)
{
    encenderLedTouch();
    vTaskDelay(pdMS_TO_TICKS(500));
    apagarLedTouch();
    vTaskDelay(pdMS_TO_TICKS(500));
}

void enviarVersion()
{
    size_t message_size = snprintf(NULL, 0, "%.1f", FIRMWARE_VERSION) + 1;
    char *message = (char *)malloc(message_size);
    if (message != NULL) {
        snprintf(message, message_size, "%.1f", FIRMWARE_VERSION);
        //enviar_mensaje_mqtt(configuration.cardIdC, message);
        enviar_mensaje_mqtt(configuration.cardId, message);
        free(message);
    }
}