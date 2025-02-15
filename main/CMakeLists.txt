idf_component_register(
    SRCS
        "projeto-final-JLMS.c"
        "src/gpio_setup.c"
        "src/adc_module.c"
        "src/wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        mqtt
        esp_wifi
        nvs_flash
        driver
        esp_adc
)