/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

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

#include "app.h"
#include "definitions.h"

#include "lv_conf.h"
#include "../../../third_party/lvgl/src/lvgl.h"
#include "../../../third_party/lvgl/demos/lv_demos.h"

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
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

#define LVGL_TICK_TIMER_MS 10

/* Display resolution - match lvgl max res */
#define DISP_HRES LV_HOR_RES_MAX
#define DISP_VRES LV_VER_RES_MAX

/* lvgl scratch buffer size*/
#define DISP_BUFF_HRES 480
#define DISP_BUFF_VRES 200

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUFF_HRES * DISP_BUFF_VRES];

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_indev_t * my_indev;
static SYS_INP_InputListener inputListener;
static uint32_t touchX = 0, touchY = 0;
static bool touchDown = false;
static SYS_TIME_HANDLE timer = SYS_TIME_HANDLE_INVALID;

gfxResult DRV_ILI9488_BlitBuffer(int32_t x, int32_t y, gfxPixelBuffer* buf);
void lv_init(void);
LV_ATTRIBUTE_TICK_INC void lv_tick_inc(uint32_t tick_period);


gfxPixelBuffer pixelBuff;

void flush_cb(struct _lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    gfxPixelBufferCreate(area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, GFX_COLOR_MODE_RGB_565, buf1, &pixelBuff);
    
    DRV_ILI9488_BlitBuffer(area->x1, area->y1, &pixelBuff);
}

void gfxBlitCallBackFunc (void)
{
    lv_disp_flush_ready(&disp_drv);
}

void input_read(lv_indev_drv_t * drv, lv_indev_data_t*data)
{
    data->point.x = touchX;
    data->point.y = touchY;
    data->state = (touchDown == true) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static void touchDownHandler(const SYS_INP_TouchStateEvent* const evt)
{
    touchX = evt->x;
    touchY = evt->y;
    touchDown = true;
}

static void touchUpHandler(const SYS_INP_TouchStateEvent* const evt)
{
    touchX = evt->x;
    touchY = evt->y;
    touchDown = false;
}

static void touchMoveHandler(const SYS_INP_TouchMoveEvent* const evt)
{
    touchX = evt->x;
    touchY = evt->y;
    touchDown = true;
}

static void Timer_Callback ( uintptr_t context)
{
    lv_tick_inc(LVGL_TICK_TIMER_MS);
}

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;



    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks ( void )
{

    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {
            gfxIOCTLArg_Value val; 
            
            bool appInitialized = true;

            /* Initialize blit callback */
            val.value.v_pointer = (void*) gfxBlitCallBackFunc;
            DRV_ILI9488_IOCTL(GFX_IOCTL_SET_BLIT_CALLBACK, &val);    

            /* Initialize lvgl library */
            lv_init();

            /* Initialize lvgl > display interface*/
            lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_BUFF_HRES * DISP_BUFF_VRES);

            lv_disp_drv_init(&disp_drv);
            disp_drv.hor_res = DISP_HRES;
            disp_drv.ver_res = DISP_VRES;
            disp_drv.flush_cb = flush_cb;
            disp_drv.draw_buf = &draw_buf;
            lv_disp_drv_register(&disp_drv);

            /* Initialize lvgl > touch interface*/
            inputListener.handleTouchDown = &touchDownHandler;
            inputListener.handleTouchUp = &touchUpHandler;
            inputListener.handleTouchMove = &touchMoveHandler;
            SYS_INP_AddListener(&inputListener);    

            lv_indev_drv_init(&indev_drv);      /*Basic initialization*/
            indev_drv.type = LV_INDEV_TYPE_POINTER;
            indev_drv.read_cb = input_read;

            /*Register the driver in LVGL and save the created input device object*/
            my_indev = lv_indev_drv_register(&indev_drv);

            lv_disp_flush_ready(&disp_drv);

            /* Initialize tick timer */
            timer = SYS_TIME_CallbackRegisterMS(Timer_Callback, 1, LVGL_TICK_TIMER_MS, SYS_TIME_PERIODIC);
            
            /* Demo */
            #if LV_USE_DEMO_WIDGETS
            lv_demo_widgets();
            #elif LV_USE_DEMO_BENCHMARK
            lv_demo_benchmark();
            #elif LV_USE_DEMO_STRESS
            lv_demo_stress();
            #elif LV_USE_DEMO_MUSIC
            lv_demo_music();
            #endif            

            if (appInitialized)
            {

                appData.state = APP_STATE_SERVICE_TASKS;
            }
            break;
        }

        case APP_STATE_SERVICE_TASKS:
        {
            gfxIOCTLArg_Value val; 
            
            DRV_ILI9488_IOCTL(GFX_IOCTL_GET_STATUS, &val);
            if (val.value.v_uint == 0)
            {
                lv_task_handler();
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
