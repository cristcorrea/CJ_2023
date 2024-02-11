set(COMPONENT_REQUIRES  esp_event esp_system app_update esp_http_client esp_https_ota lwip freertos log)
idf_component_register(SRCS "ota.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES CJSON HEADER
                        REQUIRES ${COMPONENT_REQUIRES}
                        EMBED_TXTFILES certificate.pem)