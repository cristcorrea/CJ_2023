set(COMPONENT_REQUIRES driver)
idf_component_register(SRCS "header.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES RTC STORAGE MQTTCJ RTC
                        REQUIRES ${COMPONENT_REQUIRES})