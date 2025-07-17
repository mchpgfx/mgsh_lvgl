/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_lvgl.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app_lvgl.h"
#include "definitions.h"
#include "gfx/driver/gfx_driver.h"

#include "lvgl.h"
#include "lv_conf.h"
#include "lv_demos.h"
#include "nano2D_enum.h"
#include "nano2D.h"
#include "arm_neon.h"

/* LVGL Parameters */
#define LV_UNCACHED_BUFFER  1
#define LV_TICK_INC_VAL_MS  1
#define LV_TASK_INC_VAL_MS  LV_DEF_REFR_PERIOD

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_LVGL_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/
APP_LVGL_DATA app_lvglData;

/* Scratch Buffer */
#if LV_UNCACHED_BUFFER
__attribute__ ((section(".region_nocache"), aligned (32))) uint16_t buff[720 * 1280];
#else
__attribute__ ((aligned (32))) uint16_t buff[720 * 1280];
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

static void touchDownHandler(const SYS_INP_TouchStateEvent * const evt) 
{
        app_lvglData.touch_pressed = true;
        app_lvglData.touch_x = evt->x;
        app_lvglData.touch_y = evt->y;
}

static void touchUpHandler(const SYS_INP_TouchStateEvent * const evt) 
{
        app_lvglData.touch_pressed = false;
        app_lvglData.touch_x = evt->x;
        app_lvglData.touch_y = evt->y;
}

static void touchMoveHandler(const SYS_INP_TouchMoveEvent * const evt) 
{
        app_lvglData.touch_x = evt->x;
        app_lvglData.touch_y = evt->y;
}

static void lv_tick_inc_cb(uintptr_t _) 
{
    app_lvglData.time_ticks += LV_TICK_INC_VAL_MS;
    lv_tick_inc(LV_TICK_INC_VAL_MS);
}

/* GPU2DC Stride Alignment Check */
#define GFX_STRIDE_ALIGN_FAILS(w, m, p) ( \
    ((uintptr_t)(p) & 0xF) != 0 || ( \
    ((m)==N2D_RGB565 || (m)==N2D_ARGB8888) ? ((w) & 0xF) != 0 : /* 16-byte aligned */  \
    ((m)==N2D_RGB888) ? ((w) % 6) != 0 :                        /* 6-byte aligned */   \
    1)) /* default to fail safe */

/* Alignment Check */
#define IS_ALIGNED(ptr, align) (((uintptr_t)(ptr) & ((align) - 1)) == 0)

