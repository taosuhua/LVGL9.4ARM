#include "mpu6050.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "lvgl/lvgl.h"
#include <math.h>
#include <time.h>

#define default LV_STATE_DEFAULT
LV_FONT_DECLARE(arial120);
LV_FONT_DECLARE(fontawesome60);
LV_FONT_DECLARE(fontawesome80);
LV_FONT_DECLARE(platnomor200);
#define SYMBOL_START    "\xEF\x81\x8B"
#define SYMBOL_STOP    "\xEF\x81\x8D"
#define SYMBOL_SETTING    "\xEF\x82\xAD"
#define SYMBOL_TEMP    "\xEF\x8B\x89"
#define SYMBOL_GYRO    "\xEF\x9E\xBF"
#define SYMBOL_ACCE    "\xEF\x83\xBB"

/**** GLOBAL VARIABLES ****/
static mpu_ctx_t g_ctx = {0};
static lv_style_t chart_style,data_style;
static lv_obj_t *acc_chart,*gyo_chart,*temp_chart;
static lv_chart_series_t *ser_ax,*ser_ay,*ser_az;
static lv_chart_series_t *ser_roll, *ser_pitch;
static lv_chart_series_t *ser_temp;
static lv_obj_t *accxlabel,*accylabel,*acczlabel,*gyorolllabel,*gyopitchlabel,*templabel;
static lv_obj_t* datetime;
static char time_buffer[64];    //用于存储格式化后的时间的缓冲区
/**************************/
const char* get_current_time_str(const char* format){
    time_t now;
    struct tm *timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    strftime(time_buffer,sizeof(time_buffer),format,timeinfo);
    return time_buffer;
}
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
    lv_label_set_text_fmt(accxlabel,"%d",(int)(ctx->ax_g * 1000));
    lv_chart_set_next_value(acc_chart, ser_ay, (lv_coord_t)(ctx->ay_g * ACC_SCALE));
    lv_label_set_text_fmt(accylabel,"%d",(int)(ctx->ay_g * 1000));
    lv_chart_set_next_value(acc_chart, ser_az, (lv_coord_t)(ctx->az_g * ACC_SCALE));
    lv_label_set_text_fmt(acczlabel,"%d",(int)(ctx->az_g * 1000));

    // 倾角（度→centi-degree）
    double ax = ctx->ax_g, ay = ctx->ay_g, az = ctx->az_g;
    double roll_deg  = atan2(ay, az) * 180.0 / M_PI;
    double pitch_deg = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    lv_chart_set_next_value(gyo_chart, ser_roll,  (lv_coord_t)(roll_deg  * ANG_SCALE));
    lv_chart_set_next_value(gyo_chart, ser_pitch, (lv_coord_t)(pitch_deg * ANG_SCALE));
    lv_label_set_text_fmt(gyorolllabel,"%d",(int)(roll_deg * 10));
    lv_label_set_text_fmt(gyopitchlabel,"%d",(int)(pitch_deg * 10));

    // 温度
    lv_chart_set_next_value(temp_chart, ser_temp, (lv_coord_t)(ctx->temp_c * TEMP_SCALE));
    lv_label_set_text_fmt(templabel,"%02d",(int)(ctx->temp_c));

    // 时间
    const char* timenow = get_current_time_str("%m / %d / %Y\n%H:%M:%S");
    lv_label_set_text_fmt(datetime,"%s",timenow);
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

