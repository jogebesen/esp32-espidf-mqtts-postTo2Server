#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "../../build/config/sdkconfig.h"
#include "onenet_dev_token.h"



#define DEV_TOKEN_VERISON_STR "2018-10-31"
#define DEV_TOKEN_SIG_METHOD_SHA256 "sha256"


const char *TAG_ONENET_DEV_TOKEN = "OneNet Device Token";


uint32_t onenet_dev_token_generate(uint8_t *token)
{
    uint8_t base64_data[64] = {0};
    uint8_t  str_for_sig[64] = { 0 };
    uint8_t  sign_buf[128]   = { 0 };
    uint32_t base64_data_len = sizeof(base64_data);
    uint32_t i               = 0;
    char* tmp             = NULL;

    unsigned int base64_decode_length = 0;
    unsigned int base64_encode_length = 0;

    char *device_token;
    device_token = (char *)token;
    
    sprintf(device_token, (const char *)"version=%s", DEV_TOKEN_VERISON_STR);

    sprintf(device_token + strlen(device_token), (const char*)"&res=products%%2F%s%%2Fdevices%%2F%s", CONFIG_ONENET_MQTT_PRODUCT_ID, CONFIG_ONENET_MQTT_DEVICE_USERNAME);

    sprintf(device_token + strlen(device_token), (const char*)"&et=%u", CONFIG_ONENET_MQTT_EXPIRE_TIME_IN_UNIX_TIMESTAMP_FORMAT);

    ESP_LOGD(TAG_ONENET_DEV_TOKEN, "accessKey-NonDecode: %s", CONFIG_ONENET_DEVICE_ACCESS_KEY);

    mbedtls_base64_decode(base64_data, base64_data_len, &base64_decode_length, (unsigned char *)&CONFIG_ONENET_DEVICE_ACCESS_KEY, strlen(CONFIG_ONENET_DEVICE_ACCESS_KEY));

    ESP_LOGD(TAG_ONENET_DEV_TOKEN, "base64_data decode_num: %u ---base64Decoded: %s",base64_decode_length, base64_data);

    sprintf(device_token + strlen(device_token), (const char*)"&method=%s", DEV_TOKEN_SIG_METHOD_SHA256);

    sprintf((char *)str_for_sig, (const char*)"%u\n%s\nproducts/%s/devices/%s\n%s", CONFIG_ONENET_MQTT_EXPIRE_TIME_IN_UNIX_TIMESTAMP_FORMAT, DEV_TOKEN_SIG_METHOD_SHA256, CONFIG_ONENET_MQTT_PRODUCT_ID, CONFIG_ONENET_MQTT_DEVICE_USERNAME, DEV_TOKEN_VERISON_STR);


    ESP_LOGD(TAG_ONENET_DEV_TOKEN, "string for sign: %s", str_for_sig);

    mbedtls_md_context_t sha_ctx;
    mbedtls_md_init(&sha_ctx);

    do
    {
        if(mbedtls_md_setup(&sha_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) != 0){
            mbedtls_md_free(&sha_ctx);
            ESP_LOGD(TAG_ONENET_DEV_TOKEN, "mbedtls_md_setup() failed.");
            break;
        }

        if(mbedtls_md_hmac_starts(&sha_ctx, base64_data, base64_data_len) != 0){
            mbedtls_md_free(&sha_ctx);
            ESP_LOGD(TAG_ONENET_DEV_TOKEN, "mbedtls_md_hmac_starts() error.");
            break;
        }

        if(mbedtls_md_hmac_update(&sha_ctx, str_for_sig, strlen((char *)str_for_sig)) != 0){
            mbedtls_md_free(&sha_ctx);
            ESP_LOGD(TAG_ONENET_DEV_TOKEN, "mbedtls_md_hmac_update() error.");
            break;
        }

        if(mbedtls_md_hmac_finish(&sha_ctx, sign_buf) != 0){
            mbedtls_md_free(&sha_ctx);
            ESP_LOGD(TAG_ONENET_DEV_TOKEN, "mbedtls_md_hmac_finish() error.");
            break;
        }

    } while (0);

    mbedtls_md_free(&sha_ctx);
    

    memset(base64_data, 0, sizeof(base64_data));
    base64_data_len = sizeof(base64_data);


    mbedtls_base64_encode(base64_data, base64_data_len, &base64_encode_length, sign_buf, 32);
    ESP_LOGD(TAG_ONENET_DEV_TOKEN, "base64Data encode_num:%u  Recoded: %s",base64_encode_length, base64_data);

    strcat(device_token, (const char*)"&sign=");
    tmp = device_token + strlen(device_token);

    for (i = 0; i < base64_data_len; i++) {
        switch (base64_data[i]) {
            case '+':
                strcat(tmp, (const char*)"%2B");
                tmp += 3;
                break;
            case ' ':
                strcat(tmp, (const char*)"%20");
                tmp += 3;
                break;
            case '/':
                strcat(tmp, (const char*)"%2F");
                tmp += 3;
                break;
            case '?':
                strcat(tmp, (const char*)"%3F");
                tmp += 3;
                break;
            case '%':
                strcat(tmp, (const char*)"%25");
                tmp += 3;
                break;
            case '#':
                strcat(tmp, (const char*)"%23");
                tmp += 3;
                break;
            case '&':
                strcat(tmp, (const char*)"%26");
                tmp += 3;
                break;
            case '=':
                strcat(tmp, (const char*)"%3D");
                tmp += 3;
                break;
            default:
                *tmp = base64_data[i];
                tmp += 1;
                break;
        }
    }

    ESP_LOGI(TAG_ONENET_DEV_TOKEN, "ONENET DEVICE TOKEN: %s", token);

    return 0;
}

