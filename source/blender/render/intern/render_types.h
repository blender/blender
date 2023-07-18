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

#include "DNA_scene_types.h"

#include "BLI_threads.h"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

struct Depsgraph;
struct GSet;
struct Main;
struct Object;
struct RenderEngine;
struct ReportList;

#ifdef __cplusplus
extern "C" {
#endif

struct HighlightedTile {
  rcti rect;
};

/* controls state of render, everything that's read-only during render stage */
struct Render {
  /* NOTE: Currently unused, provision for the future.
   * Add these now to allow the guarded memory allocator to catch C-specific function calls. */
  Render() = default;
  virtual ~Render() = default;

  char name[RE_MAXNAME] = "";
  int slot = 0;

  /* state settings */
  short flag = 0;
  bool ok = false;

  /* result of rendering */
  RenderResult *result = nullptr;
  /* if render with single-layer option, other rendered layers are stored here */
  RenderResult *pushedresult = nullptr;
  /** A list of #RenderResults, for full-samples. */
  ListBase fullresult = {nullptr, nullptr};
  /* read/write mutex, all internal code that writes to re->result must use a
   * write lock, all external code must use a read lock. internal code is assumed
   * to not conflict with writes, so no lock used for that */
  ThreadRWMutex resultmutex = BLI_RWLOCK_INITIALIZER;
  /* True if result has GPU textures, to quickly skip cache clear. */
  bool result_has_gpu_texture_caches = false;

  /* Guard for drawing render result using engine's `draw()` callback. */
  ThreadMutex engine_draw_mutex = BLI_MUTEX_INITIALIZER;

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

  ThreadMutex highlighted_tiles_mutex = BLI_MUTEX_INITIALIZER;
  struct GSet *highlighted_tiles = nullptr;

  /* render engine */
  struct RenderEngine *engine = nullptr;

  /* NOTE: This is a minimal dependency graph and evaluated scene which is enough to access view
   * layer visibility and use for postprocessing (compositor and sequencer). */
  struct Depsgraph *pipeline_depsgraph = nullptr;
  Scene *pipeline_scene_eval = nullptr;

  /* Realtime GPU Compositor. */
  blender::render::RealtimeCompositor *gpu_compositor = nullptr;
  ThreadMutex gpu_compositor_mutex = BLI_MUTEX_INITIALIZER;

  /* callbacks */
  void (*display_init)(void *handle, RenderResult *rr) = nullptr;
  void *dih = nullptr;
  void (*display_clear)(void *handle, RenderResult *rr) = nullptr;
  void *dch = nullptr;
  void (*display_update)(void *handle, RenderResult *rr, rcti *rect) = nullptr;
  void *duh = nullptr;
  void (*current_scene_update)(void *handle, struct Scene *scene) = nullptr;
  void *suh = nullptr;

  void (*stats_draw)(void *handle, RenderStats *ri) = nullptr;
  void *sdh = nullptr;
  void (*progress)(void *handle, float i) = nullptr;
  void *prh = nullptr;

  void (*draw_lock)(void *handle, bool lock) = nullptr;
  void *dlh = nullptr;
  bool (*test_break)(void *handle) = nullptr;
  void *tbh = nullptr;

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

#ifdef __cplusplus
}
#endif
