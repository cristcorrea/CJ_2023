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

void adjust_time(int time_zone) {
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

    int first_digit = time_zone / 100;
    
    int offset_hours = time_zone % 100;

    if (first_digit == 1) {
        timeinfo.tm_hour += offset_hours;
        ESP_LOGI("Ajuste de hora", "Ajuste de hora positivo: %i", offset_hours);
    } else {
        timeinfo.tm_hour -= offset_hours;
        ESP_LOGI("Ajuste de hora", "Ajuste de hora negativo: %i", offset_hours);
    }
    
    mktime(&timeinfo); // Asegura que la hora esté en un rango válido

    // Convierte y muestra la hora ajustada
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time adjusted for timezone is: %s", strftime_buf);

    xSemaphoreGive(semaphoreOta);
}

char* queHoraEs() {
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    localtime_r(&now, &timeinfo);
    int first_digit = configuration.time_zone / 100;
    
    int offset_hours = configuration.time_zone % 100;

    if (first_digit == 1) {
        timeinfo.tm_hour += offset_hours;
    } else {
        timeinfo.tm_hour -= offset_hours;
    }
    
    char* strftime_buf = malloc(64);
    strftime(strftime_buf, 64, "%c", &timeinfo);

    size_t len = strlen(strftime_buf);
    char* result = (char*)malloc(len + 1);
    
    for (size_t i = 0; i < len; i++) {
        if (strftime_buf[i] == ' ') {
            result[i] = ',';
        } else {
            result[i] = strftime_buf[i];
        }
    }

    if (result[8] == ',') {
        result[8] = '0';
    }

    result[len] = '\0';
    return result;
}


/*
char * queHoraEs()
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime); // Obtener la hora actual en segundos desde la época Unix
    timeinfo = localtime(&rawtime); // Convertir a estructura tm con hora local

    // Crear una cadena de caracteres formateada para la hora actual
    char *current_time_str = (char *)malloc(9 * sizeof(char)); // HH:MM:SS\0
    snprintf(current_time_str, 9, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    return current_time_str;
}
*/

/*
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