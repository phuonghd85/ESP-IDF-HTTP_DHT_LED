#include "esp_timer.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mq135.h"

static gpio_num_t mq135_gpio;
static int64_t last_read_time = -2000000;
static struct mq135_reading last_read;

static int _waitOrTimeout(uint16_t microSeconds, int level){
    int micros_ticks = 0;
    while(gpio_get_level(mq135_gpio) == level){
        if(micros_ticks++ > microSeconds){
            return MQ135_TIMEOUT_ERROR;
        }
        ets_delay_us(1);
    }
    return micros_ticks;
}

static int _checkCRC(uint8_t data[]){
    if(data[2] == (data[0] + data[1])){
        return MQ135_OK;
    }
    else{
        return MQ135_CRC_ERROR;
    }
}

static void _sendStartSignal(){
    gpio_set_direction(mq135_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(mq135_gpio, 0);
    ets_delay_us(20 * 1000);
    gpio_set_level(mq135_gpio, 1);
    ets_delay_us(40);
    gpio_set_direction(mq135_gpio, GPIO_MODE_INPUT);
}

static int _checkResponse() {
    // wait for next stop 80us
    if(_waitOrTimeout(80, 0) == MQ135_TIMEOUT_ERROR){
        return MQ135_TIMEOUT_ERROR;
    }
    // wait for next stop 80us
    if(_waitOrTimeout(80, 1) == MQ135_TIMEOUT_ERROR){
        return MQ135_TIMEOUT_ERROR;
    }
    return MQ135_OK;
}

static struct mq135_reading _timeoutError() {
    struct mq135_reading timeoutError = {MQ135_TIMEOUT_ERROR, -1};
    return timeoutError;
}

static struct mq135_reading _crcError() {
    struct mq135_reading crcError = {MQ135_CRC_ERROR, -1};
    return crcError;
}

void MQ135_init(gpio_num_t gpio_num){
    // wait 1s to make device pass its initial unstable status
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    mq135_gpio = gpio_num;
}

struct mq135_reading MQ135_read() {
    // 
    if(esp_timer_get_time() - 2000000 < last_read_time){
        return last_read;
    }

    last_read_time = esp_timer_get_time();

    uint8_t data[3] = {0, 0, 0};

    _sendStartSignal();

    if(_checkResponse() == MQ135_TIMEOUT_ERROR){
        return last_read = _timeoutError();
    }
    // read response
    for(int i = 0; i < 40; i++){
        //initial data
        if(_waitOrTimeout(50, 0) == MQ135_TIMEOUT_ERROR){
            return last_read = _timeoutError();
        }

        if(_waitOrTimeout(70, 1) > 28){
            // bit recv = 1
            data[i/8] |= (1 << (7 - (i%8)));
        }
    }

    if(_checkCRC(data) != MQ135_CRC_ERROR){
        //
        last_read.status = MQ135_OK;

        last_read.gasValue = data[0];
        if(data[3] & 0x80) {
            last_read.gasValue = -1 - last_read.gasValue;
        }
        last_read.gasValue += (data[1] & 0x0f) * 0.1;
        return last_read;
    }
    else{
        return last_read = _crcError();
    }

}