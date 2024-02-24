set(COMPONENT_REQUIRES driver freertos)
idf_component_register(SRCS "header.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES RTC STORAGE MQTTCJ RTC HTTPS_OTA
                        REQUIRES ${COMPONENT_REQUIRES})