#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "header.h"


static const char *TAG = "RTC";
extern config_data configuration; 
extern SemaphoreHandle_t semaphoreOta;

int consultaAnio(void)
{
    time_t now;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);
    int year = timeinfo.tm_year + 1900;
    return year; 
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void adjust_time(char *time_zone) {
    // Configura la zona horaria en GMT/UTC
    setenv("TZ", "GMT0", 1);
    tzset();

    ESP_LOGI(TAG, "Initializing and starting SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // Espera a que SNTP sincronice la hora
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set (pool.ntp)... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry == retry_count) {
        sntp_stop();
        sntp_setservername(0, "time.google.com");
        sntp_init();
        retry = 0;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
            ESP_LOGI(TAG, "Waiting for system time to be set (google)... (%d/%d)", retry, retry_count);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Obtén la hora actual después de la sincronización
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Usa strtol para convertir el valor de time_zone a un número entero
    char *endptr;
    long time_zone_offset = strtol(time_zone + 1, &endptr, 10);

    // Verifica si hubo errores en la conversión
    if (*endptr != '\0' && *endptr != '\n') {
        ESP_LOGE(TAG, "Error converting time_zone to integer");
        return; // Manejar el error apropiadamente
    }

    // Ajusta la hora según la zona horaria
    if(time_zone[1] == '1')
    {
        timeinfo.tm_hour += (int)time_zone_offset;
    }else{
        timeinfo.tm_hour -= (int)time_zone_offset;
    }
    mktime(&timeinfo); // Asegura que la hora esté en un rango válido

    // Convierte y muestra la hora ajustada
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time adjusted for timezone is: %s", strftime_buf);

    xSemaphoreGive(semaphoreOta);
}


/*
void adjust_time(char * time_zone)
{
    ESP_LOGI(TAG, "Initializing and starting SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set (pool.ntp)... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry == 15)
    {
        sntp_stop();
        sntp_setservername(0, "time.google.com");
        sntp_init();
        retry = 0; 
        while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
        {
            ESP_LOGI(TAG, "Waiting for system time to be set (google)... (%d/%d)", retry, retry_count);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    setenv("TZ", time_zone, 1); 
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Roma is: %s", strftime_buf);
    xSemaphoreGive(semaphoreOta);
}
*/

char * queHoraEs() {
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    setenv("TZ", configuration.time_zone, 1); 
    tzset();
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%a,%b,%d,%T,%Y", &timeinfo);

    size_t len = strlen(strftime_buf);
    char *result = (char *)malloc(len + 1); 
    if (result != NULL) {
        strcpy(result, strftime_buf);
    }
    
    return result;
}








/*
char * queHoraEs(){
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    setenv("TZ", configuration.time_zone, 1); 
    tzset();
    localtime_r(&now, &timeinfo);
    char * strftime_buf = malloc(64);
    strftime(strftime_buf, 64, "%c", &timeinfo);

    size_t len = strlen(strftime_buf);
    char * result = (char *)malloc(len + 1); 
    for(size_t i = 0; i < len; i++)
    {
        if(strftime_buf[i] == ' ')
        {
            result[i] = ',';
        }else{
            result[i] = strftime_buf[i];
        }
    }
    if(result[8] == ',')
    {
        result[8] = '0';
    }
    result[len] = '\0';
    return result;
}
*/