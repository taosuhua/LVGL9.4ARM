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
#define PMIN                -5000.0
#define PMAX                5000.0
#define FIFO_SIZE           20        // FIFO缓冲区大小，可根据需要调整

typedef struct {
    double buffer[FIFO_SIZE];
    int index;
    int count;
    double sum;
} fifo_filter_t;

typedef struct{
    int fd;
    int32_t p_data;
    double p_shift;
    uint16_t t_data;
    double p_actual,t_actual,p_calibrated;
}gzp_ctx_t;

void gzp6891dc_test(void);

#endif