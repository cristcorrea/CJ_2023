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

static const char *TAG = "RTC";


void adjust_time(void)
{
    ESP_LOGI(TAG, "Initializing and starting SNTP");
    sntp_setservername(1, "pool.ntp.org");
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

}
