
#ifndef __ALTIMETER_H__
#define __ALTIMETER_H__

#ifdef __cplusplus
extern "C" {

#endif
#include <driver/gpio.h>

const gpio_num_t SDA_GPIO = GPIO_NUM_16;  //19
const gpio_num_t SCL_GPIO = GPIO_NUM_17;  //18


void initAltimeter(void);
void getAltitude(uint32_t *pressure);


#ifdef __cplusplus
}
#endif

#endif /* __ALTIMETER_H__ */