static void lv_disp_drv_flush_cb(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * color_p) 
{
    gfxIOCTLArg_Value ioctlArg;
    ioctlArg.value.v_pbuffer = 0;
    DRV_XLCDC_IOCTL(GFX_IOCTL_GET_FRAMEBUFFER, &ioctlArg);
    
    n2d_buffer_t dst_buf = {0};
    dst_buf.width = 720;
    dst_buf.height = 1280;
    dst_buf.stride = dst_buf.width * 2;
    dst_buf.format = N2D_RGB565;
    dst_buf.tiling = N2D_LINEAR;
    dst_buf.memory = ioctlArg.value.v_pbuffer->pixels;
    dst_buf.gpu = (n2d_uintptr_t)ioctlArg.value.v_pbuffer->pixels;
    
    n2d_buffer_t src_buf = {0};
    src_buf.width = lv_area_get_width(area);
    src_buf.height = lv_area_get_height(area);
    src_buf.stride = src_buf.width * 2;
    src_buf.format = N2D_RGB565;
    src_buf.tiling = N2D_LINEAR;
    src_buf.memory = (void*)color_p;
    src_buf.gpu = (n2d_uintptr_t)(void*)color_p;
    
    n2d_rectangle_t dest_rect;
    dest_rect.x = area->x1;
    dest_rect.y = area->y1;
    dest_rect.width = lv_area_get_width(area);
    dest_rect.height = lv_area_get_height(area);

    n2d_rectangle_t src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = lv_area_get_width(area);
    src_rect.height = lv_area_get_height(area);
    
    if (GFX_STRIDE_ALIGN_FAILS(src_buf.width, src_buf.format, src_buf.memory) ||
        GFX_STRIDE_ALIGN_FAILS(dst_buf.width, dst_buf.format, dst_buf.memory))
    {
        const uint32_t pixelSize = 2;
        const uint32_t rowSize = src_rect.width * pixelSize;
        const uint32_t srcStride = src_rect.width * pixelSize;
        const uint32_t destStride = 720 * pixelSize;
        
        uint8_t* restrict srcBase = (uint8_t*)color_p;
        uint8_t* restrict destBase = (uint8_t*)ioctlArg.value.v_pbuffer->pixels +
                                     (area->y1 * destStride) +
                                     (area->x1 * pixelSize);

        for (uint32_t row = 0; row < src_rect.height ; row++)
        {
            uint8_t* restrict src = srcBase + row * srcStride;
            uint8_t* restrict dst = destBase + row * destStride;

            if (IS_ALIGNED(src, 4) && IS_ALIGNED(dst, 4) && rowSize >= 16)
            {
                uint32_t vectors = rowSize / 16;
                uint32_t remain = rowSize % 16;

                if (row < src_rect.height - 1)
                {
                    __builtin_prefetch(src + srcStride);
                }

                while (vectors--)
                {
                    __builtin_prefetch(src + 64);

                    uint8x16_t data = vld1q_u8(src);
                    vst1q_u8(dst, data);

                    src += 16;
                    dst += 16;
                }

                if (remain)
                {
                    memcpy(dst, src, remain);
                }
            }
            else
            {
                memcpy(dst, src, rowSize);
            }
        }
        
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    n2d_blit(&dst_buf, &dest_rect, &src_buf, &src_rect, N2D_BLEND_NONE);
    n2d_commit();

    lv_disp_flush_ready(disp_drv);
}

static void lv_indev_drv_read_cb(lv_indev_t * _, lv_indev_data_t * indev_data) 
{
    if (app_lvglData.touch_pressed) {
        indev_data->point.x = app_lvglData.touch_x;
        indev_data->point.y = app_lvglData.touch_y;
        indev_data->state = LV_INDEV_STATE_PRESSED;
    } else {
        indev_data->point.x = app_lvglData.touch_x;
        indev_data->point.y = app_lvglData.touch_y;
        indev_data->state = LV_INDEV_STATE_RELEASED;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_LVGL_Initialize ( void )

  Remarks:
    See prototype in app_lvgl.h.
 */

void APP_LVGL_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    app_lvglData.state = APP_LVGL_STATE_INIT;
    
    memset(&app_lvglData, 0, sizeof(app_lvglData));

    /* Initialize Input System Service */
    app_lvglData.input_listener.handleTouchDown = &touchDownHandler;
    app_lvglData.input_listener.handleTouchUp = &touchUpHandler;
    app_lvglData.input_listener.handleTouchMove = &touchMoveHandler;
    SYS_INP_AddListener(&app_lvglData.input_listener);
    app_lvglData.touch_pressed = false;
}


/******************************************************************************
  Function:
    void APP_LVGL_Tasks ( void )

  Remarks:
    See prototype in app_lvgl.h.
 */

void APP_LVGL_Tasks ( void )
{
    static uint32_t temp_ticks;
    
    /* Check the application's current state. */
    switch ( app_lvglData.state )
    {
        /* Application's initial state. */
        case APP_LVGL_STATE_INIT:
        {
            bool appInitialized = true;
    
            /* Initialize LVGL */
            lv_init();
            
            /* Set Active Layer */
            gfxIOCTLArg_Value ioctlArg;
            ioctlArg.value.v_uint = 0;
            DRV_XLCDC_IOCTL(GFX_IOCTL_SET_ACTIVE_LAYER, &ioctlArg);  
            
            /* Get Display Parameters */
            gfxIOCTLArg_DisplaySize argDispSize;
            DRV_XLCDC_IOCTL(GFX_IOCTL_GET_DISPLAY_SIZE, &argDispSize);
                        
            /* Display */
            lv_display_t * display = lv_display_create(argDispSize.width, argDispSize.height);
            lv_display_set_buffers(display, buff, NULL, sizeof(buff), LV_DISPLAY_RENDER_MODE_PARTIAL);
            lv_display_set_flush_cb(display, lv_disp_drv_flush_cb);

            /* Input */
            lv_indev_t *indev = lv_indev_create();
            lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(indev, lv_indev_drv_read_cb);

            /* Periodic tick for LVGL */
            SYS_TIME_CallbackRegisterMS(lv_tick_inc_cb, 0, LV_TICK_INC_VAL_MS,
                    SYS_TIME_PERIODIC);
            
            /* Demo */
            #if LV_USE_DEMO_WIDGETS && !LV_USE_DEMO_BENCHMARK
            lv_demo_widgets();
            #elif LV_USE_DEMO_WIDGETS && LV_USE_DEMO_BENCHMARK
            lv_demo_widgets();
            lv_demo_benchmark();
            #elif LV_USE_DEMO_STRESS
            lv_demo_stress();
            #elif LV_USE_DEMO_MUSIC
            lv_demo_music();
            #elif LV_USE_DEMO_FLEX_LAYOUT
            lv_demo_flex_layout();
            #endif
            
            if (appInitialized)
            {

                app_lvglData.state = APP_LVGL_STATE_SERVICE_TASKS;
            }
            break;
        }

        case APP_LVGL_STATE_SERVICE_TASKS:
        {
            if ((app_lvglData.time_ticks - temp_ticks) >= LV_TASK_INC_VAL_MS)
            {
                lv_task_handler();
                temp_ticks = app_lvglData.time_ticks;
            }
            break;
        }

        /* TODO: implement your application state machine.*/


        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
