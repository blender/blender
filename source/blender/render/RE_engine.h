/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "DNA_listBase.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "RE_bake.h"
#include "RNA_types.hh"

#include "BLI_threads.h"

struct BakeTargets;
struct BakePixel;
struct Depsgraph;
struct GPUContext;
struct Main;
struct Object;
struct Render;
struct RenderData;
struct RenderEngine;
struct RenderEngineType;
struct RenderLayer;
struct RenderPass;
struct RenderResult;
struct ReportList;
struct Scene;
struct ViewLayer;
struct ViewRender;
struct bNode;
struct bNodeTree;

/* External Engine */

/** #RenderEngineType.flag */
enum RenderEngineTypeFlag {
  RE_INTERNAL = (1 << 0),
  RE_USE_PREVIEW = (1 << 1),
  RE_USE_POSTPROCESS = (1 << 2),
  RE_USE_EEVEE_VIEWPORT = (1 << 3),
  RE_USE_SHADING_NODES_CUSTOM = (1 << 4),
  RE_USE_SPHERICAL_STEREO = (1 << 5),
  RE_USE_STEREO_VIEWPORT = (1 << 6),
  RE_USE_GPU_CONTEXT = (1 << 7),
  RE_USE_CUSTOM_FREESTYLE = (1 << 8),
  RE_USE_NO_IMAGE_SAVE = (1 << 9),
  RE_USE_MATERIALX = (1 << 10),
};

/** #RenderEngine.flag */
enum RenderEngineFlag {
  RE_ENGINE_ANIMATION = (1 << 0),
  RE_ENGINE_PREVIEW = (1 << 1),
  RE_ENGINE_DO_DRAW = (1 << 2),
  RE_ENGINE_DO_UPDATE = (1 << 3),
  RE_ENGINE_RENDERING = (1 << 4),
  RE_ENGINE_HIGHLIGHT_TILES = (1 << 5),
  RE_ENGINE_CAN_DRAW = (1 << 6),
};

extern ListBase R_engines;

struct RenderEngineType {
  struct RenderEngineType *next, *prev;

  /* Type info. */
  char idname[/*BKE_ST_MAXNAME*/ 64];
  char name[64];
  int flag;

  void (*update)(struct RenderEngine *engine, struct Main *bmain, struct Depsgraph *depsgraph);

  void (*render)(struct RenderEngine *engine, struct Depsgraph *depsgraph);

  /* Offline rendering is finished - no more view layers will be rendered.
   *
   * All the pending data is to be communicated from the engine back to Blender. In a possibly
   * most memory-efficient manner (engine might free its database before making Blender to allocate
   * full-frame render result). */
  void (*render_frame_finish)(struct RenderEngine *engine);

  void (*draw)(struct RenderEngine *engine,
               const struct bContext *context,
               struct Depsgraph *depsgraph);

  void (*bake)(struct RenderEngine *engine,
               struct Depsgraph *depsgraph,
               struct Object *object,
               int pass_type,
               int pass_filter,
               int width,
               int height);

  void (*view_update)(struct RenderEngine *engine,
                      const struct bContext *context,
                      struct Depsgraph *depsgraph);
  void (*view_draw)(struct RenderEngine *engine,
                    const struct bContext *context,
                    struct Depsgraph *depsgraph);

  void (*update_script_node)(struct RenderEngine *engine,
                             struct bNodeTree *ntree,
                             struct bNode *node);
  void (*update_render_passes)(struct RenderEngine *engine,
                               struct Scene *scene,
                               struct ViewLayer *view_layer);
  void (*update_custom_camera)(struct RenderEngine *engine, struct Camera *cam);

  struct DrawEngineType *draw_engine;

  /* RNA integration */
  ExtensionRNA rna_ext;
};

using update_render_passes_cb_t = void (*)(void *userdata,
                                           struct Scene *scene,
                                           struct ViewLayer *view_layer,
                                           const char *name,
                                           int channels,
                                           const char *chanid,
                                           eNodeSocketDatatype type);

struct RenderEngine {
  RenderEngineType *type;
  void *py_instance;

  int flag;
  struct Object *camera_override;
  unsigned int layer_override;

  struct Render *re;
  ListBase fullresult;
  char text[/*IMA_MAX_RENDER_TEXT_SIZE*/ 512];

  int resolution_x, resolution_y;

  struct ReportList *reports;

  struct {
    const struct BakeTargets *targets;
    const struct BakePixel *pixels;
    float *result;
    int image_id;
    int object_id;
  } bake;

  /* Depsgraph */
  struct Depsgraph *depsgraph;
  bool has_grease_pencil;

  /* callback for render pass query */
  ThreadMutex update_render_passes_mutex;
  update_render_passes_cb_t update_render_passes_cb;
  void *update_render_passes_data;

