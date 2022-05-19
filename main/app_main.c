#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "http_server_app.h"
#include "output_iot.h"
#include "dht11.h"
#include "mq135.h"
#include "ledc_app.h"

#define EXAMPLE_ESP_WIFI_SSID      "2anhem"
#define EXAMPLE_ESP_WIFI_PASS      "boo112904"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static struct dht11_reading dht11_last_data, dht11_cur_data;
static struct mq135_reading mq135_last_data, mq135_cur_data;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
}

void switch_data_callback(char *data, int len)
{
    if(*data == '1'){
        output_io_set_level(GPIO_NUM_2, 1);
    }
    else if(*data == '0'){
        output_io_set_level(GPIO_NUM_2, 0);
    }
}

void slider_data_callback(char *data, int len)
{
    char number_str[10];
    memcpy(number_str, data, len + 1); // lấy data bỏ vào number_str --- lấy cả ký tự cuối cùng
    int duty = atoi(number_str);  // hàm chuyển string --> number
    printf("%d\n", duty);
    ledc_app_set_duty(0, duty);
}

void dht11_data_callback(void)
{
    char resp[100];
    sprintf(resp, "{\"temperature\": \"%.1f\", \"humidity\": \"%.1f\"}", dht11_last_data.temperature, dht11_last_data.humidity);
    dht11_response(resp, strlen(resp));
}

void mq135_data_callback(void)
{
    char res[100];
    sprintf(res, "{\"gasValue\": \"%.1f\"}", mq135_last_data.gasValue);
    mq135_response(res, strlen(res));
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    http_set_callback_switch(switch_data_callback);
    http_set_callback_dht11(dht11_data_callback);
    http_set_callback_mq135(mq135_data_callback);
    http_set_callback_slider(slider_data_callback);

    output_io_create(GPIO_NUM_2);
    DHT11_init(GPIO_NUM_4);
    MQ135_init(GPIO_NUM_21);
    ledc_app_init();
    //led dạng PWM
    ledc_app_add_pin(GPIO_NUM_5, 0); // channel 0

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    start_webserver();

    while(1) {
        dht11_cur_data = DHT11_read();
        if(dht11_cur_data.status == 0) //read OK
        {
            dht11_last_data = dht11_cur_data;
            // printf("temp: %.1f\n", dht11_last_data.temperature);
            // printf("humid: %.1f\n", dht11_last_data.humidity);
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);

        mq135_cur_data = MQ135_read();
        if(mq135_cur_data.status == 0) //read OK
        {
            mq135_last_data = mq135_cur_data;
            printf("gas: %.1f\n", mq135_last_data.gasValue);
            // printf("humid: %.1f\n", mq135_last_data.humidity);
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }

    // while(1) {
    //     mq135_cur_data = MQ135_read();
    //     if(mq135_cur_data.status == 0) //read OK
    //     {
    //         mq135_last_data = mq135_cur_data;
    //         // printf("temp: %.1f\n", mq135_last_data.temperature);
    //         // printf("humid: %.1f\n", mq135_last_data.humidity);
    //     }
    //     vTaskDelay(1000/portTICK_PERIOD_MS);
    // }
}
