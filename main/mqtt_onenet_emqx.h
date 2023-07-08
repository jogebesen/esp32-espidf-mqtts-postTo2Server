#ifndef __MQTT_SEND_TO_2SVR__
#define __MQTT_SEND_TO_2SVR__
#include <mqtt_client.h>

void uFunc_mqtt_send2onenet_init(esp_mqtt_client_handle_t *mqtt_onenet_client);
void uFunc_mqtt_onenet_post_data(esp_mqtt_client_handle_t *mqtt_onenet_client, float temperature, float humidity, float bmp280_air_pressure, float bmp280_temp);
void uFunc_mqtt_send2emqx_init(esp_mqtt_client_handle_t *mqtt_emqx_client);
void uFunc_mqtt_emqx_post_data(esp_mqtt_client_handle_t *mqtt_emqx_client, float temperature, float humidity, float bmp280_air_pressure, float bmp280_temp);

void utask_mqtt_send_to_onenet_and_emqx(void *pvParameters);


#endif  // __MQTT_SEND_TO_2SVR__

