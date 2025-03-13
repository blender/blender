/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/* This is the Render Functions used by Realtime engines to draw with OpenGL */

#pragma once

#include <functional>

#include "BLI_math_vector_types.hh"
#include "DNA_object_enums.h"

#include "GPU_material.hh"

namespace blender::gpu {
class Batch;
}
struct ARegion;
struct bContext;
struct Depsgraph;
struct DefaultFramebufferList;
struct DefaultTextureList;
struct DupliObject;
struct GPUMaterial;
struct GPUShader;
struct GPUTexture;
struct GPUUniformBuf;
struct Object;
struct ParticleSystem;
struct rcti;
struct RegionView3D;
struct RenderEngine;
struct RenderEngineType;
struct RenderLayer;
struct RenderResult;
struct SpaceLink;
struct TaskGraph;
struct View3D;
struct ViewLayer;
struct DRWContext;
struct World;
struct DRWData;
struct DRWViewData;
struct GPUViewport;
struct GPUFrameBuffer;
struct DRWTextStore;
struct GSet;
struct GPUViewport;
namespace blender::draw {
class TextureFromPool;
struct ObjectRef;
}  // namespace blender::draw

typedef struct DRWPass DRWPass;
typedef struct DRWShadingGroup DRWShadingGroup;
typedef struct DRWUniform DRWUniform;

/* TODO: Put it somewhere else? */
struct BoundSphere {
  float center[3], radius;
};

struct DrawEngineType {
  DrawEngineType *next, *prev;

  char idname[32];

  void (*engine_init)(void *vedata);
  void (*engine_free)();

  void (*instance_free)(void *instance_data);

  void (*cache_init)(void *vedata);
  void (*cache_populate)(void *vedata, blender::draw::ObjectRef &ob_ref);
  void (*cache_finish)(void *vedata);

  void (*draw_scene)(void *vedata);

  void (*render_to_image)(void *vedata,
                          RenderEngine *engine,
                          RenderLayer *layer,
                          const rcti *rect);
  void (*store_metadata)(void *vedata, RenderResult *render_result);
};

/* Shaders */
/** IMPORTANT: Modify the currently bound context. */
void DRW_shader_init();
void DRW_shader_exit();

GPUMaterial *DRW_shader_from_world(World *wo,
                                   bNodeTree *ntree,
                                   eGPUMaterialEngine engine,
                                   const uint64_t shader_id,
                                   const bool is_volume_shader,
                                   bool deferred,
                                   GPUCodegenCallbackFn callback,
                                   void *thunk);
GPUMaterial *DRW_shader_from_material(
    Material *ma,
    bNodeTree *ntree,
    eGPUMaterialEngine engine,
    const uint64_t shader_id,
    const bool is_volume_shader,
    bool deferred,
    GPUCodegenCallbackFn callback,
    void *thunk,
    GPUMaterialPassReplacementCallbackFn pass_replacement_cb = nullptr);
void DRW_shader_queue_optimize_material(GPUMaterial *mat);

/* Viewport. */

blender::float2 DRW_viewport_size_get();

DefaultFramebufferList *DRW_viewport_framebuffer_list_get();
DefaultTextureList *DRW_viewport_texture_list_get();

/* See DRW_viewport_pass_texture_get. */
blender::draw::TextureFromPool &DRW_viewport_pass_texture_get(const char *pass_name);

void DRW_viewport_request_redraw();

void DRW_render_to_image(RenderEngine *engine, Depsgraph *depsgraph);
void DRW_render_object_iter(void *vedata,
                            RenderEngine *engine,
                            Depsgraph *depsgraph,
                            void (*callback)(void *vedata,
                                             blender::draw::ObjectRef &ob_ref,
                                             RenderEngine *engine,
                                             Depsgraph *depsgraph));

/**
 * \warning Changing frame might free the #ViewLayerEngineData.
 */
void DRW_render_set_time(RenderEngine *engine, Depsgraph *depsgraph, int frame, float subframe);

/**
 * Assume a valid GL context is bound (and that the gl_context_mutex has been acquired).
 * This function only setup DST and execute the given function.
 * \warning similar to DRW_render_to_image you cannot use default lists (`dfbl` & `dtxl`).
 */
void DRW_custom_pipeline_begin(DRWContext &draw_ctx,
                               DrawEngineType *draw_engine_type,
                               Depsgraph *depsgraph);
void DRW_custom_pipeline_end(DRWContext &draw_ctx);

/**
 * Used when the render engine want to redo another cache populate inside the same render frame.
 * Assumes it is called between `DRW_custom_pipeline_begin/end()`.
 */
void DRW_cache_restart();

/* ViewLayers */

