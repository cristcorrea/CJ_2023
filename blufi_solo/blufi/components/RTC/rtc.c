#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#include "rtc.h"

static const char* TAG = "RTC";

static void obtain_time(void);

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notificación de un evento de sincronización");
}

static void initialize_sntp()
{
    ESP_LOGI(TAG, "Inicializate SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialize_sntp();

    time_t now = 0; 
    struct tm timeinfo = {0};
    int retry = 0; 
    const int retry_count = 10; 
    while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Esperando que se ponga en hora el sistema... (%d/%d)", retry, retry_count);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

}



void dataUpdate(void)
{
    time_t now; 
    struct tm timeinfo; 
    time(&now);
    localtime_r(&now, &timeinfo);

    if(timeinfo.tm_year < (2021 - 1990))
    {
        ESP_LOGI(TAG, "Hora no establecida. Inicia proceso de puesta en hora.");
        obtain_time();
        time(&now);
    }

    char strftime_buff[64];
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buff, sizeof(strftime_buff), "%c", &timeinfo);
    ESP_LOGI(TAG, "La hora actual en Italia es: %s", strftime_buff);

}