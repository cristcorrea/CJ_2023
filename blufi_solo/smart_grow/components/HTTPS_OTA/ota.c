#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "lwip/sockets.h"
#include "esp_https_ota.h"
#include "strings.h"
#include "header.h"
#include "ota.h"

#define UPDATE_JSON_URL		"http://check.cjindoors.com/firmware.json"



const char *TAG = "HTTPS_OTA";

// server certificates
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");
extern TaskHandle_t msjTaskHandle; 

// receive buffer
char * rcv_buffer;



esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
				strncpy(rcv_buffer, (char*)evt->data, evt->data_len);
				ESP_LOGI(TAG, "datos copiados: %s\n", rcv_buffer);
            }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

void update_ota()
{
		rcv_buffer = malloc(73);
        printf("Looking for a new firmware...\n");
		// configure the esp_http_client
		esp_http_client_config_t config = {
        .url = UPDATE_JSON_URL,
        .event_handler = _http_event_handler,
		};
		esp_http_client_handle_t client = esp_http_client_init(&config);
	
		// downloading the json file
		esp_err_t err = esp_http_client_perform(client);
		int code = esp_http_client_get_status_code(client);

		if(err == ESP_OK && code == 200) {
			
			cJSON *json = cJSON_Parse(rcv_buffer);
			
			if(json == NULL) printf("downloaded file is not a valid json, aborting...\n");
			else {

				parpadeo();
				encenderLedTouch();
				cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
				cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");
				
				// check the version
				if(!cJSON_IsNumber(version)) printf("unable to read new version , aborting...\n");
				else {
					
					double new_version = version->valuedouble;
					if(new_version > FIRMWARE_VERSION) {
						
						printf("current firmware version (%.1f) is lower than the available one (%.1f), upgrading...\n", FIRMWARE_VERSION, new_version);
						if(cJSON_IsString(file) && (file->valuestring != NULL)) {
							printf("downloading and installing new firmware (%s)...\n", file->valuestring);
							
							esp_http_client_config_t ota_client_config = {
								.url = file->valuestring,
								.cert_pem = (char*)certificate_pem_start,
							};
							esp_https_ota_config_t ota_config = {
								.http_config = &ota_client_config,
							};
							esp_err_t ret = esp_https_ota(&ota_config);
							if (ret == ESP_OK) {
								printf("OTA OK, restarting...\n");
								
								for(int i = 0; i < 10; i++)
								{
									parpadeo();
								}
								vTaskDelay(pdMS_TO_TICKS(1000));
								esp_restart();
							} else {
								printf("OTA failed...\n");
							}
						}
						else printf("unable to read the new file name, aborting...\n");
					}
					else printf("current firmware version (%.1f) is greater or equal to the available one (%.1f), nothing to do...\n", FIRMWARE_VERSION, new_version);
					apagarLedTouch();
					vTaskResume(msjTaskHandle);
				}
			}
		}
		else{
			ESP_LOGI(TAG, "unable to download the json file, code: %i\n", code);

		}
		// cleanup
		esp_http_client_cleanup(client);
		free(rcv_buffer);
		printf("\n");
		vTaskResume(msjTaskHandle);
}


