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

char * queHoraEs(){
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    setenv("TZ", configuration.time_zone, 1); 
    tzset();
    localtime_r(&now, &timeinfo);
    char * strftime_buf = malloc(64);
    strftime(strftime_buf, 64, "%c", &timeinfo);
    return strftime_buf;
}
