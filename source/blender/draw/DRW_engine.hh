/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_sys_types.h" /* for bool */

#include "DNA_object_enums.h"

struct ARegion;
struct DRWData;
struct DRWInstanceDataList;
struct Depsgraph;
struct DrawDataList;
struct DrawEngineType;
struct GHash;
struct GPUMaterial;
struct GPUOffScreen;
struct GPUVertFormat;
struct CustomDataLayer;
struct CustomData;
struct GPUViewport;
struct ID;
struct Main;
struct Object;
struct RegionView3D;
struct Render;
struct RenderEngine;
struct RenderEngineType;
struct Scene;
struct View3D;
struct ViewLayer;
struct bContext;
struct rcti;

void DRW_engines_register();
void DRW_engines_free();

bool DRW_engine_render_support(DrawEngineType *draw_engine_type);
void DRW_engine_register(DrawEngineType *draw_engine_type);

void DRW_engine_external_free(RegionView3D *rv3d);

struct DRWUpdateContext {
  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  ARegion *region;
  View3D *v3d;
  RenderEngineType *engine_type;
};
void DRW_notify_view_update(const DRWUpdateContext *update_ctx);

typedef enum eDRWSelectStage {
  DRW_SELECT_PASS_PRE = 1,
  DRW_SELECT_PASS_POST,
} eDRWSelectStage;
typedef bool (*DRW_SelectPassFn)(eDRWSelectStage stage, void *user_data);
typedef bool (*DRW_ObjectFilterFn)(Object *ob, void *user_data);

/**
 * Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine.
 */
void DRW_draw_view(const bContext *C);
/**
 * Draw render engine info.
 */
void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height);

/**
 * Used for both regular and off-screen drawing.
 * Need to reset DST before calling this function
 */
void DRW_draw_render_loop_ex(Depsgraph *depsgraph,
                             RenderEngineType *engine_type,
                             ARegion *region,
                             View3D *v3d,
                             GPUViewport *viewport,
                             const bContext *evil_C);
void DRW_draw_render_loop(Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          GPUViewport *viewport);
/**
 * \param viewport: can be NULL, in this case we create one.
 */
void DRW_draw_render_loop_offscreen(Depsgraph *depsgraph,
                                    RenderEngineType *engine_type,
                                    ARegion *region,
                                    View3D *v3d,
                                    bool is_image_render,
                                    bool draw_background,
                                    bool do_color_management,
                                    GPUOffScreen *ofs,
                                    GPUViewport *viewport);
void DRW_draw_render_loop_2d_ex(Depsgraph *depsgraph,
                                ARegion *region,
                                GPUViewport *viewport,
                                const bContext *evil_C);
/**
 * object mode select-loop, see: #ED_view3d_draw_select_loop (legacy drawing).
 */
void DRW_draw_select_loop(Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool use_nearest,
                          bool do_material_sub_selection,
                          const rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data);
/**
 * Object mode select-loop, see: #ED_view3d_draw_depth_loop (legacy drawing).
 */
void DRW_draw_depth_loop(Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         const bool use_gpencil,
                         const bool use_basic,
                         const bool use_overlay);
/**
 * Clears the Depth Buffer and draws only the specified object.
 */
void DRW_draw_depth_object(
    Scene *scene, ARegion *region, View3D *v3d, GPUViewport *viewport, Object *object);
void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d);

/* Grease pencil render. */

/**
 * Helper to check if exit object type to render.
 */
bool DRW_render_check_grease_pencil(Depsgraph *depsgraph);
void DRW_render_gpencil(RenderEngine *engine, Depsgraph *depsgraph);

/**
 * This is here because #GPUViewport needs it.
 */
DRWInstanceDataList *DRW_instance_data_list_create();
void DRW_instance_data_list_free(DRWInstanceDataList *idatalist);
void DRW_uniform_attrs_pool_free(GHash *table);

void DRW_render_context_enable(Render *render);
void DRW_render_context_disable(Render *render);

void DRW_gpu_context_create();
void DRW_gpu_context_destroy();
void DRW_gpu_context_enable();
void DRW_gpu_context_disable();

#ifdef WITH_XR_OPENXR
/* XXX: see comment on #DRW_system_gpu_context_get() */
void *DRW_system_gpu_context_get();
void *DRW_xr_blender_gpu_context_get();
void DRW_xr_drawing_begin();
void DRW_xr_drawing_end();
#endif

/* For garbage collection */
void DRW_cache_free_old_batches(Main *bmain);

namespace blender::draw {

void DRW_cache_free_old_subdiv();

/* For the OpenGL evaluators and garbage collected subdivision data. */
void DRW_subdiv_free();

}  // namespace blender::draw

/* Never use this. Only for closing blender. */
void DRW_gpu_context_enable_ex(bool restore);
void DRW_gpu_context_disable_ex(bool restore);

/* Render pipeline GPU context control.
 * Enable system context first, then enable blender context,
 * then disable blender context, then disable system context. */
void DRW_system_gpu_render_context_enable(void *re_system_gpu_context);
void DRW_system_gpu_render_context_disable(void *re_system_gpu_context);
void DRW_blender_gpu_render_context_enable(void *re_gpu_context);
void DRW_blender_gpu_render_context_disable(void *re_gpu_context);

void DRW_deferred_shader_remove(GPUMaterial *mat);
void DRW_deferred_shader_optimize_remove(GPUMaterial *mat);

/**
 * Get DrawData from the given ID-block. In order for this to work, we assume that
 * the DrawData pointer is stored in the  in the same fashion as in #IdDdtTemplate.
 */
DrawDataList *DRW_drawdatalist_from_id(ID *id);
void DRW_drawdata_free(ID *id);

DRWData *DRW_viewport_data_create();
void DRW_viewport_data_free(DRWData *drw_data);

bool DRW_gpu_context_release();
void DRW_gpu_context_activate(bool drw_state);

/**
 * We may want to move this into a more general location.
 * \note This doesn't require the draw context to be in use.
 */
void DRW_draw_cursor_2d_ex(const ARegion *region, const float cursor[2]);

void DRW_cdlayer_attr_aliases_add(GPUVertFormat *format,
                                  const char *base_name,
                                  int data_type,
                                  const char *layer_name,
                                  bool is_active_render,
                                  bool is_active_layer);
