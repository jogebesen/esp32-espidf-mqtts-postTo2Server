#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_system.h>
#include <aht.h>
#include <bmp280.h>
#include <pcf8574.h>
#include <hd44780.h>
#include <i2cdev.h>
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

#include "onenet_dev_token.h"





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

static const char *TAG_LCD1602 = "lcd1602-show";

static const char *TAG_MAIN = "mainFunction-show";



static float temperature = -40, humidity = 0, bmp280_air_pressure = 0, bmp280_temp = -40;
SemaphoreHandle_t uSemaphoreMutex_sensorData; //该互斥信号量用于保护上述四个 static float 参数在任务访问时不发生冲突。

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

    aht_reset(&dev);

    vTaskDelay(150 / portTICK_PERIOD_MS);

    res = aht_get_data(&dev, &temperature, &humidity);
    if (res == ESP_OK)
        ESP_LOGI(AHT_TAG, "Temperature: %.1f°C, Humidity: %.2f%%", temperature, humidity);
    else
        ESP_LOGE(AHT_TAG, "Error reading data: %d (%s)", res, esp_err_to_name(res));


    aht_free_desc(&dev);
    vTaskDelay(100 / portTICK_PERIOD_MS);

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

    vTaskDelay(200 / portTICK_PERIOD_MS);
    

}


void uTask_dht20_and_bmp280_sense(void *pvParameters)
{
    while(1)
    {
        if(xSemaphoreTake(uSemaphoreMutex_sensorData, portMAX_DELAY) == pdTRUE){ //未获取到互斥信号量时，portMAX_DELAY意味着该任务将无限阻塞。
        uFunc_dht20_get_temp_and_humidity();
        vTaskDelay(150 / portTICK_PERIOD_MS);
        uFunc_bmp280_get_airPressure_and_temperature();

        ESP_LOGI(TAG_MAIN, "temperature: %f, humidity: %f, airPressure: %f, bmp_temp: %f", temperature, humidity, bmp280_air_pressure, bmp280_temp);

        

        xSemaphoreGive(uSemaphoreMutex_sensorData); 
        }

        vTaskDelay(3*1000 / portTICK_PERIOD_MS);
    }
}


void uTask_mqtt_post_to_onenet(void)
{
    esp_mqtt_client_handle_t mqtt_onenet_client;
    uFunc_mqtt_send2onenet_init(&mqtt_onenet_client);

    int time_min;

    while(1)
    {
        time_min = 15;

        if(xSemaphoreTake(uSemaphoreMutex_sensorData, portMAX_DELAY) == pdTRUE){
            if(humidity == 0 && bmp280_air_pressure == 0){
                xSemaphoreGive(uSemaphoreMutex_sensorData);
                vTaskDelay(5*1000 / portTICK_PERIOD_MS);
                time_min = 15;
                continue;
            }
            uFunc_mqtt_onenet_post_data(&mqtt_onenet_client, temperature, humidity, bmp280_air_pressure, bmp280_temp);

            ESP_LOGD(TAG_MAIN, "mqtt_post_to_onenet");

            xSemaphoreGive(uSemaphoreMutex_sensorData);
        }

        while(time_min > 0)
        {
            vTaskDelay(60*1000 / portTICK_PERIOD_MS);
            time_min = time_min - 1;
        }
        
    }

}


void uTask_mqtt_post_to_emqx(void)
{
    esp_mqtt_client_handle_t mqtt_emqx_client;
    uFunc_mqtt_send2emqx_init(&mqtt_emqx_client);

    while(1)
    {
        if(xSemaphoreTake(uSemaphoreMutex_sensorData, portMAX_DELAY) == pdTRUE){
            if(humidity == 0 && bmp280_air_pressure == 0){
                xSemaphoreGive(uSemaphoreMutex_sensorData);
                vTaskDelay(5*1000 / portTICK_PERIOD_MS);
                continue;
            }
            uFunc_mqtt_emqx_post_data(&mqtt_emqx_client, temperature, humidity, bmp280_air_pressure, bmp280_temp);

            ESP_LOGD(TAG_MAIN, "mqtt_post_to_emqx");

            xSemaphoreGive(uSemaphoreMutex_sensorData);
        }
        vTaskDelay(5*1000 / portTICK_PERIOD_MS);
    }   
}






static i2c_dev_t pcf8574;

