#include "gpio_control.h"


static void gpio_export_set(int gpio)
{
    char buffer[64];
    int len;
    int fd = open("/sys/class/gpio/export",O_WRONLY);
    if(fd < 0){
        perror("Export open failed.");
        return;
    }
    len = snprintf(buffer,sizeof(buffer),"%d",gpio);
    write(fd,buffer,len);
    close(fd);
}

static void gpio_direction_set(int gpio, const char *direction)
{
    char path[64];
    snprintf(path,sizeof(path),"/sys/class/gpio/gpio%d/direction",gpio);
    int fd = open(path,O_WRONLY);
    if(fd < 0)
    {
        perror("Direction open failed.");
        return;
    }
    write(fd,direction,strlen(direction));
    close(fd);
}

void gpio_set(int gpio,const char *direction){
    gpio_export_set(gpio);
    gpio_direction_set(gpio,direction);
}

void gpio_write(int gpio,int value)
{
    char path[64];
    snprintf(path,sizeof(path),"/sys/class/gpio/gpio%d/value",gpio);
    int fd = open(path,O_RDWR);
    if(fd < 0){
        perror("gpio open failed.");
        return;
    }
    if(value)
        write(fd,"1",2);
    else
        write(fd,"0",2);
    close(fd);
}

