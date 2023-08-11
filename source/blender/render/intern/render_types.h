/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

/* ------------------------------------------------------------------------- */
/* exposed internal in render module only! */
/* ------------------------------------------------------------------------- */

#include <mutex>

#include "DNA_scene_types.h"

#include "BLI_threads.h"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

#include "tile_highlight.h"

struct bNodeTree;
struct Depsgraph;
struct GSet;
struct Main;
struct Object;
struct RenderEngine;
struct ReportList;
struct Scene;

struct BaseRender {
  BaseRender() = default;
  virtual ~BaseRender();

  /* Get class which manages highlight of tiles.
   * Note that it might not exist: for example, viewport render does not support the tile
   * highlight. */
  virtual blender::render::TilesHighlight *get_tile_highlight() = 0;

  /* GPU/realtime compositor. */
  virtual void compositor_execute(const Scene &scene,
                                  const RenderData &render_data,
                                  const bNodeTree &node_tree,
                                  const bool use_file_output,
                                  const char *view_name) = 0;
  virtual void compositor_free() = 0;

  /* Result of rendering */
  RenderResult *result = nullptr;

  /* Read/write mutex, all internal code that writes to the `result` must use a
   * write lock, all external code must use a read lock. Internal code is assumed
   * to not conflict with writes, so no lock used for that. */
  ThreadRWMutex resultmutex = BLI_RWLOCK_INITIALIZER;

  /* Render engine. */
  struct RenderEngine *engine = nullptr;

  /* Guard for drawing render result using engine's `draw()` callback. */
  ThreadMutex engine_draw_mutex = BLI_MUTEX_INITIALIZER;
};

struct ViewRender : public BaseRender {
  blender::render::TilesHighlight *get_tile_highlight() override
  {
    return nullptr;
  }

  void compositor_execute(const Scene & /*scene*/,
                          const RenderData & /*render_data*/,
                          const bNodeTree & /*node_tree*/,
                          const bool /*use_file_output*/,
                          const char * /*view_name*/) override
  {
  }
  void compositor_free() override {}
};

/* Controls state of render, everything that's read-only during render stage */
struct Render : public BaseRender {
  /* NOTE: Currently unused, provision for the future.
   * Add these now to allow the guarded memory allocator to catch C-specific function calls. */
  Render() = default;
  virtual ~Render();

  blender::render::TilesHighlight *get_tile_highlight() override
  {
    return &tile_highlight;
  }

  void compositor_execute(const Scene &scene,
                          const RenderData &render_data,
                          const bNodeTree &node_tree,
                          const bool use_file_output,
                          const char *view_name) override;
  void compositor_free() override;

  char name[RE_MAXNAME] = "";
  int slot = 0;

  /* state settings */
  short flag = 0;
  bool ok = false;

  /* if render with single-layer option, other rendered layers are stored here */
  RenderResult *pushedresult = nullptr;
  /** A list of #RenderResults, for full-samples. */
  ListBase fullresult = {nullptr, nullptr};
  /* True if result has GPU textures, to quickly skip cache clear. */
  bool result_has_gpu_texture_caches = false;

  /** Window size, display rect, viewplane.
   * \note Buffer width and height with percentage applied
   * without border & crop. convert to long before multiplying together to avoid overflow. */
  int winx = 0, winy = 0;
  rcti disprect = {0, 0, 0, 0};  /* part within winx winy */
  rctf viewplane = {0, 0, 0, 0}; /* mapped on winx winy */

  /* final picture width and height (within disprect) */
  int rectx = 0, recty = 0;

  /* Camera transform, only used by Freestyle. */
  float winmat[4][4] = {{0}};

  /* Clipping. */
  float clip_start = 0.0f;
  float clip_end = 0.0f;

  /* main, scene, and its full copy of renderdata and world */
  struct Main *main = nullptr;
  Scene *scene = nullptr;
  RenderData r = {};
  char single_view_layer[MAX_NAME] = "";
  struct Object *camera_override = nullptr;

  blender::render::TilesHighlight tile_highlight;

  /* NOTE: This is a minimal dependency graph and evaluated scene which is enough to access view
   * layer visibility and use for postprocessing (compositor and sequencer). */
  struct Depsgraph *pipeline_depsgraph = nullptr;
  Scene *pipeline_scene_eval = nullptr;

  /* Realtime GPU Compositor.
   * NOTE: Use bare pointer instead of smart pointer because the RealtimeCompositor is a fully
   * opaque type. */
  blender::render::RealtimeCompositor *gpu_compositor = nullptr;
  std::mutex gpu_compositor_mutex;

  /* callbacks */
  void (*display_init_cb)(void *handle, RenderResult *rr) = nullptr;
  void *dih = nullptr;
  void (*display_clear_cb)(void *handle, RenderResult *rr) = nullptr;
  void *dch = nullptr;
  void (*display_update_cb)(void *handle, RenderResult *rr, rcti *rect) = nullptr;
  void *duh = nullptr;
  void (*current_scene_update_cb)(void *handle, struct Scene *scene) = nullptr;
  void *suh = nullptr;

  void (*stats_draw_cb)(void *handle, RenderStats *ri) = nullptr;
  void *sdh = nullptr;
  void (*progress_cb)(void *handle, float i) = nullptr;
  void *prh = nullptr;

  void (*draw_lock_cb)(void *handle, bool lock) = nullptr;
  void *dlh = nullptr;
  bool (*test_break_cb)(void *handle) = nullptr;
  void *tbh = nullptr;

  /**
   * Executed right before the initialization of the depsgraph, in order to modify some stuff in
   * the viewlayer. The modified ids must be tagged in the depsgraph.
   */
  bool (*prepare_viewlayer_cb)(void *handle, struct ViewLayer *vl, struct Depsgraph *depsgraph);
  void *prepare_vl_handle;

  RenderStats i = {};

  /**
   * Optional report list which may be null (borrowed memory).
   * Callers to rendering functions are responsible for setting can clearing, see: #RE_SetReports.
   */
  struct ReportList *reports = nullptr;

  void **movie_ctx_arr = nullptr;
  char viewname[MAX_NAME] = "";

  /* TODO: replace by a whole draw manager. */
  void *system_gpu_context = nullptr;
  void *blender_gpu_context = nullptr;
};

/* **************** defines ********************* */

/** #R.flag */
#define R_ANIMATION 1
