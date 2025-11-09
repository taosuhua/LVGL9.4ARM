#ifndef __GPIO_CONTROL_H
#define __GPIO_CONTROL_H
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define GPIO3_A1    97      //GPIO2_A1 = 3*32 + 1 = 97

void gpio_set(int gpio,const char *direction);
void gpio_write(int gpio,int value);

#endif