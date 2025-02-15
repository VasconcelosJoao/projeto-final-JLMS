#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "adc_module.h"
#include "gpio_setup.h"
#include "wifi.h"
#include "math.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "mqtt_client.h"
#include "esp_netif.h"

#define THINGSBOARD_HOST "tb.fse.lappis.rocks"
#define THINGSBOARD_PORT 1883
#define THINGSBOARD_TOKEN "lJoCVzRyNoiqKl0JeEEO"

#define LED_R 25
#define LED_G 26
#define LED_B 27

#define ADC_CHANNEL ADC_CHANNEL_6  // GPIO36 (VP) no ESP32
#define R_FIXED 4700
#define A 0.001129148
#define B 0.000234125
#define C 0.0000000876741

#define LEDC_RES    LEDC_TIMER_13_BIT  // Defina a resolução do timer
#define LEDC_FREQ   5000               // Defina a frequência do timer

static esp_mqtt_client_handle_t mqtt_client = NULL;
static const char *TAG = "MQTT";
static bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected to ThingsBoard!");
            mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected from ThingsBoard! Retrying...");
            mqtt_connected = false;
            esp_mqtt_client_reconnect(mqtt_client);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error Event");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                        strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(TAG, "Other MQTT event id:%d", event->event_id);
            break;
    }
}

void mqtt_init(void)
{
    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    int retry_count = 0;
    while (retry_count < 30) {  // Wait up to 30 seconds
        if (esp_netif_is_netif_up(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))) {
            break;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry_count++;
    }
    
    if (retry_count >= 30) {
        ESP_LOGE(TAG, "WiFi connection timeout!");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .hostname = THINGSBOARD_HOST,
                .port = THINGSBOARD_PORT,
                .transport = MQTT_TRANSPORT_OVER_TCP,
            },
        },
        .credentials = {
            .username = THINGSBOARD_TOKEN,  // ThingsBoard uses token as username
            .client_id = "ESP32_Temperature_Sensor",
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

void publish_temperature(float temperature)
{
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT not connected, skipping publish");
        return;
    }

    char data[100];
    snprintf(data, sizeof(data), "{\"temperature\": %.2f}", temperature);
    int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", data, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish temperature: %d", msg_id);
    } else {
        ESP_LOGI(TAG, "Successfully published temperature: %.2f", temperature);
    }
}

void configure_led_pwm() {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_RES,
        .freq_hz = LEDC_FREQ,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel[3] = {
        {.channel = LEDC_CHANNEL_0, .gpio_num = LED_R, .speed_mode = LEDC_HIGH_SPEED_MODE, .timer_sel = LEDC_TIMER_0},
        {.channel = LEDC_CHANNEL_1, .gpio_num = LED_G, .speed_mode = LEDC_HIGH_SPEED_MODE, .timer_sel = LEDC_TIMER_0},
        {.channel = LEDC_CHANNEL_2, .gpio_num = LED_B, .speed_mode = LEDC_HIGH_SPEED_MODE, .timer_sel = LEDC_TIMER_0}
    };
    for (int i = 0; i < 3; i++) {
        ledc_channel_config(&ledc_channel[i]);
    }
}

void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    ESP_LOGI("LED", "Setting LED color to R:%d, G:%d, B:%d", r, g, b);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, r * (8191 / 255));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, g * (8191 / 255));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, b * (8191 / 255));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2);
}

void app_main(void)
{
    // Initialize components
    configure_led_pwm();
    wifi_init();
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Give WiFi some time to connect
    mqtt_init();
    
    // ADC configuration
    adc_init(ADC_UNIT_1);
    pinMode(ADC_CHANNEL, GPIO_ANALOG);

    // Main loop
    while (true)
    {
        int valor_adc = analogRead(ADC_CHANNEL);
        ESP_LOGI("ADC", "Valor ADC: %d", valor_adc);

        float voltage = (valor_adc / 4095.0) * 3.3;
        float resistance = R_FIXED * (voltage / (3.3 - voltage));
        float temp_kelvin = 1.0 / (A + B * log(resistance) + C * pow(log(resistance), 3));
        float temp_celsius = temp_kelvin - 273.15;

        ESP_LOGI("KY-028", "Temperatura(C): %.2f", temp_celsius);
        
        // Publish temperature and update LED
        publish_temperature(temp_celsius);
        
        if (temp_celsius > 39.0) {
            set_led_color(0, 255, 255);  // Red for high temperature
        } else {
            set_led_color(255, 0, 255);  // Green for normal temperature
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}