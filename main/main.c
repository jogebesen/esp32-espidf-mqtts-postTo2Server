#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_system.h>
#include <aht.h>
#include <bmp280.h>
#include <esp_err.h>

#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <esp_log.h>

#include "protocol_examples_common.h"
#include "../build/config/sdkconfig.h"

#include "mqtt_onenet_emqx.h"






#ifdef CONFIG_TEMPERATURE_SENSOR_I2C_ADDRESS_GND
#define ADDR_DHT20 AHT_I2C_ADDRESS_GND
#endif
#ifdef CONFIG_TEMPERATURE_SENSOR_I2C_ADDRESS_VCC
#define ADDR_DHT20 AHT_I2C_ADDRESS_VCC
#endif

#ifdef CONFIG_TEMPERATURE_SENSOR_TYPE_AHT1x
#define AHT_TYPE AHT_TYPE_AHT1x
#endif

#ifdef CONFIG_TEMPERATURE_SENSOR_TYPE_AHT20
#define AHT_TYPE AHT_TYPE_AHT20
#endif


#ifdef CONFIG_AIR_PRESSURE_I2C_ADDRESS_GND
#define AIR_PRESSURE_I2C_ADDRESS_BMP280 BMP280_I2C_ADDRESS_0
#endif
#ifdef CONFIG_AIR_PRESSURE_I2C_ADDRESS_VCC
#define AIR_PRESSURE_I2C_ADDRESS_BMP280 BMP280_I2C_ADDRESS_1
#endif



#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif


static const char *AHT_TAG = "aht-show";

static const char *TAG_BMP280 = "bmp280-show";

static const char *TAG_MAIN = "mainFunction-show";



static float temperature = -40, humidity = 0, bmp280_air_pressure = 0, bmp280_temp = -40;



void uFunc_dht20_get_temp_and_humidity(void)
{
    aht_t dev = { 0 };
    dev.mode = AHT_MODE_NORMAL;
    dev.type = AHT_TYPE;

    aht_init_desc(&dev, ADDR_DHT20, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_PORT, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_MASTER_SDA, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_MASTER_SCL);
    aht_init(&dev);

    bool calibrated;
    aht_get_status(&dev, NULL, &calibrated);
    if (calibrated == true)
        ESP_LOGI(AHT_TAG, "Sensor calibrated");
    else
        ESP_LOGW(AHT_TAG, "Sensor not calibrated!");

    esp_err_t res;



    res = aht_get_data(&dev, &temperature, &humidity);
    if (res == ESP_OK)
        ESP_LOGI(AHT_TAG, "Temperature: %.1f°C, Humidity: %.2f%%", temperature, humidity);
    else
        ESP_LOGE(AHT_TAG, "Error reading data: %d (%s)", res, esp_err_to_name(res));


    aht_free_desc(&dev);
    vTaskDelay(500 / portTICK_PERIOD_MS);

}


void uFunc_bmp280_get_airPressure_and_temperature(void)
{
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    bmp280_t dev;
    memset(&dev, 0, sizeof(bmp280_t));

    bmp280_init_desc(&dev, AIR_PRESSURE_I2C_ADDRESS_BMP280, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_PORT, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_MASTER_SDA, CONFIG_CLIMATE_TERMINAL_SENSOR_I2C_MASTER_SCL);
    bmp280_init(&dev, &params);

    bool bme280p = dev.id == BME280_CHIP_ID;
    ESP_LOGI(TAG_BMP280, "BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

    bmp280_force_measurement(&dev);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    float bme280p_humidity;



        if (bmp280_read_float(&dev, &bmp280_temp, &bmp280_air_pressure, &bme280p_humidity) != ESP_OK)
        {
            ESP_LOGI(TAG_BMP280, "Temperature/pressure reading failed\n");

            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        /* float is used in printf(). you need non-default configuration in
         * sdkconfig for ESP8266, which is enabled by default for this
         * example. see sdkconfig.defaults.esp8266
         */
        ESP_LOGI(TAG_BMP280, "Pressure: %.2f Pa, Temperature: %.2f C", bmp280_air_pressure, bmp280_temp);
        if (bme280p)
            ESP_LOGI(TAG_BMP280, ", Humidity: %.2f\n", bme280p_humidity);
        else
            ESP_LOGI(TAG_BMP280, "\n");


    bmp280_free_desc(&dev);

    vTaskDelay(500 / portTICK_PERIOD_MS);
    

}


void uTask_post_temperature_and_humidity(void)
{
    esp_mqtt_client_handle_t mqtt_onenet_client;
    uFunc_mqtt_send2onenet_init(&mqtt_onenet_client);
    
    esp_mqtt_client_handle_t mqtt_emqx_client;
    uFunc_mqtt_send2emqx_init(&mqtt_emqx_client);

    while(1)
    {
        uFunc_dht20_get_temp_and_humidity();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        uFunc_bmp280_get_airPressure_and_temperature();

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG_MAIN, "temperature: %f, humidity: %f, airPressure: %f, bmp_temp: %f", temperature, humidity, bmp280_air_pressure, bmp280_temp);

        uFunc_mqtt_onenet_post_data(&mqtt_onenet_client, temperature, humidity, bmp280_air_pressure, bmp280_temp);

        vTaskDelay(5*1000 / portTICK_PERIOD_MS);

        uFunc_mqtt_emqx_post_data(&mqtt_emqx_client, temperature, humidity, bmp280_air_pressure, bmp280_temp);

        vTaskDelay(3*1000 / portTICK_PERIOD_MS);        
    }
}


void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "[APP] Startup..");
    ESP_LOGI(TAG_MAIN, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG_MAIN, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());    
    
    
    ESP_ERROR_CHECK(i2cdev_init());
    
    
    xTaskCreate((void *)uTask_post_temperature_and_humidity, "Post_sensorData", 1024 * 10, NULL, 1, NULL);





}