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

// Canvas 相关
#define CANVAS_WIDTH  1080
#define CANVAS_HEIGHT 413
#define MAX_POINTS    200

static lv_obj_t *acc_canvas, *gyo_canvas, *temp_canvas;
static lv_color_t acc_buf[CANVAS_WIDTH * CANVAS_HEIGHT];
static lv_color_t gyo_buf[CANVAS_WIDTH * CANVAS_HEIGHT];
static lv_color_t temp_buf[CANVAS_WIDTH * CANVAS_HEIGHT];

// 数据缓冲区
static int16_t acc_x_data[MAX_POINTS] = {0};
static int16_t acc_y_data[MAX_POINTS] = {0};
static int16_t acc_z_data[MAX_POINTS] = {0};
static int16_t roll_data[MAX_POINTS] = {0};
static int16_t pitch_data[MAX_POINTS] = {0};
static int16_t temp_data[MAX_POINTS] = {0};

static mpu_ctx_t data_buffer;
static volatile int buffer_ready = 0;
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
    lv_obj_set_size(obj,1080,400);
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

    lv_timer_t* t = lv_timer_create(sensor_timer_cb,READ_PERIOD,NULL);
    lv_timer_set_user_data(t,&g_ctx);
}

static void fast_sample_cb(lv_timer_t *t)
{
    mpu_ctx_t *ctx = (mpu_ctx_t*)lv_timer_get_user_data(t);
    if(!ctx) return;
    
    if(mpu6050_read_all(ctx) == 0) {
        data_buffer = *ctx;
        buffer_ready = 1;
    }
}
static inline int x_at(int i) {
    return (int)lroundf(i * (CANVAS_WIDTH - 1.0f) / (MAX_POINTS - 1));
}
// 绘制加速度图表
static void draw_acc_chart(void)
{
    // 清空画布
    lv_canvas_fill_bg(acc_canvas, lv_color_hex(0x101010), LV_OPA_COVER);
    
    lv_layer_t layer;
    lv_canvas_init_layer(acc_canvas, &layer);
    
    // 绘制中心线（0g 基准线）
    lv_draw_line_dsc_t center_line_dsc;
    lv_draw_line_dsc_init(&center_line_dsc);
    center_line_dsc.width = 1;
    center_line_dsc.color = lv_color_hex(0x404040);
    center_line_dsc.opa = LV_OPA_50;
    center_line_dsc.p1.x = 0;
    center_line_dsc.p1.y = CANVAS_HEIGHT / 2;
    center_line_dsc.p2.x = CANVAS_WIDTH;
    center_line_dsc.p2.y = CANVAS_HEIGHT / 2;
    lv_draw_line(&layer, &center_line_dsc);
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.p1.x = 0;
    line_dsc.p1.y = 0;
    line_dsc.p2.x = 0;
    line_dsc.p2.y = 0;
    
    int mid_y = CANVAS_HEIGHT / 2;
    // int x_step = CANVAS_WIDTH / MAX_POINTS;
    // 加速度范围 ±2g，缩放到画布高度：413/2 = 206 像素对应 2g
    float acc_scale_factor = (CANVAS_HEIGHT / 2.0) / (2.0 * ACC_SCALE);
    
    // 绘制 X 轴 - 红色
    line_dsc.color = lv_palette_main(LV_PALETTE_RED);
    for(int i = 1; i < MAX_POINTS; i++) {
    line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = mid_y - (int)(acc_x_data[i-1] * acc_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = mid_y - (int)(acc_x_data[i] * acc_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    // 绘制 Y 轴 - 绿色
    line_dsc.color = lv_palette_main(LV_PALETTE_GREEN);
    for(int i = 1; i < MAX_POINTS; i++) {
        line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = mid_y - (int)(acc_y_data[i-1] * acc_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = mid_y - (int)(acc_y_data[i] * acc_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    // 绘制 Z 轴 - 蓝色
    line_dsc.color = lv_palette_main(LV_PALETTE_BLUE);
    for(int i = 1; i < MAX_POINTS; i++) {
        line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = mid_y - (int)(acc_z_data[i-1] * acc_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = mid_y - (int)(acc_z_data[i] * acc_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    lv_canvas_finish_layer(acc_canvas, &layer);
}

// 绘制倾角图表
static void draw_gyo_chart(void)
{
    lv_canvas_fill_bg(gyo_canvas, lv_color_hex(0x151515), LV_OPA_COVER);
    
    lv_layer_t layer;
    lv_canvas_init_layer(gyo_canvas, &layer);
    
    // 先绘制中心线（0° 基准线）
    lv_draw_line_dsc_t center_line_dsc;
    lv_draw_line_dsc_init(&center_line_dsc);
    center_line_dsc.width = 1;
    center_line_dsc.color = lv_color_hex(0x404040);  // 灰色基准线
    center_line_dsc.opa = LV_OPA_50;
    center_line_dsc.p1.x = 0;
    center_line_dsc.p1.y = CANVAS_HEIGHT / 2;
    center_line_dsc.p2.x = CANVAS_WIDTH;
    center_line_dsc.p2.y = CANVAS_HEIGHT / 2;
    lv_draw_line(&layer, &center_line_dsc);
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.p1.x = 0;
    line_dsc.p1.y = 0;
    line_dsc.p2.x = 0;
    line_dsc.p2.y = 0;
    
    int mid_y = CANVAS_HEIGHT / 2;  // 中心线位置
    // int x_step = CANVAS_WIDTH / MAX_POINTS;
    // 角度范围 ±90°，缩放到画布高度：413/2 = 206 像素对应 90°
    float ang_scale_factor = (CANVAS_HEIGHT / 2.0) / (90.0 * ANG_SCALE);
    
    // 绘制 Roll - 橙色（以中心线为基准）
    line_dsc.color = lv_palette_main(LV_PALETTE_ORANGE);
    for(int i = 1; i < MAX_POINTS; i++) {
        line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = mid_y - (int)(roll_data[i-1] * ang_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = mid_y - (int)(roll_data[i] * ang_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    // 绘制 Pitch - 紫色（以中心线为基准）
    line_dsc.color = lv_palette_main(LV_PALETTE_PURPLE);
    for(int i = 1; i < MAX_POINTS; i++) {
        line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = mid_y - (int)(pitch_data[i-1] * ang_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = mid_y - (int)(pitch_data[i] * ang_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    lv_canvas_finish_layer(gyo_canvas, &layer);
}

// 绘制温度图表
static void draw_temp_chart(void)
{
    lv_canvas_fill_bg(temp_canvas, lv_color_hex(0x1A1A1A), LV_OPA_COVER);
    
    lv_layer_t layer;
    lv_canvas_init_layer(temp_canvas, &layer);
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 2;
    line_dsc.color = lv_palette_main(LV_PALETTE_TEAL);
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.p1.x = 0;
    line_dsc.p1.y = 0;
    line_dsc.p2.x = 0;
    line_dsc.p2.y = 0;
    
    // int x_step = CANVAS_WIDTH / MAX_POINTS;
    // 温度范围 0-60°C，缩放到整个画布高度 414 像素
    float temp_scale_factor = 414.0 / (60.0 * TEMP_SCALE);
    
    for(int i = 1; i < MAX_POINTS; i++) {
        line_dsc.p1.x = x_at(i-1);
        line_dsc.p1.y = 414 - (int)(temp_data[i-1] * temp_scale_factor);
        line_dsc.p2.x = x_at(i);
        line_dsc.p2.y = 414 - (int)(temp_data[i] * temp_scale_factor);
        lv_draw_line(&layer, &line_dsc);
    }
    
    lv_canvas_finish_layer(temp_canvas, &layer);
}

static void chart_timer_cb(lv_timer_t *t)
{
    if(!buffer_ready) return;
    buffer_ready = 0;

    mpu_ctx_t *ctx = &data_buffer;
    
    // SHIFT 左移效果：每次左移 2 个点（速度加倍）
    #define SHIFT_STEP 1
    for(int i = 0; i < MAX_POINTS - SHIFT_STEP; i++) {
        acc_x_data[i] = acc_x_data[i + SHIFT_STEP];
        acc_y_data[i] = acc_y_data[i + SHIFT_STEP];
        acc_z_data[i] = acc_z_data[i + SHIFT_STEP];
        roll_data[i] = roll_data[i + SHIFT_STEP];
        pitch_data[i] = pitch_data[i + SHIFT_STEP];
        temp_data[i] = temp_data[i + SHIFT_STEP];
    }
    
    // 计算倾角
    double ax = ctx->ax_g, ay = ctx->ay_g, az = ctx->az_g;
    double roll_deg  = atan2(ay, az) * 180.0 / M_PI;
    double pitch_deg = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    
    // 在最右边填充新数据（填充 SHIFT_STEP 个相同值）
    for(int i = 0; i < SHIFT_STEP; i++) {
        int idx = MAX_POINTS - SHIFT_STEP + i;
        acc_x_data[idx] = (int16_t)(ctx->ax_g * ACC_SCALE);
        acc_y_data[idx] = (int16_t)(ctx->ay_g * ACC_SCALE);
        acc_z_data[idx] = (int16_t)(ctx->az_g * ACC_SCALE);
        roll_data[idx] = (int16_t)(roll_deg * ANG_SCALE);
        pitch_data[idx] = (int16_t)(pitch_deg * ANG_SCALE);
        temp_data[idx] = (int16_t)(ctx->temp_c * TEMP_SCALE);
    }
    
    // 重绘所有图表
    draw_acc_chart();
    draw_gyo_chart();
    draw_temp_chart();
}

void mpu6050_chart_display_ui(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    
    // 创建加速度画布
    acc_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(acc_canvas, acc_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(acc_canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_align(acc_canvas, LV_ALIGN_TOP_MID, 0, 0);
    lv_canvas_fill_bg(acc_canvas, lv_color_hex(0x101010), LV_OPA_COVER);
    
    // 创建倾角画布
    gyo_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(gyo_canvas, gyo_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(gyo_canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_align_to(gyo_canvas, acc_canvas, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_canvas_fill_bg(gyo_canvas, lv_color_hex(0x151515), LV_OPA_COVER);
    
    // 创建温度画布
    temp_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(temp_canvas, temp_buf, CANVAS_WIDTH, 414, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(temp_canvas, CANVAS_WIDTH, 414);
    lv_obj_align_to(temp_canvas, gyo_canvas, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_canvas_fill_bg(temp_canvas, lv_color_hex(0x1A1A1A), LV_OPA_COVER);
}

void setup_mpu6050_chart_refresh(void){
    if(mpu6050_init(&g_ctx) != 0){
        printf("MPU6050_INITILIZE_FAILED.\n");
        return;
    }
    mpu6050_chart_display_ui();

    // 创建两个定时器
    lv_timer_t* sample_tmr = lv_timer_create(fast_sample_cb, 20, &g_ctx);  // 10ms 采样
    lv_timer_t* chart_tmr = lv_timer_create(chart_timer_cb, 50, NULL);     // 50ms → 20ms，速度提升2.5倍
}