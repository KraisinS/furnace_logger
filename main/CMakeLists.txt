idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_common driver temp_sensor tm1637_display sd_logging
                                nvs_flash esp_event esp_netif esp_wifi lwip
                                esp_http_server)
