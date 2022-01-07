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

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct DRWData;
struct DRWInstanceDataList;
struct Depsgraph;
struct DrawEngineType;
struct GHash;
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

/**
 * Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine.
 */
void DRW_draw_view(const struct bContext *C);
/**
 * Draw render engine info.
 */
void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height);

/**
 * Used for both regular and off-screen drawing.
 * Need to reset DST before calling this function
 */
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
/**
 * \param viewport: can be NULL, in this case we create one.
 */
void DRW_draw_render_loop_offscreen(struct Depsgraph *depsgraph,
                                    struct RenderEngineType *engine_type,
                                    struct ARegion *region,
                                    struct View3D *v3d,
                                    bool is_image_render,
                                    bool draw_background,
                                    bool do_color_management,
                                    struct GPUOffScreen *ofs,
                                    struct GPUViewport *viewport);
void DRW_draw_render_loop_2d_ex(struct Depsgraph *depsgraph,
                                struct ARegion *region,
                                struct GPUViewport *viewport,
                                const struct bContext *evil_C);
/**
 * object mode select-loop, see: #ED_view3d_draw_select_loop (legacy drawing).
 */
void DRW_draw_select_loop(struct Depsgraph *depsgraph,
                          struct ARegion *region,
                          struct View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool use_nearest,
                          bool do_material_sub_selection,
                          const struct rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data);
/**
 * object mode select-loop, see: #ED_view3d_draw_depth_loop (legacy drawing).
 */
void DRW_draw_depth_loop(struct Depsgraph *depsgraph,
                         struct ARegion *region,
                         struct View3D *v3d,
                         struct GPUViewport *viewport);
/**
 * Converted from #ED_view3d_draw_depth_gpencil (legacy drawing).
 */
void DRW_draw_depth_loop_gpencil(struct Depsgraph *depsgraph,
                                 struct ARegion *region,
                                 struct View3D *v3d,
                                 struct GPUViewport *viewport);
/**
 * Clears the Depth Buffer and draws only the specified object.
 */
void DRW_draw_depth_object(struct Scene *scene,
                           struct ARegion *region,
                           struct View3D *v3d,
                           struct GPUViewport *viewport,
                           struct Object *object);
void DRW_draw_select_id(struct Depsgraph *depsgraph,
                        struct ARegion *region,
                        struct View3D *v3d,
                        const struct rcti *rect);

/* Grease pencil render. */

/**
 * Helper to check if exit object type to render.
 */
bool DRW_render_check_grease_pencil(struct Depsgraph *depsgraph);
void DRW_render_gpencil(struct RenderEngine *engine, struct Depsgraph *depsgraph);

/**
 * This is here because #GPUViewport needs it.
 */
struct DRWInstanceDataList *DRW_instance_data_list_create(void);
void DRW_instance_data_list_free(struct DRWInstanceDataList *idatalist);
void DRW_uniform_attrs_pool_free(struct GHash *table);

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
void DRW_cache_free_old_subdiv(void);

/* For the OpenGL evaluators and garbage collected subdivision data. */
void DRW_subdiv_free(void);

/* Never use this. Only for closing blender. */
void DRW_opengl_context_enable_ex(bool restore);
void DRW_opengl_context_disable_ex(bool restore);

void DRW_opengl_render_context_enable(void *re_gl_context);
void DRW_opengl_render_context_disable(void *re_gl_context);
/**
 * Needs to be called AFTER #DRW_opengl_render_context_enable().
 */
void DRW_gpu_render_context_enable(void *re_gpu_context);
/**
 * Needs to be called BEFORE #DRW_opengl_render_context_disable().
 */
void DRW_gpu_render_context_disable(void *re_gpu_context);

void DRW_deferred_shader_remove(struct GPUMaterial *mat);

/**
 * Get DrawData from the given ID-block. In order for this to work, we assume that
 * the DrawData pointer is stored in the struct in the same fashion as in #IdDdtTemplate.
 */
struct DrawDataList *DRW_drawdatalist_from_id(struct ID *id);
void DRW_drawdata_free(struct ID *id);

struct DRWData *DRW_viewport_data_create(void);
void DRW_viewport_data_free(struct DRWData *drw_data);

bool DRW_opengl_context_release(void);
void DRW_opengl_context_activate(bool drw_state);

/**
 * We may want to move this into a more general location.
 * \note This doesn't require the draw context to be in use.
 */
void DRW_draw_cursor_2d_ex(const struct ARegion *region, const float cursor[2]);

#ifdef __cplusplus
}
#endif
