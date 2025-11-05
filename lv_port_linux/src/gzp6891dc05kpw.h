#ifndef __GZP6891DC05KPW_H
#define __GZP6891DC05KPW_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>


#define I2C_DEV_GZP6891     "/dev/i2c-2"
#define GZP_ADDR            0x58
#define PMIN                -500.0
#define PMAX                500.0

typedef struct{
    int fd;
    uint32_t p_data;
    uint16_t t_data;
    double p_actual,t_actual;
}gzp_ctx_t;

void gzp6891dc_test(void);

#endif