void modern_ui_mpu6050(void){
    //define default chartstyle
    lv_style_init(&chart_style);
    lv_style_set_radius(&chart_style,0);
    lv_style_set_border_width(&chart_style,2);
    // lv_style_set_border_opa(&chart_style,50);
    lv_style_set_border_color(&chart_style,lv_color_hex(0x333333));
    lv_style_set_pad_all(&chart_style,0);
    lv_style_set_line_width(&chart_style,0);
    lv_style_set_bg_color(&chart_style,lv_color_black());
    lv_style_set_height(&chart_style,350);

    //define default datastyle
    lv_style_init(&data_style);
    lv_style_set_text_font(&data_style,&arial120);
    lv_style_set_text_align(&data_style,LV_TEXT_ALIGN_RIGHT); 

    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x000000),LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lv_scr_act(),5,LV_STATE_DEFAULT);
    acc_chart = lv_chart_create(lv_scr_act());
    gyo_chart = lv_chart_create(lv_scr_act());
    temp_chart = lv_chart_create(lv_scr_act());
    //set default style
    lv_obj_add_style(acc_chart,&chart_style,0);
    lv_obj_add_style(gyo_chart,&chart_style,0);
    lv_obj_add_style(temp_chart,&chart_style,0);
    //set char alignment
    lv_obj_align(acc_chart,LV_ALIGN_TOP_LEFT,0,-1);
    lv_obj_align_to(gyo_chart,acc_chart,LV_ALIGN_OUT_BOTTOM_LEFT,0,-2);
    lv_obj_align_to(temp_chart,gyo_chart,LV_ALIGN_OUT_BOTTOM_LEFT,0,-2);
    //set char background color
    // lv_obj_set_style_bg_color(acc_chart,lv_color_hex(0x101010),LV_PART_MAIN);
    // lv_obj_set_style_bg_color(gyo_chart,lv_color_hex(0x151515),LV_PART_MAIN);
    // lv_obj_set_style_bg_color(temp_chart,lv_color_hex(0x1A1A1A),LV_PART_MAIN);
    //accelerate chart
    lv_chart_set_type(acc_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(acc_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(acc_chart, CHART_POINTS);
    lv_chart_set_range(acc_chart, LV_CHART_AXIS_PRIMARY_Y, -2*ACC_SCALE, 2*ACC_SCALE);
    ser_ax = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser_ay = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    ser_az = lv_chart_add_series(acc_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(acc_chart,0,0,LV_PART_INDICATOR);
    lv_obj_set_style_width(acc_chart,800,default);
    lv_obj_t* acclogo = lv_label_create(lv_scr_act());      //accelerate logo
    lv_label_set_text(acclogo,SYMBOL_ACCE);
    lv_obj_set_style_text_font(acclogo,&fontawesome60,default);
    lv_obj_align_to(acclogo,acc_chart,LV_ALIGN_TOP_LEFT,20,20);
    lv_obj_set_style_text_color(acclogo,lv_palette_main(LV_PALETTE_CYAN),default);
    //gyo chart
    lv_chart_set_type(gyo_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(gyo_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(gyo_chart, CHART_POINTS);
    lv_chart_set_range(gyo_chart, LV_CHART_AXIS_PRIMARY_Y, -90*ANG_SCALE, 90*ANG_SCALE);
    ser_roll  = lv_chart_add_series(gyo_chart, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
    ser_pitch = lv_chart_add_series(gyo_chart, lv_palette_main(LV_PALETTE_PURPLE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(gyo_chart,0,0,LV_PART_INDICATOR);
    lv_obj_set_style_width(gyo_chart,800,default);
    lv_obj_t* gyologo = lv_label_create(lv_scr_act());      //gyroscope logo
    lv_label_set_text(gyologo,SYMBOL_GYRO);
    lv_obj_set_style_text_font(gyologo,&fontawesome60,default);
    lv_obj_align_to(gyologo,gyo_chart,LV_ALIGN_TOP_LEFT,20,20);
    lv_obj_set_style_text_color(gyologo,lv_palette_main(LV_PALETTE_BROWN),default);
    //temperature chart
    lv_chart_set_type(temp_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(temp_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(temp_chart, CHART_POINTS);
    lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 60*TEMP_SCALE);
    ser_temp = lv_chart_add_series(temp_chart, lv_palette_main(LV_PALETTE_PINK), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(temp_chart,0,0,LV_PART_INDICATOR);
    lv_obj_set_style_width(temp_chart,800,default);
    lv_obj_t* templogo = lv_label_create(lv_scr_act());      //temeperature logo
    lv_label_set_text(templogo,SYMBOL_TEMP);
    lv_obj_set_style_text_font(templogo,&fontawesome60,default);
    lv_obj_align_to(templogo,temp_chart,LV_ALIGN_TOP_LEFT,30,20);
    lv_obj_set_style_text_color(templogo,lv_palette_main(LV_PALETTE_PINK),default);

    //bottom touch area container
    lv_obj_t* toucharea = lv_obj_create(lv_scr_act());
    lv_obj_set_style_height(toucharea,170,default);
    lv_obj_set_style_pad_all(toucharea,0,default);
    lv_obj_align(toucharea,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_set_style_width(toucharea,lv_pct(100),default);
    lv_obj_set_style_radius(toucharea,0,default);
    lv_obj_set_style_border_width(toucharea,1,default);
    lv_obj_set_style_border_opa(toucharea,LV_OPA_10,default);
    lv_obj_set_style_bg_color(toucharea,lv_color_hex(0x202020),default);
    lv_obj_set_style_bg_grad_color(toucharea,lv_color_hex(0x101010),default);
    lv_obj_set_style_bg_grad_dir(toucharea,LV_GRAD_DIR_VER,default);
    lv_obj_set_scrollbar_mode(toucharea,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(toucharea,LV_OBJ_FLAG_SCROLLABLE);

    //DATA display [accelerate]
    lv_obj_t* accdata = lv_obj_create(lv_scr_act());
    lv_obj_set_width(accdata,270);
    lv_obj_add_style(accdata,&chart_style,default);
    lv_obj_align_to(accdata,acc_chart,LV_ALIGN_OUT_RIGHT_TOP,-2,0);
    accxlabel = lv_label_create(accdata);
    lv_obj_add_style(accxlabel,&data_style,default);
    lv_obj_set_style_text_color(accxlabel,lv_palette_main(LV_PALETTE_RED),default);
    lv_obj_clear_flag(accdata,LV_OBJ_FLAG_SCROLLABLE);
    accylabel = lv_label_create(accdata);
    lv_obj_add_style(accylabel,&data_style,default);
    lv_obj_set_style_text_color(accylabel,lv_palette_main(LV_PALETTE_GREEN),default);
    lv_obj_clear_flag(accylabel,LV_OBJ_FLAG_SCROLLABLE);
    acczlabel = lv_label_create(accdata);
    lv_obj_add_style(acczlabel,&data_style,default);
    lv_obj_set_style_text_color(acczlabel,lv_palette_main(LV_PALETTE_BLUE),default);
    lv_obj_clear_flag(acczlabel,LV_OBJ_FLAG_SCROLLABLE);

    //roll and pitch
    lv_obj_t* gyodata = lv_obj_create(lv_scr_act());
    lv_obj_set_width(gyodata,270);
    lv_obj_add_style(gyodata,&chart_style,default);
    lv_obj_align_to(gyodata,gyo_chart,LV_ALIGN_OUT_RIGHT_TOP,-2,0);
    gyorolllabel = lv_label_create(gyodata);
    lv_obj_add_style(gyorolllabel,&data_style,default);
    lv_obj_set_style_text_color(gyorolllabel,lv_palette_main(LV_PALETTE_ORANGE),default);
    lv_obj_clear_flag(gyorolllabel,LV_OBJ_FLAG_SCROLLABLE);
    gyopitchlabel = lv_label_create(gyodata);
    lv_obj_add_style(gyopitchlabel,&data_style,default);
    lv_obj_set_style_text_color(gyopitchlabel,lv_palette_main(LV_PALETTE_PURPLE),default);
    lv_obj_clear_flag(gyopitchlabel,LV_OBJ_FLAG_SCROLLABLE);

    //temperature
    lv_obj_t* tempdata = lv_obj_create(lv_scr_act());
    lv_obj_set_width(tempdata,270);
    lv_obj_add_style(tempdata,&chart_style,default);
    lv_obj_align_to(tempdata,temp_chart,LV_ALIGN_OUT_RIGHT_TOP,-2,0);
    templabel = lv_label_create(tempdata);
    lv_obj_add_style(templabel,&data_style,default);
    lv_obj_set_style_text_font(templabel,&platnomor200,default);
    lv_obj_set_style_text_color(templabel,lv_palette_main(LV_PALETTE_PINK),default);
    lv_obj_clear_flag(templabel,LV_OBJ_FLAG_SCROLLABLE);

    //data label alignment
    lv_obj_align(accylabel,LV_ALIGN_RIGHT_MID,-10,0);
    lv_obj_align(accxlabel,LV_ALIGN_RIGHT_MID,-10,-110);
    lv_obj_align(acczlabel,LV_ALIGN_RIGHT_MID,-10,110);
    lv_obj_align(gyorolllabel,LV_ALIGN_RIGHT_MID,-10,-55);
    lv_obj_align(gyopitchlabel,LV_ALIGN_RIGHT_MID,-10,55);
    lv_obj_align(templabel,LV_ALIGN_RIGHT_MID,-10,0);

    //indicator
    lv_obj_t* indicator = lv_obj_create(toucharea);
    lv_obj_set_size(indicator,320,70);
    lv_obj_set_style_bg_color(indicator,lv_color_hex(0x52bbc1),default);
    lv_obj_set_style_bg_grad_color(indicator,lv_color_hex(0x2fa8b0),default);
    lv_obj_set_style_bg_grad_dir(indicator,LV_GRAD_DIR_VER,default);
    lv_obj_align(indicator,LV_ALIGN_TOP_LEFT,15,15);
    lv_obj_set_style_border_opa(indicator,LV_OPA_10,default);
    lv_obj_set_style_radius(indicator,10,default);
    lv_obj_set_style_bg_grad_opa(indicator,LV_OPA_50,default);
    lv_obj_clear_flag(indicator,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(indicator,LV_SCROLLBAR_MODE_OFF);
    lv_obj_t* labelIndicator = lv_label_create(indicator);
    lv_label_set_text(labelIndicator,"Auto mode");
    lv_obj_set_style_text_font(labelIndicator,&lv_font_montserrat_48,default);
    lv_obj_set_style_text_color(labelIndicator,lv_color_black(),default);
    lv_obj_center(labelIndicator);


    //buttons
    lv_obj_t* btnSetting = lv_btn_create(toucharea);
    lv_obj_set_size(btnSetting,230,170);
    lv_obj_align(btnSetting,LV_ALIGN_RIGHT_MID,-10,1);
    lv_obj_set_style_radius(btnSetting,0,default);
    lv_obj_set_style_bg_color(btnSetting,lv_color_hex(0xaaaaaa),default);
    lv_obj_set_style_bg_opa(btnSetting,LV_OPA_10,default);
    lv_obj_set_style_outline_width(btnSetting,0,default);
    lv_obj_t* labelSetting = lv_label_create(btnSetting);
    lv_label_set_text(labelSetting,SYMBOL_SETTING);
    lv_obj_center(labelSetting);
    lv_obj_set_style_text_font(labelSetting,&fontawesome80,default);


    lv_obj_t* btnStop = lv_btn_create(toucharea);
    lv_obj_set_size(btnStop,230,170);
    lv_obj_align_to(btnStop,btnSetting,LV_ALIGN_OUT_LEFT_TOP,0,0);
    lv_obj_set_style_radius(btnStop,0,default);
    lv_obj_set_style_bg_color(btnStop,lv_color_hex(0xaaaaaa),default);
    lv_obj_set_style_bg_opa(btnStop,LV_OPA_10,default);
    lv_obj_set_style_outline_width(btnStop,0,default);
    lv_obj_t* labelStop = lv_label_create(btnStop);
    lv_label_set_text(labelStop,SYMBOL_STOP);
    lv_obj_center(labelStop);
    lv_obj_set_style_text_font(labelStop,&fontawesome80,default);

    lv_obj_t* btnStart = lv_btn_create(toucharea);
    lv_obj_set_size(btnStart,230,170);
    lv_obj_align_to(btnStart,btnStop,LV_ALIGN_OUT_LEFT_TOP,0,0);
    lv_obj_set_style_radius(btnStart,0,default);
    lv_obj_set_style_bg_color(btnStart,lv_color_hex(0xaaaaaa),default);
    lv_obj_set_style_bg_opa(btnStart,LV_OPA_10,default);
    lv_obj_set_style_outline_width(btnStart,0,default);   
    lv_obj_t* labelStart = lv_label_create(btnStart);
    lv_label_set_text(labelStart,SYMBOL_START);
    lv_obj_center(labelStart);
    lv_obj_set_style_text_font(labelStart,&fontawesome80,default);

    //datetime
    datetime = lv_label_create(lv_scr_act());
    lv_obj_align(datetime,LV_ALIGN_BOTTOM_LEFT,30,-5);
    lv_obj_set_style_text_font(datetime,&lv_font_montserrat_32,default);
    lv_obj_set_style_text_color(datetime,lv_color_white(),default);

}

void setup_mpu6050_chart_refresh(void){
#ifdef __aarch64__
    if(mpu6050_init(&g_ctx) != 0){
        printf("MPU6050_INITILIZE_FAILED.\n");
        return;
    }
#endif
    // mpu6050_chart_display_ui();
    modern_ui_mpu6050();

#ifdef __aarch64__
    lv_timer_t* tmr = lv_timer_create(chart_timer_cb,READ_PERIOD,&g_ctx);
    lv_timer_set_user_data(tmr,&g_ctx);
#endif
}