  /* GPU context. */
  void *system_gpu_context; /* WindowManager GPU context -> GHOSTContext. */
  ThreadMutex blender_gpu_context_mutex;
  bool use_drw_render_context;
  struct GPUContext *blender_gpu_context;
  /* Whether to restore DRWState after RenderEngine display pass. */
  bool gpu_restore_context;
};

RenderEngine *RE_engine_create(RenderEngineType *type);
void RE_engine_free(RenderEngine *engine);

/**
 * Loads in image into a result, size must match
 * x/y offsets are only used on a partial copy when dimensions don't match.
 */
void RE_layer_load_from_file(
    struct RenderLayer *layer, struct ReportList *reports, const char *filepath, int x, int y);
void RE_result_load_from_file(struct RenderResult *result,
                              struct ReportList *reports,
                              const char *filepath);

struct RenderResult *RE_engine_begin_result(
    RenderEngine *engine, int x, int y, int w, int h, const char *layername, const char *viewname);
void RE_engine_update_result(RenderEngine *engine, struct RenderResult *result);
void RE_engine_add_pass(RenderEngine *engine,
                        const char *name,
                        int channels,
                        const char *chan_id,
                        const char *layername);
void RE_engine_end_result(RenderEngine *engine,
                          struct RenderResult *result,
                          bool cancel,
                          bool highlight,
                          bool merge_results);
struct RenderResult *RE_engine_get_result(struct RenderEngine *engine);

struct RenderPass *RE_engine_pass_by_index_get(struct RenderEngine *engine,
                                               const char *layer_name,
                                               int index);

const char *RE_engine_active_view_get(RenderEngine *engine);
void RE_engine_active_view_set(RenderEngine *engine, const char *viewname);
float RE_engine_get_camera_shift_x(RenderEngine *engine,
                                   struct Object *camera,
                                   bool use_spherical_stereo);
void RE_engine_get_camera_model_matrix(RenderEngine *engine,
                                       struct Object *camera,
                                       bool use_spherical_stereo,
                                       float r_modelmat[16]);
bool RE_engine_get_spherical_stereo(RenderEngine *engine, struct Object *camera);

bool RE_engine_test_break(RenderEngine *engine);
void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info);
void RE_engine_update_progress(RenderEngine *engine, float progress);
void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak);
void RE_engine_report(RenderEngine *engine, int type, const char *msg);
void RE_engine_set_error_message(RenderEngine *engine, const char *msg);

bool RE_engine_render(struct Render *re, bool do_all);

bool RE_engine_is_external(const struct Render *re);

void RE_engine_frame_set(struct RenderEngine *engine, int frame, float subframe);

void RE_engine_update_render_passes(struct RenderEngine *engine,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    update_render_passes_cb_t callback,
                                    void *callback_data);
void RE_engine_register_pass(struct RenderEngine *engine,
                             struct Scene *scene,
                             struct ViewLayer *view_layer,
                             const char *name,
                             int channels,
                             const char *chanid,
                             eNodeSocketDatatype type);

bool RE_engine_use_persistent_data(struct RenderEngine *engine);

struct RenderEngine *RE_engine_get(const struct Render *re);
struct RenderEngine *RE_view_engine_get(const struct ViewRender *view_render);

/**
 * Acquire render engine for drawing via its `draw()` callback.
 *
 * If drawing is not possible false is returned. If drawing is possible then the engine is
 * "acquired" so that it can not be freed by the render pipeline.
 *
 * Drawing is possible if the engine has the `draw()` callback and it is in its `render()`
 * callback.
 */
bool RE_engine_draw_acquire(struct Render *re);
void RE_engine_draw_release(struct Render *re);

/**
 * GPU context for engine to create and update GPU resources in its own thread,
 * without blocking the main thread. Used by Cycles' display driver to create
 * display textures.
 */
bool RE_engine_gpu_context_create(struct RenderEngine *engine);
void RE_engine_gpu_context_destroy(struct RenderEngine *engine);

bool RE_engine_gpu_context_enable(struct RenderEngine *engine);
void RE_engine_gpu_context_disable(struct RenderEngine *engine);

void RE_engine_gpu_context_lock(struct RenderEngine *engine);
void RE_engine_gpu_context_unlock(struct RenderEngine *engine);

/* Engine Types */

void RE_engines_init(void);
void RE_engines_exit(void);
void RE_engines_register(RenderEngineType *render_type);

RenderEngineType *RE_engines_find(const char *idname);

const rcti *RE_engine_get_current_tiles(struct Render *re, int *r_total_tiles);
struct RenderData *RE_engine_get_render_data(struct Render *re);
void RE_bake_engine_set_engine_parameters(struct Render *re,
                                          struct Main *bmain,
                                          struct Scene *scene);

void RE_engine_free_blender_memory(struct RenderEngine *engine);

void RE_engine_tile_highlight_set(
    struct RenderEngine *engine, int x, int y, int width, int height, bool highlight);
void RE_engine_tile_highlight_clear_all(struct RenderEngine *engine);