void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type);
void **DRW_view_layer_engine_data_ensure_ex(ViewLayer *view_layer,
                                            DrawEngineType *engine_type,
                                            void (*callback)(void *storage));
void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type,
                                         void (*callback)(void *storage));

/* DrawData */

DrawData *DRW_drawdata_get(ID *id, DrawEngineType *engine_type);
DrawData *DRW_drawdata_ensure(ID *id,
                              DrawEngineType *engine_type,
                              size_t size,
                              DrawDataInitCb init_cb,
                              DrawDataFreeCb free_cb);

/* Settings. */

bool DRW_object_is_renderable(const Object *ob);
/**
 * Does `ob` needs to be rendered in edit mode.
 *
 * When using duplicate linked meshes, objects that are not in edit-mode will be drawn as
 * it is in edit mode, when another object with the same mesh is in edit mode.
 * This will not be the case when one of the objects are influenced by modifiers.
 */
bool DRW_object_is_in_edit_mode(const Object *ob);
/**
 * Return whether this object is visible depending if
 * we are rendering or drawing in the viewport.
 */
int DRW_object_visibility_in_active_context(const Object *ob);
bool DRW_object_use_hide_faces(const Object *ob);

bool DRW_object_is_visible_psys_in_active_context(const Object *object,
                                                  const ParticleSystem *psys);

Object *DRW_object_get_dupli_parent(const Object *ob);
DupliObject *DRW_object_get_dupli(const Object *ob);

/* Draw State. */

/* -------------------------------------------------------------------- */
/** \name Draw Context
 * \{ */

struct DRWContext {
 private:
  /** Render State: No persistent data between draw calls. */
  static thread_local DRWContext *g_context;

  /* TODO(fclem): Private? */
 public:
  /* TODO: clean up this struct a bit. */
  /* Cache generation */
  DRWData *data = nullptr;
  /** Active view data structure for one of the 2 stereo view. */
  DRWViewData *view_data_active = nullptr;

  /* Optional associated viewport. Can be nullptr. */
  GPUViewport *viewport = nullptr;
  /* Size of the viewport or the final render frame. */
  blender::float2 size = {0, 0};
  blender::float2 inv_size = {0, 0};

  /* Returns the viewport's default framebuffer. */
  GPUFrameBuffer *default_framebuffer();

  const enum Mode {
    /* Render for display of 2D or 3D area. Runs on main thread. */
    VIEWPORT = 0,

    /* These viewport modes will render without some overlays (i.e. no text). */

    /* Render for a 3D viewport in XR. Runs on main thread. */
    VIEWPORT_XR,
    /* Render for a 3D viewport offscreen render (python). Runs on main thread. */
    VIEWPORT_OFFSCREEN,
    /* Render for a 3D viewport image render (render preview). Runs on main thread. */
    VIEWPORT_RENDER,

    /* Render for object mode selection. Runs on main thread. */
    SELECT_OBJECT,
    /* Render for object material selection. Runs on main thread. */
    SELECT_OBJECT_MATERIAL,
    /* Render for edit mesh selection. Runs on main thread. */
    SELECT_EDIT_MESH,

    /* Render for depth picking (auto-depth). Runs on main thread. */
    DEPTH,

    /* Render for F12 final render. Can run in any thread. */
    RENDER,
    /* Used by custom pipeline. Can run in any thread. */
    CUSTOM,
  } mode;

  struct {
    bool draw_background = false;
    bool draw_text = false;
  } options;

  /* Convenience pointer to text_store owned by the viewport */
  DRWTextStore **text_store_p = nullptr;

  /* Contains list of objects that needs to be extracted from other objects. */
  GSet *delayed_extraction = nullptr;

  /* TODO(fclem): Public. */

  /* Current rendering context. Avoid too many lookups while drawing. */

  /* Evaluated Depsgraph. */
  Depsgraph *depsgraph = nullptr;
  /* Evaluated Scene. */
  Scene *scene = nullptr;
  /* Evaluated ViewLayer. */
  ViewLayer *view_layer = nullptr;

  /** Last resort (some functions take this as an arg so we can't easily avoid).
   * May be nullptr when used for selection or depth buffer. */
  const bContext *evil_C = nullptr;
  /* Can be nullptr depending on context. */
  ARegion *region = nullptr;
  /* Can be nullptr depending on context. */
  SpaceLink *space_data = nullptr;
  /* Can be nullptr depending on context. */
  RegionView3D *rv3d = nullptr;
  /* Can be nullptr depending on context. */
  View3D *v3d = nullptr;
  /* Use 'object_edit' for edit-mode */
  Object *obact = nullptr;
  Object *object_pose = nullptr;
  Object *object_edit = nullptr;

  eObjectMode object_mode = OB_MODE_OBJECT;

