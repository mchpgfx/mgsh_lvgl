#include "lv_draw_gpu2dc.h"

#if LV_USE_DRAW_GPU2DC

#include "../../../draw/lv_draw_rect.h"
#include "../../../draw/lv_draw_image.h"
#include "../../../misc/lv_log.h"
#include "../../../misc/lv_color.h"
#include "nano2D.h"

/* GPU2DC Stride Alignment Check */
#define GFX_STRIDE_ALIGN_FAILS(w, m, p) ( \
    ((uintptr_t)(p) & 0xF) != 0 || ( \
    ((m)==N2D_RGB565 || (m)==N2D_ARGB8888) ? ((w) & 0xF) != 0 : /* 16-byte aligned */  \
    ((m)==N2D_RGB888) ? ((w) % 6) != 0 :                        /* 6-byte aligned */   \
    1)) /* default to fail safe */

/* GPU2DC Driver Heap */
#define GPU_HEAP_SIZE 0x100000U // 1MB
__attribute__ ((section(".region_nocache"), aligned (32))) static uint8_t gpu_heap[GPU_HEAP_SIZE];

static n2d_buffer_format_t lv_color_format_to_n2d_format(lv_color_format_t cf)
{
    switch(cf) 
    {
        case LV_COLOR_FORMAT_RGB565:       return N2D_RGB565;
        case LV_COLOR_FORMAT_RGB888:       return N2D_RGB888;
        case LV_COLOR_FORMAT_ARGB8888:     return N2D_ARGB8888;
        default:                           return (n2d_buffer_format_t)-1;
    }
}

static n2d_blend_t lv_blend_mode_to_n2d_blend(lv_blend_mode_t blend_mode)
{
    switch(blend_mode) {
        case LV_BLEND_MODE_NORMAL:
            return N2D_BLEND_SRC_OVER;
        case LV_BLEND_MODE_ADDITIVE:
            return N2D_BLEND_ADDITIVE;
        case LV_BLEND_MODE_SUBTRACTIVE:
            return N2D_BLEND_SUBTRACT;
        default:
            return N2D_BLEND_NONE;
    }
}

static n2d_color_t lv_color_to_n2d_color(lv_color_t color, lv_opa_t opa)
{
    return (n2d_color_t)((opa << 24) | (color.red << 16) | (color.green << 8) | color.blue);
}

static int32_t gpu2dc_evaluate_cb(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);

    const lv_draw_dsc_base_t * base_dsc = task->draw_dsc;
    if (base_dsc == NULL) return 0;

    /* Check if the destination buffer format is supported */
    if(lv_color_format_to_n2d_format(base_dsc->layer->color_format) == (n2d_buffer_format_t)-1) {
        return 0;
    }

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: 
        {
            const lv_draw_fill_dsc_t * dsc = task->draw_dsc;

            /* Gradient fill or radius not supported */
            if (dsc->grad.dir != LV_GRAD_DIR_NONE || dsc->radius != 0) return 0;
            if (GFX_STRIDE_ALIGN_FAILS(dsc->base.layer->draw_buf->header.w, lv_color_format_to_n2d_format(dsc->base.layer->draw_buf->header.cf), dsc->base.layer->draw_buf->data)) return 0;

            /* If task is supported, assign a preference score. */
            task->preference_score = 90;
            task->preferred_draw_unit_id = GPU2DC_DRAW_UNIT_ID;

            break;
        }
        case LV_DRAW_TASK_TYPE_IMAGE: 
        {
            const lv_draw_image_dsc_t * dsc = task->draw_dsc;
            if (lv_color_format_to_n2d_format(dsc->header.cf) == (n2d_buffer_format_t)-1) return 0;
            if(!dsc->bitmap_mask_src || dsc->rotation || dsc->clip_radius || dsc->scale_x || dsc->scale_y || dsc->skew_x || dsc->skew_y) return 0;
            if (GFX_STRIDE_ALIGN_FAILS(dsc->header.w, lv_color_format_to_n2d_format(dsc->header.cf), dsc->src)) return 0;
            
            /* If task is supported, assign a preference score. */
            task->preference_score = 90;
            task->preferred_draw_unit_id = GPU2DC_DRAW_UNIT_ID;
            
            break;
        }
        default:
            return 0;
    }

    return 1;
}

