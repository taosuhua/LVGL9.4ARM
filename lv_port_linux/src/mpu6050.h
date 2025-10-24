#ifndef __MPU6050_H
#define __MPU6050_H
#include "lvgl/lvgl.h"

#define I2C_DEV         "/dev/i2c-2"
#define MPU_ADDR        0x68
#define READ_PERIOD     30
#define CHART_POINTS    80
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
// Í³Ò»Ëõ·Å
#define ACC_SCALE   100   // g -> mg
#define ANG_SCALE   10    // deg -> centi-degree
#define TEMP_SCALE  100    // ¡ãC -> centi-degree

typedef struct {
    int fd;
    lv_obj_t* label;
    double ax_g, ay_g, az_g;
    double gx_dps, gy_dps, gz_dps;
    double temp_c;
}mpu_ctx_t;

void setup_mpu6050_and_ui(void);
void mpu6050_chart_display_ui(void);
void setup_mpu6050_chart_refresh(void);

#endif