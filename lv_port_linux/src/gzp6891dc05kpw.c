#include "gzp6891dc05kpw.h"

static gzp_ctx_t gctx={0};

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
    if(ctx->p_data > 8388608) ctx->p_data-= 16777216;
    ctx->p_actual = ((double)ctx->p_data / (1 << 21))*(PMAX-PMIN)+PMIN;
    ctx->t_data = ((uint16_t)buffer[3] << 8) | (int16_t)buffer[4];
    printf("Pressure: %.5f\n",ctx->p_actual);
    return 0;
}

void gzp6891dc_test(void){
    if(gzp6891dc_init(&gctx) != 0){
        perror("init failed.");
        return;
    }
    while(1){
        gzp6891dc_readall(&gctx);
        usleep(20000);
    }
}