#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <driver/gpio.h>
#include "driver/rtc_io.h"

class Button {
    private:
        gpio_num_t _pin;
    public:
        Button(gpio_num_t pin): _pin(pin) {     
            gpio_pullup_dis(_pin);
	        gpio_pulldown_en(_pin);
            gpio_set_direction(_pin, GPIO_MODE_INPUT);
        };
        void enable(void) { gpio_set_direction(_pin, GPIO_MODE_OUTPUT); } 
        void disable(void) { rtc_gpio_isolate(_pin); } 
        int getValue(void) { return gpio_get_level(_pin); }
};

#endif /* __BUTTON_H__ */