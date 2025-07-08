#ifndef LV_DRAW_GPU2DC_H
#define LV_DRAW_GPU2DC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../../lv_conf_internal.h"

#if LV_USE_DRAW_GPU2DC
#include "../../../draw/lv_draw_private.h"

#define GPU2DC_DRAW_UNIT_ID 3

typedef struct {
    lv_draw_unit_t base_unit;
    lv_draw_task_t * active_task;
} lv_draw_gpu2dc_unit_t;

void lv_draw_gpu2dc_init(void);

#endif /*LV_USE_DRAW_GPU2DC*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_GPU2DC_H*/
