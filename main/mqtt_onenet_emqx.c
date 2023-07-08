#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <mqtt_client.h>
#include <esp_log.h>

#include "esp_log.h"

#include "../build/config/sdkconfig.h"

#include "mqtt_onenet_emqx.h"

// OneNet平台相关的MQTT宏定义
#define SUB_TOPIC_ONET_DP_RESULT    "$sys/" CONFIG_ONENET_MQTT_PRODUCT_ID "/" CONFIG_ONENET_MQTT_DEVICE_USERNAME "/dp/post/json/+"
#define SUB_TOPIC_ONET_DP_ACCEPT    "$sys/" CONFIG_ONENET_MQTT_PRODUCT_ID "/" CONFIG_ONENET_MQTT_DEVICE_USERNAME "/dp/post/accepted"
#define SUB_TOPIC_ONET_DP_REJECT    "$sys/" CONFIG_ONENET_MQTT_PRODUCT_ID "/" CONFIG_ONENET_MQTT_DEVICE_USERNAME "/dp/post/rejected"
#define SUB_TOPIC_ONET_CMD_RESULT   "$sys/" CONFIG_ONENET_MQTT_PRODUCT_ID "/" CONFIG_ONENET_MQTT_DEVICE_USERNAME "/cmd/#"

#define PUB_TOPIC_ONET_DATA_UP      "$sys/" CONFIG_ONENET_MQTT_PRODUCT_ID "/" CONFIG_ONENET_MQTT_DEVICE_USERNAME "/dp/post/json"

#define PUB_ONET_DATA_FORMAT        "{\"id\":%d,\"dp\":%s}"



#if CONFIG_ONENET_CONNECTION_VIA_MQTTS == 1
static const uint8_t *MQTTS_certificate_pem_start = (const uint8_t*)"-----BEGIN CERTIFICATE-----\r\n"
                                                   "MIIDOzCCAiOgAwIBAgIJAPCCNfxANtVEMA0GCSqGSIb3DQEBCwUAMDQxCzAJBgNV\r\n"
                                                   "BAYTAkNOMQ4wDAYDVQQKDAVDTUlPVDEVMBMGA1UEAwwMT25lTkVUIE1RVFRTMB4X\r\n"
                                                   "DTE5MDUyOTAxMDkyOFoXDTQ5MDUyMTAxMDkyOFowNDELMAkGA1UEBhMCQ04xDjAM\r\n"
                                                   "BgNVBAoMBUNNSU9UMRUwEwYDVQQDDAxPbmVORVQgTVFUVFMwggEiMA0GCSqGSIb3\r\n"
                                                   "DQEBAQUAA4IBDwAwggEKAoIBAQC/VvJ6lGWfy9PKdXKBdzY83OERB35AJhu+9jkx\r\n"
                                                   "5d4SOtZScTe93Xw9TSVRKrFwu5muGgPusyAlbQnFlZoTJBZY/745MG6aeli6plpR\r\n"
                                                   "r93G6qVN5VLoXAkvqKslLZlj6wXy70/e0GC0oMFzqSP0AY74icANk8dUFB2Q8usS\r\n"
                                                   "UseRafNBcYfqACzF/Wa+Fu/upBGwtl7wDLYZdCm3KNjZZZstvVB5DWGnqNX9HkTl\r\n"
                                                   "U9NBMS/7yph3XYU3mJqUZxryb8pHLVHazarNRppx1aoNroi+5/t3Fx/gEa6a5PoP\r\n"
                                                   "ouH35DbykmzvVE67GUGpAfZZtEFE1e0E/6IB84PE00llvy3pAgMBAAGjUDBOMB0G\r\n"
                                                   "A1UdDgQWBBTTi/q1F2iabqlS7yEoX1rbOsz5GDAfBgNVHSMEGDAWgBTTi/q1F2ia\r\n"
                                                   "bqlS7yEoX1rbOsz5GDAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAL\r\n"
                                                   "aqJ2FgcKLBBHJ8VeNSuGV2cxVYH1JIaHnzL6SlE5q7MYVg+Ofbs2PRlTiWGMazC7\r\n"
                                                   "q5RKVj9zj0z/8i3ScWrWXFmyp85ZHfuo/DeK6HcbEXJEOfPDvyMPuhVBTzuBIRJb\r\n"
                                                   "41M27NdIVCdxP6562n6Vp0gbE8kN10q+ksw8YBoLFP0D1da7D5WnSV+nwEIP+F4a\r\n"
                                                   "3ZX80bNt6tRj9XY0gM68mI60WXrF/qYL+NUz+D3Lw9bgDSXxpSN8JGYBR85BxBvR\r\n"
                                                   "NNAhsJJ3yoAvbPUQ4m8J/CoVKKgcWymS1pvEHmF47pgzbbjm5bdthlIx+swdiGFa\r\n"
                                                   "WzdhzTYwVkxBaU+xf/2w\r\n"
                                                   "-----END CERTIFICATE-----";

#endif  //CONFIG_ONENET_CONNECTION_VIA_MQTTS



// EMQX平台相关的MQTT宏定义
#define EMQX_SUB_TOPIC_CMD           "/topic/309/cmd"
#define EMQX_SUB_TOPIC_DATA          "/topic/309/data"
#define EMQX_PUB_TOPIC_DATA          "/topic/309/post"


