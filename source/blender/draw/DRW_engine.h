/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_sys_types.h" /* for bool */

#include "DNA_object_enums.h"

#include "DRW_engine_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct DRWInstanceDataList;
struct Depsgraph;
struct DrawEngineType;
struct GPUMaterial;
struct GPUOffScreen;
struct GPUViewport;
struct ID;
struct Main;
struct Object;
struct Render;
struct RenderEngine;
struct RenderEngineType;
struct Scene;
struct View3D;
struct ViewLayer;
struct bContext;
struct rcti;

void DRW_engines_register(void);
void DRW_engines_free(void);

bool DRW_engine_render_support(struct DrawEngineType *draw_engine_type);
void DRW_engine_register(struct DrawEngineType *draw_engine_type);
void DRW_engine_viewport_data_size_get(
    const void *engine_type, int *r_fbl_len, int *r_txl_len, int *r_psl_len, int *r_stl_len);

typedef struct DRWUpdateContext {
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ViewLayer *view_layer;
  struct ARegion *region;
  struct View3D *v3d;
  struct RenderEngineType *engine_type;
} DRWUpdateContext;
void DRW_notify_view_update(const DRWUpdateContext *update_ctx);

typedef enum eDRWSelectStage {
  DRW_SELECT_PASS_PRE = 1,
  DRW_SELECT_PASS_POST,
} eDRWSelectStage;
typedef bool (*DRW_SelectPassFn)(eDRWSelectStage stage, void *user_data);
typedef bool (*DRW_ObjectFilterFn)(struct Object *ob, void *user_data);

void DRW_draw_view(const struct bContext *C);
void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height);

void DRW_draw_render_loop_ex(struct Depsgraph *depsgraph,
                             struct RenderEngineType *engine_type,
                             struct ARegion *region,
                             struct View3D *v3d,
                             struct GPUViewport *viewport,
                             const struct bContext *evil_C);
void DRW_draw_render_loop(struct Depsgraph *depsgraph,
                          struct ARegion *region,
                          struct View3D *v3d,
                          struct GPUViewport *viewport);
void DRW_draw_render_loop_offscreen(struct Depsgraph *depsgraph,
                                    struct RenderEngineType *engine_type,
                                    struct ARegion *region,
                                    struct View3D *v3d,
                                    const bool is_image_render,
                                    const bool draw_background,
                                    const bool do_color_management,
                                    struct GPUOffScreen *ofs,
                                    struct GPUViewport *viewport);
void DRW_draw_select_loop(struct Depsgraph *depsgraph,
                          struct ARegion *region,
                          struct View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool use_nearest,
                          const struct rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data);
void DRW_draw_depth_loop(struct Depsgraph *depsgraph,
                         struct ARegion *region,
                         struct View3D *v3d,
                         struct GPUViewport *viewport,
                         bool use_opengl_context);
void DRW_draw_depth_loop_gpencil(struct Depsgraph *depsgraph,
                                 struct ARegion *region,
                                 struct View3D *v3d,
                                 struct GPUViewport *viewport);
void DRW_draw_depth_object(struct Scene *scene,
                           struct ARegion *region,
                           struct View3D *v3d,
                           struct GPUViewport *viewport,
                           struct Object *object);
void DRW_draw_select_id(struct Depsgraph *depsgraph,
                        struct ARegion *region,
                        struct View3D *v3d,
                        const struct rcti *rect);

/* grease pencil render */
bool DRW_render_check_grease_pencil(struct Depsgraph *depsgraph);
void DRW_render_gpencil(struct RenderEngine *engine, struct Depsgraph *depsgraph);

/* This is here because GPUViewport needs it */
struct DRWInstanceDataList *DRW_instance_data_list_create(void);
void DRW_instance_data_list_free(struct DRWInstanceDataList *idatalist);

void DRW_render_context_enable(struct Render *render);
void DRW_render_context_disable(struct Render *render);

void DRW_opengl_context_create(void);
void DRW_opengl_context_destroy(void);
void DRW_opengl_context_enable(void);
void DRW_opengl_context_disable(void);

#ifdef WITH_XR_OPENXR
/* XXX see comment on DRW_xr_opengl_context_get() */
void *DRW_xr_opengl_context_get(void);
void *DRW_xr_gpu_context_get(void);
void DRW_xr_drawing_begin(void);
void DRW_xr_drawing_end(void);
#endif

/* For garbage collection */
void DRW_cache_free_old_batches(struct Main *bmain);

/* Never use this. Only for closing blender. */
void DRW_opengl_context_enable_ex(bool restore);
void DRW_opengl_context_disable_ex(bool restore);

void DRW_opengl_render_context_enable(void *re_gl_context);
void DRW_opengl_render_context_disable(void *re_gl_context);
void DRW_gpu_render_context_enable(void *re_gpu_context);
void DRW_gpu_render_context_disable(void *re_gpu_context);

void DRW_deferred_shader_remove(struct GPUMaterial *mat);

struct DrawDataList *DRW_drawdatalist_from_id(struct ID *id);
void DRW_drawdata_free(struct ID *id);

#ifdef __cplusplus
}
#endif
