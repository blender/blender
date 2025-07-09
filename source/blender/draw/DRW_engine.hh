/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_string_ref.hh"

struct ARegion;
struct DRWData;
struct DRWInstanceDataList;
struct Depsgraph;
struct GPUMaterial;
struct GPUOffScreen;
struct GPUVertFormat;
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

namespace blender::bke {
enum class AttrType : int16_t;
}

void DRW_engines_register();
void DRW_engines_free();

void DRW_module_init();
void DRW_module_exit();

void DRW_engine_external_free(RegionView3D *rv3d);

enum eDRWSelectStage {
  DRW_SELECT_PASS_PRE = 1,
  DRW_SELECT_PASS_POST,
};
using DRW_SelectPassFn = bool (*)(eDRWSelectStage stage, void *user_data);
using DRW_ObjectFilterFn = bool (*)(Object *ob, void *user_data);

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
/**
 * Object mode select-loop.
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
 * Used by auto-depth and other depth queries feature.
 */
void DRW_draw_depth_loop(Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         const bool use_gpencil,
                         const bool use_only_selected,
                         const bool use_only_active_object);

void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d);

/**
 * Query that drawing is in progress (use to prevent nested draw calls).
 */
bool DRW_draw_in_progress();

/* Grease pencil render. */

/**
 * Helper to check if exit object type to render.
 */
bool DRW_render_check_grease_pencil(Depsgraph *depsgraph);

/**
 * This function only does following things to make quick checks for whether Grease Pencil drawing
 * is needed:
 * - Whether Grease Pencil objects are excluded in the viewport.
 * - If any Grease Pencil typed ID exists inside the depsgraph.
 * Note: it does not to full check for cases where Grease Pencil strokes are generated within a
 * non-grease-pencil object, to do complete check, use `DRW_render_check_grease_pencil`.
 */
bool DRW_gpencil_engine_needed_viewport(Depsgraph *depsgraph, View3D *v3d);

/**
 * Render grease pencil on top of other render engine output.
 * This function creates a DRWContext.
 */
void DRW_render_gpencil(RenderEngine *engine, Depsgraph *depsgraph);

void DRW_render_context_enable(Render *render);
void DRW_render_context_disable(Render *render);

void DRW_mutexes_init();
void DRW_mutexes_exit();

/* Mutex to lock the drw manager and avoid concurrent context usage.
 * Equivalent to the old DST lock.
 * Brought back to 4.5 due to unforeseen issues causing data races and race conditions with Images
 * and GPUTextures. (See #141253) */
void DRW_lock_start();
void DRW_lock_end();

/* Critical section for GPUShader usage. Can be removed when we have threadsafe GPUShader class. */
void DRW_submission_start();
void DRW_submission_end();

void DRW_gpu_context_create();
void DRW_gpu_context_destroy();
/**
 * Binds the draw GPU context to the active thread.
 * In background mode, this will create the draw GPU context on first call.
 */
void DRW_gpu_context_enable();
/**
 * Tries to bind the draw GPU context to the active thread.
 * Returns true on success, false if the draw GPU context does not exists.
 */
bool DRW_gpu_context_try_enable();
void DRW_gpu_context_disable();

#ifdef WITH_XR_OPENXR
/* XXX: see comment on #DRW_system_gpu_context_get() */
void *DRW_system_gpu_context_get();
void *DRW_xr_blender_gpu_context_get();
void DRW_xr_drawing_begin();
void DRW_xr_drawing_end();
#endif

/** For garbage collection. */
void DRW_cache_free_old_batches(Main *bmain);

namespace blender::draw {

/** Free garbage collected subdivision data. */
void DRW_cache_free_old_subdiv();

}  // namespace blender::draw

/** Never use this. Only for closing blender. */
void DRW_gpu_context_enable_ex(bool restore);
void DRW_gpu_context_disable_ex(bool restore);

/* Render pipeline GPU context control.
 * Enable system context first, then enable blender context,
 * then disable blender context, then disable system context. */

void DRW_system_gpu_render_context_enable(void *re_system_gpu_context);
void DRW_system_gpu_render_context_disable(void *re_system_gpu_context);
void DRW_blender_gpu_render_context_enable(void *re_gpu_context);
void DRW_blender_gpu_render_context_disable(void *re_gpu_context);

DRWData *DRW_viewport_data_create();
void DRW_viewport_data_free(DRWData *drw_data);

bool DRW_gpu_context_release();
void DRW_gpu_context_activate(bool drw_state);

namespace blender::draw {

void DRW_cdlayer_attr_aliases_add(GPUVertFormat *format,
                                  const char *base_name,
                                  bke::AttrType data_type,
                                  blender::StringRef layer_name,
                                  bool is_active_render,
                                  bool is_active_layer);

}
