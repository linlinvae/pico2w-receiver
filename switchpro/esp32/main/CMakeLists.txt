idf_component_register(SRCS "main.c" "switchpro_platform.c"
    "../../common/sp_link.c" "../../common/sp_endpoint.c"
    INCLUDE_DIRS "." "../../common"
    REQUIRES bluepad32 btstack esp_driver_uart esp_timer esp_system)