 public:
  /**
   * If `viewport` is not specified, `DRWData` will be considered temporary and discarded on exit.
   * If `C` is nullptr, it means that the context is **not** associated with any UI or operator.
   * If `region` is nullptr, it will be sourced from the context `C` or left as nullptr otherwise.
   * If `v3d` is nullptr, it will be sourced from the context `C` or left as nullptr otherwise.
   */
  DRWContext(Mode mode,
             Depsgraph *depsgraph,
             GPUViewport *viewport,
             const bContext *C = nullptr,
             ARegion *region = nullptr,
             View3D *v3d = nullptr);
  DRWContext(Mode mode,
             Depsgraph *depsgraph,
             const blender::int2 size = {1, 1},
             const bContext *C = nullptr,
             ARegion *region = nullptr,
             View3D *v3d = nullptr);

  ~DRWContext();

  /**
   * Acquire `data` and `view_data_active`.
   * Needs to be called before enabling any draw engine.
   * IMPORTANT: This can be called multiple times before release_data.
   * IMPORTANT: This must be called with an active GPUContext.
   */
  void acquire_data();

  /**
   * Make sure to release acquired DRWData. If created on the fly, make sure to destroy them.
   * IMPORTANT: This needs to be called with the same active GPUContext `acquire_data()` was called
   * with.
   */
  void release_data();

  /**
   * Enable engines from context. Not needed for Mode::RENDER and Mode::CUSTOM.
   *
   * `render_engine_type` specify the engine to use in OB_MATERIAL or OB_RENDER modes.
   * `gpencil_engine_needed` should be set to true if the grease pencil engine is needed.
   */
  void enable_engines(bool gpencil_engine_needed = false,
                      RenderEngineType *render_engine_type = nullptr);

  /* Free unused engine data. */
  void engines_data_validate();

  using iter_callback_t =
      std::function<void(struct DupliCacheManager &, struct ExtractionGraph &)>;

  /* Run the sync phase with data extraction. iter_callback defines which object to sync. */
  void sync(iter_callback_t iter_callback);
  /* Run enabled engine init and sync callbacks. iter_callback defines which object to sync. */
  void engines_init_and_sync(iter_callback_t iter_callback);
  /* Run enabled engine init and draw scene callbacks. */
  void engines_draw_scene();

  static DRWContext &get_active()
  {
    return *g_context;
  }

  /* Return true if any DRWContext is active on this thread. */
  static bool is_active()
  {
    return g_context != nullptr;
  }

  bool is_select() const
  {
    return ELEM(mode, SELECT_OBJECT, SELECT_OBJECT_MATERIAL, SELECT_EDIT_MESH);
  }
  bool is_material_select() const
  {
    return ELEM(mode, SELECT_OBJECT_MATERIAL);
  }
  bool is_depth() const
  {
    return ELEM(mode, DEPTH);
  }
  bool is_image_render() const
  {
    return ELEM(mode, VIEWPORT_RENDER, RENDER);
  }
  bool is_scene_render() const
  {
    return ELEM(mode, RENDER);
  }
  bool is_viewport_image_render() const
  {
    return ELEM(mode, VIEWPORT_RENDER);
  }
};

/** \} */

const DRWContext *DRW_context_get();

/**
 * For when engines need to know if this is drawing for selection or not.
 */
static inline bool DRW_state_is_select()
{
  return DRWContext::get_active().is_select();
}

/**
 * For when engines need to know if this is drawing for selection or not.
 */
static inline bool DRW_state_is_material_select()
{
  return DRWContext::get_active().is_material_select();
}

/**
 * For when engines need to know if this is drawing for depth picking.
 */
static inline bool DRW_state_is_depth()
{
  return DRWContext::get_active().is_depth();
}

/**
 * Whether we are rendering for an image
 */
static inline bool DRW_state_is_image_render()
{
  return DRWContext::get_active().is_image_render();
  ;
}

/**
 * Whether we are rendering only the render engine,
 * or if we should also render the mode engines.
 */
static inline bool DRW_state_is_scene_render()
{
  return DRWContext::get_active().is_scene_render();
}

/**
 * Whether we are rendering simple opengl render
 */
static inline bool DRW_state_is_viewport_image_render()
{
  return DRWContext::get_active().is_viewport_image_render();
}

bool DRW_state_is_playback();
/**
 * Is the user navigating or painting the region.
 */
bool DRW_state_is_navigating();
/**
 * Is the user painting?
 */
bool DRW_state_is_painting();
/**
 * Should text draw in this mode?
 */
bool DRW_state_show_text();
/**
 * Should draw support elements
 * Objects center, selection outline, probe data, ...
 */
bool DRW_state_draw_support();
/**
 * Whether we should render the background
 */
bool DRW_state_draw_background();

bool DRW_state_viewport_compositor_enabled();