static int32_t gpu2dc_dispatch_cb(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_gpu2dc_unit_t * unit = (lv_draw_gpu2dc_unit_t *)draw_unit;

    if(unit->active_task) return 0;

    /* Get the next available task for this unit */
    lv_draw_task_t * task = lv_draw_get_available_task(layer, NULL, GPU2DC_DRAW_UNIT_ID);
    
    if(task == NULL || task->preferred_draw_unit_id != GPU2DC_DRAW_UNIT_ID) 
    {
        n2d_commit();
        return LV_DRAW_UNIT_IDLE;
    }

    task->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    unit->active_task = task;

    n2d_buffer_t dst_buf = {0};
    dst_buf.width = layer->draw_buf->header.w;
    dst_buf.height = layer->draw_buf->header.h;
    dst_buf.stride = layer->draw_buf->header.stride;
    dst_buf.format = lv_color_format_to_n2d_format(layer->color_format);
    dst_buf.tiling = N2D_LINEAR;
    dst_buf.memory = layer->draw_buf->data;
    dst_buf.gpu = (n2d_uintptr_t)layer->draw_buf->data;

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: 
        {
            const lv_draw_fill_dsc_t * dsc = task->draw_dsc;

            lv_area_t dest_area = task->area;
            lv_area_move(&dest_area, -layer->buf_area.x1, -layer->buf_area.y1);

            n2d_rectangle_t dest_rect;
            dest_rect.x = dest_area.x1;
            dest_rect.y = dest_area.y1;
            dest_rect.width = lv_area_get_width(&dest_area);
            dest_rect.height = lv_area_get_height(&dest_area);

            n2d_color_t n2d_color = lv_color_to_n2d_color(dsc->color, dsc->opa);
            n2d_blend_t n2d_blend = N2D_BLEND_SRC_OVER;
          
            n2d_fill(&dst_buf, &dest_rect, n2d_color, n2d_blend);

            break;
        }
        case LV_DRAW_TASK_TYPE_IMAGE: 
        {
                const lv_draw_image_dsc_t * dsc = task->draw_dsc;
                
                lv_area_t dest_area = task->area;
                lv_area_move(&dest_area, -layer->buf_area.x1, -layer->buf_area.y1);
                
                n2d_rectangle_t dest_rect;
                dest_rect.x = dest_area.x1;
                dest_rect.y = dest_area.y1;
                dest_rect.width = lv_area_get_width(&dest_area);
                dest_rect.height = lv_area_get_height(&dest_area);
                
                n2d_rectangle_t src_rect;
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = dsc->header.w - 1;
                src_rect.height = dsc->header.h - 1;

                n2d_buffer_t src_buf = {0};            
                src_buf.width = dsc->header.w;
                src_buf.height = dsc->header.h;
                src_buf.stride = dsc->header.stride;
                src_buf.format = lv_color_format_to_n2d_format(dsc->header.cf);
                src_buf.tiling = N2D_LINEAR;
                src_buf.memory = (void*)dsc->src;
                src_buf.gpu = (n2d_uintptr_t)(void*)dsc->src;
                
                n2d_blend_t n2d_blend = lv_blend_mode_to_n2d_blend(dsc->blend_mode);
                
                n2d_blit(&dst_buf, &dest_rect, &src_buf, &src_rect, n2d_blend);
                
                break;
            }
        default:
            break;
    }

    unit->active_task->state = LV_DRAW_TASK_STATE_READY;
    unit->active_task = NULL;

    /* The draw unit is free now. Request a new dispatch. */
    lv_draw_dispatch_request();

    return 1;
}

static int32_t gpu2dc_delete_cb(lv_draw_unit_t * draw_unit)
{
    n2d_close();
    
    return 1;
}

void lv_draw_gpu2dc_init(void)
{
    /*Initialize the GPU2DC hardware*/
    n2d_init(gpu_heap, GPU_HEAP_SIZE);
    n2d_open();
    
    lv_draw_gpu2dc_unit_t * draw_gpu2dc_unit = lv_draw_create_unit(sizeof(lv_draw_gpu2dc_unit_t));

    if (draw_gpu2dc_unit == NULL) 
    {
        LV_LOG_ERROR("Failed to create GPU2DC draw unit");
        return;
    }

    draw_gpu2dc_unit->base_unit.evaluate_cb = gpu2dc_evaluate_cb;
    draw_gpu2dc_unit->base_unit.dispatch_cb = gpu2dc_dispatch_cb;
    draw_gpu2dc_unit->base_unit.delete_cb = gpu2dc_delete_cb;
    draw_gpu2dc_unit->base_unit.name = "GPU2DC";
}

#endif /*LV_USE_DRAW_GPU2DC*/
