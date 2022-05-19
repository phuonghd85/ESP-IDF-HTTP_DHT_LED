#ifndef LEDC_APP_H
#define LEDC_APP_H
#include "esp_err.h"
#include "hal/gpio_types.h"

void ledc_app_init(void);
void ledc_app_add_pin(int pin, int channel);
void ledc_app_set_duty(int channel, int duty);

#endif