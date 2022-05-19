#ifndef __HTTP_SERVER_APP_H
#define __HTTP_SERVER_APP_H
//#include "stdint.h" //-- thư viện cho uint8_t....

typedef void (*http_post_callback_t) (char *data, int len);
typedef void (*http_get_callback_t) (void);

void start_webserver(void);
void stop_webserver(void) ;
void http_set_callback_switch(void *cb);
void http_set_callback_dht11(void *cb);
void http_set_callback_mq135(void *cb);
void dht11_response(char *data, int len);
void mq135_response(char *data, int len);
void http_set_callback_slider(void *cb);

#endif