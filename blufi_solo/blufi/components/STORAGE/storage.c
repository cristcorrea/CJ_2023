#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

#include "storage.h"


void NVS_read(char *data, char *Get_Data) // data es la referencia
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    else 
    {   
        size_t required_size = 0; 
        nvs_get_str(my_handle, data, NULL, &required_size);
        char *dato_leido = malloc(required_size);
        err = nvs_get_str(my_handle, data, dato_leido, &required_size);

        switch (err) 
        {
            case ESP_OK:
                printf("Done\n");
                printf("Read data: %s\n", dato_leido);
                strcpy(Get_Data, dato_leido);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
    }
    nvs_close(my_handle);
}

void NVS_write(char *data, char *write_string) // data es la referencia
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    else 
    {   
        nvs_set_str(my_handle, data, write_string);
        printf("write data: %s\n", write_string);
        printf("Commiting updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    }
    nvs_close(my_handle);
}

