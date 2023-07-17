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
  struct Render *next, *prev;
  char name[RE_MAXNAME];
  int slot;

  /* state settings */
  short flag;
  bool ok, result_ok;

  /* result of rendering */
  RenderResult *result;
  /* if render with single-layer option, other rendered layers are stored here */
  RenderResult *pushedresult;
  /** A list of #RenderResults, for full-samples. */
  ListBase fullresult;
  /* read/write mutex, all internal code that writes to re->result must use a
   * write lock, all external code must use a read lock. internal code is assumed
   * to not conflict with writes, so no lock used for that */
  ThreadRWMutex resultmutex;
  /* True if result has GPU textures, to quickly skip cache clear. */
  bool result_has_gpu_texture_caches;

  /* Guard for drawing render result using engine's `draw()` callback. */
  ThreadMutex engine_draw_mutex;

  /** Window size, display rect, viewplane.
   * \note Buffer width and height with percentage applied
   * without border & crop. convert to long before multiplying together to avoid overflow. */
  int winx, winy;
  rcti disprect;  /* part within winx winy */
  rctf viewplane; /* mapped on winx winy */

  /* final picture width and height (within disprect) */
  int rectx, recty;

  /* Camera transform, only used by Freestyle. */
  float winmat[4][4];

  /* Clipping. */
  float clip_start;
  float clip_end;

  /* main, scene, and its full copy of renderdata and world */
  struct Main *main;
  Scene *scene;
  RenderData r;
  char single_view_layer[MAX_NAME];
  struct Object *camera_override;

  ThreadMutex highlighted_tiles_mutex;
  struct GSet *highlighted_tiles;

  /* render engine */
  struct RenderEngine *engine;

  /* NOTE: This is a minimal dependency graph and evaluated scene which is enough to access view
   * layer visibility and use for postprocessing (compositor and sequencer). */
  struct Depsgraph *pipeline_depsgraph;
  Scene *pipeline_scene_eval;

  /* Realtime GPU Compositor. */
  blender::render::RealtimeCompositor *gpu_compositor;
  ThreadMutex gpu_compositor_mutex;

  /* callbacks */
  void (*display_init)(void *handle, RenderResult *rr);
  void *dih;
  void (*display_clear)(void *handle, RenderResult *rr);
  void *dch;
  void (*display_update)(void *handle, RenderResult *rr, rcti *rect);
  void *duh;
  void (*current_scene_update)(void *handle, struct Scene *scene);
  void *suh;

  void (*stats_draw)(void *handle, RenderStats *ri);
  void *sdh;
  void (*progress)(void *handle, float i);
  void *prh;

  void (*draw_lock)(void *handle, bool lock);
  void *dlh;
  bool (*test_break)(void *handle);
  void *tbh;

  RenderStats i;

  /**
   * Optional report list which may be null (borrowed memory).
   * Callers to rendering functions are responsible for setting can clearing, see: #RE_SetReports.
   */
  struct ReportList *reports;

  void **movie_ctx_arr;
  char viewname[MAX_NAME];

  /* TODO: replace by a whole draw manager. */
  void *system_gpu_context;
  void *blender_gpu_context;
};

/* **************** defines ********************* */

/** #R.flag */
#define R_ANIMATION 1

#ifdef __cplusplus
}
#endif
