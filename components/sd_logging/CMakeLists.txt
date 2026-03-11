idf_component_register(
    SRCS "sd_logging.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common driver temp_sensor
)
