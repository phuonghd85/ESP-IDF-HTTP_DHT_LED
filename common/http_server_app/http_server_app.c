#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>
#include "http_server_app.h"
#include "stdint.h"

static const char *TAG = "HTTP_SERVER";
static httpd_handle_t server = NULL;
static httpd_req_t *REQ;
static httpd_req_t *REQ1;


// nhúng file index.html
extern const uint8_t index_html_start[] asm("_binary_index_html_start");  
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static http_get_callback_t http_get_dht11_callback = NULL;
static http_get_callback_t http_get_mq135_callback = NULL;
static http_post_callback_t http_post_switch_callback = NULL;
static http_post_callback_t http_post_slider_callback = NULL;

//--------------------------------------------------------------------------
/* An HTTP POST handler */
static esp_err_t data_post_handler(httpd_req_t *req)
{
    char buf[100];
    /* Read the data for the request */
    httpd_req_recv(req, buf, req->content_len); // đọc data
    printf("DATA: %s\n", buf);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t post_data = {
    .uri       = "/data",
    .method    = HTTP_POST,
    .handler   = data_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

//-----------------------------POST SWITCH----------------------------------------
/* An HTTP POST handler ---- POST for SWITCH */
static esp_err_t switch_post_handler(httpd_req_t *req)
{
    char buf[100];
    /* Read the data for the request */
    httpd_req_recv(req, buf, req->content_len); // đọc data
    http_post_switch_callback(buf, req->content_len);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t switch_post_data = {
    .uri       = "/switch1",
    .method    = HTTP_POST,
    .handler   = switch_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

//--------------------------POST SLIDER--------------------------------------------
/* An HTTP POST handler ---- POST for SLIDER */
static esp_err_t slider_post_handler(httpd_req_t *req)
{
    char buf[100];
    /* Read the data for the request */
    httpd_req_recv(req, buf, req->content_len); // đọc data
    http_post_slider_callback(buf, req->content_len);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t slider_post_data = {
    .uri       = "/slider",
    .method    = HTTP_POST,
    .handler   = slider_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

//---------------------------------------------------------------------------------
/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    // const char* resp_str = (const char*) "Hello World!";//req->user_ctx
    httpd_resp_set_type(req, "text/html"); //kiểu / . gì?
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start); // truyền vào địa chỉ chuỗi (index_html_start), độ lớn của chuỗi(index_html_end - index_html_start)
    //httpd_resp_set_type(req, "image/jpg");
    //httpd_resp_send(req, (const char*) index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

static const httpd_uri_t get_dht11 = {
    .uri       = "/dht11",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

// hàm phản hồi lại dữ liệu dht11
void dht11_response(char *data, int len)
{
    httpd_resp_send(REQ, data, len);
}

// hàm phản hồi lại dữ liệu mq135
void mq135_response(char *data, int len)
{
    httpd_resp_send(REQ1, data, len);
}
//-------------------------------------------------------------------------------------
/* An HTTP GET handler - cứ 1s gửi temp và humd lên web */
static esp_err_t dht11_get_handler(httpd_req_t *req)
{
    // const char* resp_str = (const char*) "{\"temperature\": \"25\", \"humidity\": \"80\"}";
    // httpd_resp_send(req, resp_str, strlen(resp_str)); 
    REQ = req;
    http_get_dht11_callback();
    return ESP_OK;
}

static const httpd_uri_t get_data_dht11 = {
    .uri       = "/getdatadht11",
    .method    = HTTP_GET,
    .handler   = dht11_get_handler,
    .user_ctx  = NULL
};

//-------------------------------------------------------------------------------------
/* An HTTP GET handler - cứ 1s gửi gasValue lên web */
static esp_err_t mq135_get_handler(httpd_req_t *req)
{
    // const char* resp_str = (const char*) "{\"temperature\": \"25\", \"humidity\": \"80\"}";
    // httpd_resp_send(req, resp_str, strlen(resp_str)); 
    REQ1 = req;
    http_get_mq135_callback();
    return ESP_OK;
}

static const httpd_uri_t get_data_mq135 = {
    .uri       = "/getdatamq135",
    .method    = HTTP_GET,
    .handler   = mq135_get_handler,
    .user_ctx  = NULL
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/dht11", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/dht11 URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } 
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &post_data);
        httpd_register_uri_handler(server, &switch_post_data);
        httpd_register_uri_handler(server, &slider_post_data);
        httpd_register_uri_handler(server, &get_dht11);
        httpd_register_uri_handler(server, &get_data_dht11);
        httpd_register_uri_handler(server, &get_data_mq135);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else{
        ESP_LOGI(TAG, "Error starting server!");
    }    
}

void stop_webserver(void) //httpd_handle_t server
{
    // Stop the httpd server
    httpd_stop(server);
}

void http_set_callback_switch(void *cb) {
    http_post_switch_callback = cb;
}

void http_set_callback_dht11(void *cb) {
    http_get_dht11_callback = cb;
}

void http_set_callback_mq135(void *cb) {
    http_get_mq135_callback = cb;
}

void http_set_callback_slider(void *cb) {
    http_post_slider_callback = cb;
}