const char *TAG_ONENET = "mqtt_to_onenet";
const char *TAG_EMQX = "mqtt_to_emqx";



static void mqtt_onenet_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_ONENET, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, SUB_TOPIC_ONET_DP_RESULT, 1);
        ESP_LOGI(TAG_ONENET, "TOPIC: SUB_DP_RESULT: %s ", SUB_TOPIC_ONET_DP_RESULT);
        ESP_LOGI(TAG_ONENET, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, SUB_TOPIC_ONET_CMD_RESULT, 1);
        ESP_LOGI(TAG_ONENET, "TOPIC: SUB_CMD_RESULT: %s", SUB_TOPIC_ONET_CMD_RESULT);
        ESP_LOGI(TAG_ONENET, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_ONENET, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG_ONENET, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG_ONENET, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG_ONENET, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG_ONENET, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG_ONENET, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG_ONENET, "Other event id:%d", event->event_id);
        break;
    }
}


static void mqtt_emqx_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_EMQX, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, EMQX_SUB_TOPIC_CMD, 0);
        ESP_LOGI(TAG_EMQX, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, EMQX_SUB_TOPIC_DATA, 0);
        ESP_LOGI(TAG_EMQX, "sent subscribe successful, msg_id=%d", msg_id);
        

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_DATA");
        printf("EMQX TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("EMQX DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_EMQX, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG_EMQX, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG_EMQX, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG_EMQX, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG_EMQX, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG_EMQX, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG_EMQX, "Other event id:%d", event->event_id);
        break;
    }
}




void uFunc_mqtt_send2onenet_init(esp_mqtt_client_handle_t *mqtt_onenet_client)
{
    char URI[50] = {0};
    sprintf(URI, "%s%s", "mqtts://", CONFIG_ONENET_MQTT_BROKER_ADDR_AND_PORT);
    const esp_mqtt_client_config_t mqtt_onenet_cfg = {
        .broker.address.uri = URI,
        .broker.verification.certificate = (const char *)MQTTS_certificate_pem_start,
        .broker.verification.skip_cert_common_name_check = true,
        .credentials.client_id = CONFIG_ONENET_MQTT_DEVICE_USERNAME,
        .credentials.username = CONFIG_ONENET_MQTT_PRODUCT_ID,
        .credentials.authentication.password = CONFIG_ONENET_MQTT_CLIENT_TOKEN,        
    };

    *mqtt_onenet_client = esp_mqtt_client_init(&mqtt_onenet_cfg);
    esp_mqtt_client_register_event(*mqtt_onenet_client, ESP_EVENT_ANY_ID, mqtt_onenet_event_handler, NULL);
    esp_mqtt_client_start(*mqtt_onenet_client);    
}


void uFunc_mqtt_onenet_post_data(esp_mqtt_client_handle_t *mqtt_onenet_client, float temperature, float humidity, float bmp280_air_pressure, float bmp280_temp)
{
    char data_pack[178] = {0};
    char data_body[128] = {0};

    int msg_onet_id = 001;

    sprintf(data_body, "{\"temperature\":[{\"v\":%0.1f}],\"humidity\":[{\"v\":%0.2f}], \"air_pressure_hPa\":[{\"v\":%0.3f}], \"bmp280_temp\":[{\"v\":%0.1f}]}", temperature, humidity, bmp280_air_pressure/100, bmp280_temp);
    sprintf(data_pack, PUB_ONET_DATA_FORMAT, msg_onet_id, data_body); 

    esp_mqtt_client_publish(*mqtt_onenet_client, PUB_TOPIC_ONET_DATA_UP, data_pack, strlen(data_pack), 1, 0);

}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*  emqx settings and function
*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void uFunc_mqtt_send2emqx_init(esp_mqtt_client_handle_t *mqtt_emqx_client)
{
    char URI[50] = {0};
    sprintf(URI, "%s%s", "mqtt://", CONFIG_EMQX_MQTT_URI);
    const esp_mqtt_client_config_t mqtt_emqx_cfg = {
        .broker.address.uri = URI,
        .credentials.username = CONFIG_EMQX_CLIENT_USERNAME,
        .credentials.client_id = CONFIG_EMQX_CLIENT_ID,
        .credentials.authentication.password = CONFIG_EMQX_CLIENT_PASSWD,
    };

    *mqtt_emqx_client = esp_mqtt_client_init(&mqtt_emqx_cfg);
    esp_mqtt_client_register_event(*mqtt_emqx_client, ESP_EVENT_ANY_ID, mqtt_emqx_event_handler, NULL);
    esp_mqtt_client_start(*mqtt_emqx_client);
}


void uFunc_mqtt_emqx_post_data(esp_mqtt_client_handle_t *mqtt_emqx_client, float temperature, float humidity, float bmp280_air_pressure, float bmp280_temp)
{
    char data_pack[128];

    sprintf(data_pack, "temperature: %.2f, humidity: %.2f, air_pressure: %.3f \r\n", temperature, humidity, bmp280_air_pressure);

    esp_mqtt_client_publish(*mqtt_emqx_client, EMQX_PUB_TOPIC_DATA, data_pack, strlen(data_pack), 1, 0);
}