static esp_err_t write_lcd_data(const hd44780_t *lcd, uint8_t data)
{
    return pcf8574_port_write(&pcf8574, data);
}

void uTask_lcd1602_display_sensor_data(void *pvParemeters)
{
    static const uint8_t centigrade[] = {0x18, 0x1e, 0x09, 0x08, 0x08, 0x08, 0x09, 0x06};

    hd44780_t lcd1602 = {
        .write_cb = write_lcd_data,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = 0,
            .e = 2,
            .d4 = 4,
            .d5 = 5,
            .d6 = 6,
            .d7 = 7,
            .bl = 3
        }
    };

    memset(&pcf8574, 0, sizeof(i2c_dev_t));
    pcf8574_init_desc(&pcf8574, CONFIG_LCD1602_PCF8574_I2C_ADDRESS, CONFIG_LCD1602_PCF8574_I2C_PORT, CONFIG_LCD1602_PCF8574_I2C_SDA_PIN, CONFIG_LCD1602_PCF8574_I2C_SCL_PIN); //SCL: 19, SDA: 18.
    
    char display_data[32] = {0};
    
    while(1)
    {
        //如果还未连接使用pcf8574的lcd1602屏幕的话。
        if(i2c_dev_probe(&pcf8574, I2C_DEV_WRITE) != ESP_OK){
            ESP_LOGW(TAG_LCD1602, "lcd1602 didn't exist.");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        hd44780_init(&lcd1602);   //可运用该命令清屏且不影响后续显示，意思是：清屏后仍可持续调用hd44780_gotoxy()函数和hd44780_puts()函数。
        hd44780_switch_backlight(&lcd1602, true);

        hd44780_upload_character(&lcd1602, 0, centigrade);

        ESP_LOGV(TAG_LCD1602, "LCD1602 task is running ");
        if(xSemaphoreTake(uSemaphoreMutex_sensorData, portMAX_DELAY) == pdTRUE)
        {
            if(humidity == 0 && bmp280_air_pressure == 0){
                xSemaphoreGive(uSemaphoreMutex_sensorData);
                ESP_LOGD(TAG_LCD1602, "LCD1602 Display: Sensor didn't get data.");
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                continue;
            }
            
            snprintf(display_data, 17, "%s %5.1f %c", "temp:", temperature, '\x08');
            ESP_LOGD(TAG_LCD1602, "lcd data: %s", display_data);

            hd44780_gotoxy(&lcd1602, 0, 0);
            hd44780_puts(&lcd1602, display_data);


            snprintf(display_data + 16, 17, "%s %5.1f %s ", "aP:", bmp280_air_pressure/100, "hPa");
            ESP_LOGD(TAG_LCD1602, "lcd+16 data: %s", display_data + 16);

            hd44780_gotoxy(&lcd1602, 0, 1);
            hd44780_puts(&lcd1602, display_data + 16);

            xSemaphoreGive(uSemaphoreMutex_sensorData);

        }
                
        vTaskDelay(2000 / portTICK_PERIOD_MS);

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
    
    
    uSemaphoreMutex_sensorData = xSemaphoreCreateMutex(); //此互斥信号量用于避免传感器数据访问冲突。
    xSemaphoreGive(uSemaphoreMutex_sensorData);
    if(uSemaphoreMutex_sensorData != NULL)
    {
        ESP_LOGI(TAG_MAIN, "SemaphoreMutex_sensorData create succeed");
        // 以下是需要用到该互斥量的任务。
        xTaskCreate((void *)uTask_dht20_and_bmp280_sense, "SensorSenseData", configMINIMAL_STACK_SIZE * 8, NULL, 2, NULL);
        // xTaskCreate((void *)uTask_mqtt_post_to_onenet, "MQTT_PostToOneNet", configMINIMAL_STACK_SIZE * 8, NULL, 1, NULL);
        xTaskCreate((void *)uTask_mqtt_post_to_emqx, "MQTT_PostToEmqx", configMINIMAL_STACK_SIZE * 8, NULL, 1, NULL);

        xTaskCreate((void *)uTask_lcd1602_display_sensor_data, "LCD1602_Display", configMINIMAL_STACK_SIZE * 10, NULL, 1, NULL);
    }
    else ESP_LOGE(TAG_MAIN, "uSemaphoreMutex_sensorData Create Failed.");


}