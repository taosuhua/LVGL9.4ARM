/*******************************************************************
 *
 * main.c - LVGL simulator for GNU/Linux
 *
 * Based on the original file from the repository
 *
 * @note eventually this file won't contain a main function and will
 * become a library supporting all major operating systems
 *
 * To see how each driver is initialized check the
 * 'src/lib/display_backends' directory
 *
 * - Clean up
 * - Support for multiple backends at once
 *   2025 EDGEMTech Ltd.
 *
 * Author: EDGEMTech Ltd, Erik Tagirov (erik.tagirov@edgemtech.ch)
 *
 ******************************************************************/
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"

#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"


#include "mpu6050.h"

/* Internal functions */
static void configure_simulator(int argc, char **argv);
static void print_lvgl_version(void);
static void print_usage(void);

/* contains the name of the selected backend if user
 * has specified one on the command line */
static char *selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;


/**
 * @brief Print LVGL version
 */
static void print_lvgl_version(void)
{
    fprintf(stdout, "%d.%d.%d-%s\n",
            LVGL_VERSION_MAJOR,
            LVGL_VERSION_MINOR,
            LVGL_VERSION_PATCH,
            LVGL_VERSION_INFO);
}

/**
 * @brief Print usage information
 */
static void print_usage(void)
{
    fprintf(stdout, "\nlvglsim [-V] [-B] [-b backend_name] [-W window_width] [-H window_height]\n\n");
    fprintf(stdout, "-V print LVGL version\n");
    fprintf(stdout, "-B list supported backends\n");
}

/**
 * @brief Configure simulator
 * @description process arguments recieved by the program to select
 * appropriate options
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
static void configure_simulator(int argc, char **argv)
{
    int opt = 0;

    selected_backend = NULL;
    driver_backends_register();

    const char *env_w = getenv("LV_SIM_WINDOW_WIDTH");
    const char *env_h = getenv("LV_SIM_WINDOW_HEIGHT");
    /* Default values */
    settings.window_width = atoi(env_w ? env_w : "1080");
    settings.window_height = atoi(env_h ? env_h : "1240");

    /* Parse the command-line options. */
    while ((opt = getopt (argc, argv, "b:fmW:H:BVh")) != -1) {
        switch (opt) {
        case 'h':
            print_usage();
            exit(EXIT_SUCCESS);
            break;
        case 'V':
            print_lvgl_version();
            exit(EXIT_SUCCESS);
            break;
        case 'B':
            driver_backends_print_supported();
            exit(EXIT_SUCCESS);
            break;
        case 'b':
            if (driver_backends_is_supported(optarg) == 0) {
                die("error no such backend: %s\n", optarg);
            }
            selected_backend = strdup(optarg);
            break;
        case 'W':
            settings.window_width = atoi(optarg);
            break;
        case 'H':
            settings.window_height = atoi(optarg);
            break;
        case ':':
            print_usage();
            die("Option -%c requires an argument.\n", optopt);
            break;
        case '?':
            print_usage();
            die("Unknown option -%c.\n", optopt);
        }
    }
}

static void input_config(void){
#if LV_USE_EVDEV
    // if (driver_backends_init_backend("EVDEV") == -1) {
    //     die("Failed to initialize evdev");
    // }
    lv_indev_t * indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event4");
#endif

    // === 添加 SDL 输入设备初始化（替代 EVDEV�?===
#if LV_USE_SDL
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        printf("Initializing SDL input devices...\n");
        
        // 创建 SDL 鼠标输入
        lv_indev_t *mouse = lv_sdl_mouse_create();
        if (mouse) {
            lv_indev_set_display(mouse, disp);
            printf("SDL mouse created\n");
        }
        
        // 创建键盘
        lv_indev_t *keyboard = lv_sdl_keyboard_create();
        if (keyboard) {
            lv_indev_set_display(keyboard, disp);
            printf("SDL keyboard created\n");
        }
        
        // 创建鼠标滚轮
        lv_indev_t *mousewheel = lv_sdl_mousewheel_create();
        if (mousewheel) {
            lv_indev_set_display(mousewheel, disp);
            printf("SDL mousewheel created\n");
        }
    }
#endif
}

static void myapp(void){
    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
    lv_obj_t* obj = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(obj,lv_color_hex(0x0000FF),0);
    lv_obj_set_size(obj,800,400);
    lv_obj_center(obj);

    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn,200,100);
    lv_obj_align_to(btn,obj,LV_ALIGN_OUT_BOTTOM_RIGHT,0,0);
    
    lv_obj_t* label = lv_label_create(obj);
    lv_obj_center(label);
    lv_label_set_text(label,"Hello World");
    lv_obj_set_style_text_color(label,lv_color_white(),0);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_20,0);
    
}

int main(int argc, char **argv)
{

    configure_simulator(argc, argv);
    /* Initialize LVGL. */
    lv_init();


    /* Initialize the configured backend */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    /* Enable for EVDEV support OR SDL support*/
    /* Initialize touch or mouse input*/
    input_config();

    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_benchmark();
    // setup_mpu6050_and_ui();
    
    setup_mpu6050_chart_refresh();
	while(1){
        lv_timer_handler();
        usleep(1000);
    }
    return 0;
}
