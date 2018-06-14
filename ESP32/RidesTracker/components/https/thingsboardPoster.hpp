#ifndef __THINGSBOARD_POSTER_H__
#define __THINGSBOARD_POSTER_H__

#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct  {
    struct timeval timestamp;
    uint32_t presure;
    float voltage;
} alive_report_item_t;

void reportAlive(alive_report_item_t &report);

#endif //THINGSBOARD_POSTER
