set(COMPONENT_REQUIRES freertos log esp_system esp_event app_update esp_http_client esp_https_ota lwip)
idf_component_register(SRCS "ota.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES CJSON
                        REQUIRES ${COMPONENT_REQUIRES}
                        EMBED_TXTFILES certificate.pem)
