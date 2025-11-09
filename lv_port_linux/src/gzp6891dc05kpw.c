#include "gzp6891dc05kpw.h"
#include <string.h>

static gzp_ctx_t gctx={0};
static fifo_filter_t pressure_filter = {0};

/* FIFO滤波 */
// FIFO滤波函数 - 移动平均滤波
// 初始化FIFO滤波器
static void fifo_filter_init(fifo_filter_t *filter)
{
    memset(filter->buffer, 0, sizeof(filter->buffer));
    filter->index = 0;
    filter->count = 0;
    filter->sum = 0.0;
}
static double fifo_filter_update(fifo_filter_t *filter, double new_value)
{
    // 如果缓冲区已满，减去最旧的值
    if (filter->count >= FIFO_SIZE) {
        filter->sum -= filter->buffer[filter->index];
    } else {
        filter->count++;
    }
    
    // 添加新值
    filter->buffer[filter->index] = new_value;
    filter->sum += new_value;
    
    // 更新索引（循环）
    filter->index = (filter->index + 1) % FIFO_SIZE;
    
    // 返回平均值
    return filter->sum / filter->count;
}

// 带中值滤波的FIFO（更强的抗干扰能力）
static double fifo_filter_median(fifo_filter_t *filter, double new_value)
{
    // 更新FIFO缓冲区
    if (filter->count >= FIFO_SIZE) {
        // 缓冲区已满，移除最旧的值
    } else {
        filter->count++;
    }
    
    filter->buffer[filter->index] = new_value;
    filter->index = (filter->index + 1) % FIFO_SIZE;
    
    // 如果样本数不足，返回当前值
    if (filter->count < 3) {
        return new_value;
    }
    
    // 复制当前有效数据用于排序
    double sorted[FIFO_SIZE];
    memcpy(sorted, filter->buffer, filter->count * sizeof(double));
    
    // 简单冒泡排序
    for (int i = 0; i < filter->count - 1; i++) {
        for (int j = 0; j < filter->count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                double temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    // 返回中值
    return sorted[filter->count / 2];
}

// 加权移动平均滤波（最新数据权重更大）
static double fifo_filter_weighted(fifo_filter_t *filter, double new_value)
{
    if (filter->count >= FIFO_SIZE) {
        // 缓冲区已满
    } else {
        filter->count++;
    }
    
    filter->buffer[filter->index] = new_value;
    filter->index = (filter->index + 1) % FIFO_SIZE;
    
    // 加权平均：最新的数据权重最大
    double weighted_sum = 0.0;
    double weight_total = 0.0;
    
    for (int i = 0; i < filter->count; i++) {
        int buf_index = (filter->index - 1 - i + FIFO_SIZE) % FIFO_SIZE;
        double weight = filter->count - i;  // 权重递减
        weighted_sum += filter->buffer[buf_index] * weight;
        weight_total += weight;
    }
    
    return weighted_sum / weight_total;
}
/* I2C读取 */
static int gzp6891dc_read_block(int fd, uint8_t start_reg, uint8_t *buf,int len)
{
    if(write(fd,&start_reg,1) != 1)
    {
        perror("write start reg failed.\n");
        return -1;
    }
    if(read(fd,buf,len) != len)
    {
        perror("read out failed.\n");
        return -2;
    }
    return 0;
}

static int gzp6891dc_init(gzp_ctx_t *ctx)
{
    perror("Starting GZP6891DC0.5KPW.\n");
    ctx->fd = open(I2C_DEV_GZP6891,O_RDWR);
    if(ctx->fd < 0){
        perror("open gzp6891d i2c failed.\n");
        return -1;
    }
    if(ioctl(ctx->fd,I2C_SLAVE,GZP_ADDR) < 0){
        perror("I2C_SLAVE\n");
        return -2;
    }
    return 0;
}

static int gzp6891dc_readall(gzp_ctx_t *ctx)
{
    uint8_t buffer[5];
    if(gzp6891dc_read_block(ctx->fd,0x04,buffer,5) != 0)
    {
        return -1;
    }
    ctx->p_data = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | (uint32_t)buffer[2];
    if(ctx->p_data > 8388608) ctx->p_data-= 16777216;                       //read out raw pressure data
    ctx->p_actual = ((double)ctx->p_data / (1 << 21))*(PMAX-PMIN)+PMIN;     //convert raw pressure to real pressure
    ctx->t_data = ((uint16_t)buffer[3] << 8) | (int16_t)buffer[4];
    ctx->p_calibrated = ctx->p_actual + ctx->p_shift;
    // printf("Pressure: [ %d ]Pa\n",(int)ctx->p_actual);
    return 0;
}

void gzp6891dc_test(void){
    int boot_calibration = 20;
    double filterPressure = 0.0;
    //初始化滤波器
    fifo_filter_init(&pressure_filter);

    if(gzp6891dc_init(&gctx) != 0){
        perror("init failed.");
        return;
    }
    //零偏校准
    while(boot_calibration--){
        gzp6891dc_readall(&gctx);
        if(gctx.p_actual != 0){
            gctx.p_shift = 0 - gctx.p_actual;
        }
        usleep(10000);
    }
    // 主循环：每10ms读取一次，每100ms输出一次滤波后的值
    int read_count = 0;

    // 等待开始指令
    int start_key = 0;
    int ch;
    printf("PRESS 5 START.\n");
    fflush(stdout);
    while (1) {
        if (scanf("%d", &start_key) == 1) {
            // 吃掉本行剩余字符（包括回车）
            while ((ch = getchar()) != '\n' && ch != EOF);

            if (start_key == 5) {
                printf("Start Fetching ...\n");
                break;
            } else {
                printf("You entered %d. Please input number 5:\n", start_key);
                fflush(stdout);
            }
        } else {
            // 非数字：清空本行
            while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Invalid input, please input number 5:\n");
            fflush(stdout);
        }
    }

    while(1){
        // 读取原始压力值
        if(gzp6891dc_readall(&gctx) == 0) {
            // 更新滤波器（可选择不同的滤波方式）
            
            // 方式1: 简单移动平均（推荐用于稳定环境）
            // filterPressure = fifo_filter_update(&pressure_filter, gctx.p_calibrated);
            
            // 方式2: 中值滤波（推荐用于有突发干扰的环境）
            filterPressure = fifo_filter_median(&pressure_filter, gctx.p_calibrated);
            
            // 方式3: 加权移动平均（推荐用于需要快速响应的场景）
            // filterPressure = fifo_filter_weighted(&pressure_filter, gctx.p_calibrated);
            
            read_count++;
        }
        
        // 每100ms输出一次（每10次读取）
        if(read_count >= 20) {
            printf("Filtered Pressure: [ %d ]Pa (Raw: %d Pa) (Shift: %d Pa)\n", 
                   (int)filterPressure, (int)gctx.p_actual,(int)gctx.p_shift);
            read_count = 0;
        }
        
        usleep(10000);  // 10ms
    }
}