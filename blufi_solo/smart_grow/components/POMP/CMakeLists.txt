set(COMPONENT_REQUIRES driver freertos)
idf_component_register(SRCS "pomp.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES DHT HEADER MQTTCJ
                        REQUIRES ${COMPONENT_REQUIRES})

# 