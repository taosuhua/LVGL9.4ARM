#include "mpu6050.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "lvgl/lvgl.h"
#include <math.h>

/**** GLOBAL VARIABLES ****/
static mpu_ctx_t g_ctx = {0};
static lv_style_t chart_style;
static lv_obj_t *acc_chart,*gyo_chart,*temp_chart;
static lv_chart_series_t *ser_ax,*ser_ay,*ser_az;
static lv_chart_series_t *ser_roll, *ser_pitch;
static lv_chart_series_t *ser_temp;
/**************************/
static int16_t be16(uint8_t hi, uint8_t lo){
    return (int16_t)((hi << 8) | lo);
}


static int mpu6050_write_reg(int fd, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    if(write(fd,buf,2) == 2){
        return 0;
    }
    return -1;
}

static int mpu6050_read_block(int fd, uint8_t start_reg,uint8_t *buf,int len)
{
    if(write(fd,&start_reg,1) != 1){
        return -1;
    }
    if(read(fd,buf,len) != len){
        return -2;
    }
    return 0;
}

static int mpu6050_init(mpu_ctx_t *ctx)
{
    ctx->fd = open(I2C_DEV,O_RDWR);
    if(ctx->fd < 0){
        perror("open i2c mpu6050 failed.\n");
        return -1;
    }
    if(ioctl(ctx->fd,I2C_SLAVE,MPU_ADDR) < 0){
        perror("I2C_SLAVE\n");
        return -2;
    }
    //wakeup
    if(mpu6050_write_reg(ctx->fd,0x6B,0x00) < 0)    return -3;  //PWR_MGMT_1 = 0
    usleep(10000);
    if(mpu6050_write_reg(ctx->fd,0x1A,0x03) < 0)    return -4;  //DLPF
    if(mpu6050_write_reg(ctx->fd,0x1B,0x00) < 0)    return -5;  //GYRO ±250 dps
    if(mpu6050_write_reg(ctx->fd,0x1C,0x00) < 0)    return -6;  // ACC ±2 g
    return 0;
}

static int mpu6050_read_all(mpu_ctx_t* ctx){
    uint8_t b[14];
    if(mpu6050_read_block(ctx->fd,0x3B,b,14) != 0){
        return -1;
    }

    int16_t ax = be16(b[0],b[1]);
    int16_t ay = be16(b[2], b[3]);
    int16_t az = be16(b[4], b[5]);
    int16_t t  = be16(b[6], b[7]);
    int16_t gx = be16(b[8], b[9]);
    int16_t gy = be16(b[10], b[11]);
    int16_t gz = be16(b[12], b[13]);

    ctx->ax_g = ax / 16384.0;     // ±2g：16384 LSB/g
    ctx->ay_g = ay / 16384.0;
    ctx->az_g = az / 16384.0;
    ctx->gx_dps = gx / 131.0;     // ±250 dps：131 LSB/(°/s)
    ctx->gy_dps = gy / 131.0;
    ctx->gz_dps = gz / 131.0;
    ctx->temp_c = t / 340.0 + 36.53;

    return 0;
}

static void sensor_timer_cb(lv_timer_t* timer){
    mpu_ctx_t* ctx = (mpu_ctx_t*)lv_timer_get_user_data(timer);
    if(!ctx) return;

    if(mpu6050_read_all(ctx) == 0){
        char line[160];
        snprintf(line,sizeof(line),
                   "ACC[g]:  X=%+.3f  Y=%+.3f  Z=%+.3f\n"
                 "GYR[dps]:X=%+.2f  Y=%+.2f  Z=%+.2f\n"
                 "TEMP: %.2f °C",
                 ctx->ax_g, ctx->ay_g, ctx->az_g,
                 ctx->gx_dps, ctx->gy_dps, ctx->gz_dps,
                 ctx->temp_c);
        lv_label_set_text(ctx->label,line);
    }
}

void setup_mpu6050_and_ui(void){

    if(mpu6050_init(&g_ctx) != 0){
        printf("MPU6050_INITILIZE_FAILED.\n");
        return;
    }

    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
    lv_obj_t* obj = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(obj,lv_color_hex(0x0000FF),0);
    lv_obj_set_size(obj,800,400);
    lv_obj_center(obj);

    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn,200,100);
    lv_obj_align_to(btn,obj,LV_ALIGN_OUT_BOTTOM_RIGHT,0,0);
    
    g_ctx.label = lv_label_create(obj);
    lv_obj_center(g_ctx.label);
    lv_obj_set_style_text_color(g_ctx.label,lv_color_white(),0);
    lv_obj_set_style_text_font(g_ctx.label,&lv_font_montserrat_20,0);
    lv_label_set_long_mode(g_ctx.label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(g_ctx.label, "Reading MPU6050...");
    lv_obj_align(g_ctx.label, LV_ALIGN_TOP_LEFT, 8, 8);

    // 3) 创建一个 LVGL 定时器每 50ms 采样并更新文本
    // lv_timer_create(sensor_timer_cb, READ_PERIOD, &g_ctx);
    lv_timer_t* t = lv_timer_create(sensor_timer_cb,READ_PERIOD,NULL);
    lv_timer_set_user_data(t,&g_ctx);
}

