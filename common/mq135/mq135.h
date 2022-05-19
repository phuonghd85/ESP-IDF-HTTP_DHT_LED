#ifndef MQ135_H_
#define MQ135_H_

#include "driver/gpio.h"

enum mq135_status {
    MQ135_CRC_ERROR = -2,
    MQ135_TIMEOUT_ERROR,
    MQ135_OK
};

struct mq135_reading {
    int status;
    float gasValue;
};

void MQ135_init(gpio_num_t gpio_num);

struct mq135_reading MQ135_read();

#endif