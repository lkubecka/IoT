class Notifier {
    private:
        gpio_num_t _pin;
        bool _state;
    public:
        Notifier(gpio_num_t pin): _pin(pin) {_state = false; }
        void enable(void) { gpio_set_direction(_pin, GPIO_MODE_OUTPUT); } 
        void disable(void) { rtc_gpio_isolate(_pin); } 
        void setLow(void) { gpio_set_level(_pin, _state = false); }
        void setHigh(void) { gpio_set_level(_pin, _state = true); }
        void toggle(void) { gpio_set_level(_pin, _state = !_state); }
};