static void chart_timer_cb(lv_timer_t *t)
{
    mpu_ctx_t *ctx = (mpu_ctx_t*)lv_timer_get_user_data(t);
    if(!ctx) return;

    if(mpu6050_read_all(ctx) != 0) return;

    // 加速度
    lv_chart_set_next_value(acc_chart, ser_ax, (lv_coord_t)(ctx->ax_g * ACC_SCALE));
    lv_chart_set_next_value(acc_chart, ser_ay, (lv_coord_t)(ctx->ay_g * ACC_SCALE));
    lv_chart_set_next_value(acc_chart, ser_az, (lv_coord_t)(ctx->az_g * ACC_SCALE));

    // 倾角（度→centi-degree）
    double ax = ctx->ax_g, ay = ctx->ay_g, az = ctx->az_g;
    double roll_deg  = atan2(ay, az) * 180.0 / M_PI;
    double pitch_deg = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    lv_chart_set_next_value(gyo_chart, ser_roll,  (lv_coord_t)(roll_deg  * ANG_SCALE));
    lv_chart_set_next_value(gyo_chart, ser_pitch, (lv_coord_t)(pitch_deg * ANG_SCALE));

    // 温度
    lv_chart_set_next_value(temp_chart, ser_temp, (lv_coord_t)(ctx->temp_c * TEMP_SCALE));
}

void mpu6050_chart_display_ui(void)
{
    //define default style
    lv_style_init(&chart_style);
    lv_style_set_radius(&chart_style,0);
    lv_style_set_border_width(&chart_style,0);
    lv_style_set_pad_all(&chart_style,0);
    lv_style_set_line_width(&chart_style,0);

    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
    acc_chart = lv_chart_create(lv_scr_act());
    lv_obj_set_height(acc_chart,413);
    gyo_chart = lv_chart_create(lv_scr_act());
    lv_obj_set_height(gyo_chart,413);
    temp_chart = lv_chart_create(lv_scr_act());
    lv_obj_set_height(temp_chart,414);
    //set default style
    lv_obj_add_style(acc_chart,&chart_style,0);
    lv_obj_add_style(gyo_chart,&chart_style,0);
    lv_obj_add_style(temp_chart,&chart_style,0);
    //set char alignment
    lv_obj_align(acc_chart,LV_ALIGN_TOP_MID,0,0);
    lv_obj_align_to(gyo_chart,acc_chart,LV_ALIGN_OUT_BOTTOM_MID,0,0);
    lv_obj_align_to(temp_chart,gyo_chart,LV_ALIGN_OUT_BOTTOM_MID,0,0);
    //set char background color
    lv_obj_set_style_bg_color(acc_chart,lv_color_hex(0x101010),LV_PART_MAIN);
    lv_obj_set_style_bg_color(gyo_chart,lv_color_hex(0x151515),LV_PART_MAIN);
    lv_obj_set_style_bg_color(temp_chart,lv_color_hex(0x1A1A1A),LV_PART_MAIN);
    //accelerate chart
    lv_chart_set_type(acc_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(acc_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(acc_chart, CHART_POINTS);
    lv_chart_set_range(acc_chart, LV_CHART_AXIS_PRIMARY_Y, -2*ACC_SCALE, 2*ACC_SCALE);
    ser_ax = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser_ay = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    ser_az = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(acc_chart,0,0,LV_PART_INDICATOR);
    //gyo chart
    lv_chart_set_type(gyo_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(gyo_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(gyo_chart, CHART_POINTS);
    lv_chart_set_range(gyo_chart, LV_CHART_AXIS_PRIMARY_Y, -90*ANG_SCALE, 90*ANG_SCALE);
    ser_roll  = lv_chart_add_series(gyo_chart, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
    ser_pitch = lv_chart_add_series(gyo_chart, lv_palette_main(LV_PALETTE_PURPLE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(gyo_chart,0,0,LV_PART_INDICATOR);
    //temperature chart
    lv_chart_set_type(temp_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(temp_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(temp_chart, CHART_POINTS);
    lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 60*TEMP_SCALE);
    ser_temp = lv_chart_add_series(temp_chart, lv_palette_main(LV_PALETTE_TEAL), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(temp_chart,0,0,LV_PART_INDICATOR);

}

void setup_mpu6050_chart_refresh(void){
    if(mpu6050_init(&g_ctx) != 0){
        printf("MPU6050_INITILIZE_FAILED.\n");
        return;
    }
    mpu6050_chart_display_ui();

    lv_timer_t* tmr = lv_timer_create(chart_timer_cb,READ_PERIOD,&g_ctx);
    lv_timer_set_user_data(tmr,&g_ctx);
}
