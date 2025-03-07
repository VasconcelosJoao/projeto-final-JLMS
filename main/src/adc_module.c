#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"

// Remova a linha abaixo
// const static char *TAG = "ADC_MODULE";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/

//-------------ADC1 Init---------------//
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1;

void adc_init(adc_unit_t adc_unit)
{
    init_config1.unit_id = adc_unit;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
}

void adc_config_pin(adc_channel_t channel)
{
    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // Atualize para o valor não deprecado
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &config));
}

void adc_deinit()
{
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}

int analogRead(adc_channel_t channel){
    int adc_raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &adc_raw));
    return adc_raw;
}