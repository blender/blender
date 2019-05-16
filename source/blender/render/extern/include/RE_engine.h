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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup render
 */

#ifndef __RE_ENGINE_H__
#define __RE_ENGINE_H__

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "RNA_types.h"
#include "RE_bake.h"

#include "BLI_threads.h"

struct BakePixel;
struct Depsgraph;
struct IDProperty;
struct Main;
struct Object;
struct Render;
struct RenderData;
struct RenderEngine;
struct RenderEngineType;
struct RenderLayer;
struct RenderResult;
struct ReportList;
struct Scene;
struct ViewLayer;
struct bNode;
struct bNodeTree;

/* External Engine */

/* RenderEngineType.flag */
#define RE_INTERNAL 1
/* #define RE_FLAG_DEPRECATED   2 */
#define RE_USE_PREVIEW 4
#define RE_USE_POSTPROCESS 8
#define RE_USE_SHADING_NODES 16
#define RE_USE_EXCLUDE_LAYERS 32
#define RE_USE_SAVE_BUFFERS 64
#define RE_USE_SHADING_NODES_CUSTOM 256
#define RE_USE_SPHERICAL_STEREO 512

/* RenderEngine.flag */
#define RE_ENGINE_ANIMATION 1
#define RE_ENGINE_PREVIEW 2
#define RE_ENGINE_DO_DRAW 4
#define RE_ENGINE_DO_UPDATE 8
#define RE_ENGINE_RENDERING 16
#define RE_ENGINE_HIGHLIGHT_TILES 32
#define RE_ENGINE_USED_FOR_VIEWPORT 64

extern ListBase R_engines;

typedef struct RenderEngineType {
  struct RenderEngineType *next, *prev;

  /* type info */
  char idname[64];  // best keep the same size as BKE_ST_MAXNAME
  char name[64];
  int flag;

  void (*update)(struct RenderEngine *engine, struct Main *bmain, struct Depsgraph *depsgraph);
  void (*render)(struct RenderEngine *engine, struct Depsgraph *depsgraph);
  void (*bake)(struct RenderEngine *engine,
               struct Depsgraph *depsgraph,
               struct Object *object,
               const int pass_type,
               const int pass_filter,
               const int object_id,
               const struct BakePixel *pixel_array,
               const int num_pixels,
               const int depth,
               void *result);

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

  struct DrawEngineType *draw_engine;

  /* RNA integration */
  ExtensionRNA ext;
} RenderEngineType;

typedef void (*update_render_passes_cb_t)(void *userdata,
                                          struct Scene *scene,
                                          struct ViewLayer *view_layer,
                                          const char *name,
                                          int channels,
                                          const char *chanid,
                                          int type);

typedef struct RenderEngine {
  RenderEngineType *type;
  void *py_instance;

  int flag;
  struct Object *camera_override;
  unsigned int layer_override;

  int tile_x;
  int tile_y;

  struct Render *re;
  ListBase fullresult;
  char text[512]; /* IMA_MAX_RENDER_TEXT */

  int resolution_x, resolution_y;

  struct ReportList *reports;

  /* Depsgraph */
  struct Depsgraph *depsgraph;

  /* callback for render pass query */
  ThreadMutex update_render_passes_mutex;
  update_render_passes_cb_t update_render_passes_cb;
  void *update_render_passes_data;

  rctf last_viewplane;
  rcti last_disprect;
  float last_viewmat[4][4];
  int last_winx, last_winy;
} RenderEngine;

RenderEngine *RE_engine_create(RenderEngineType *type);
RenderEngine *RE_engine_create_ex(RenderEngineType *type, bool use_for_viewport);
void RE_engine_free(RenderEngine *engine);

void RE_layer_load_from_file(
    struct RenderLayer *layer, struct ReportList *reports, const char *filename, int x, int y);
void RE_result_load_from_file(struct RenderResult *result,
                              struct ReportList *reports,
                              const char *filename);

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

const char *RE_engine_active_view_get(RenderEngine *engine);
void RE_engine_active_view_set(RenderEngine *engine, const char *viewname);
float RE_engine_get_camera_shift_x(RenderEngine *engine,
                                   struct Object *camera,
                                   bool use_spherical_stereo);
void RE_engine_get_camera_model_matrix(RenderEngine *engine,
                                       struct Object *camera,
                                       bool use_spherical_stereo,
                                       float *r_modelmat);
bool RE_engine_get_spherical_stereo(RenderEngine *engine, struct Object *camera);

bool RE_engine_test_break(RenderEngine *engine);
void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info);
void RE_engine_update_progress(RenderEngine *engine, float progress);
void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak);
void RE_engine_report(RenderEngine *engine, int type, const char *msg);
void RE_engine_set_error_message(RenderEngine *engine, const char *msg);

int RE_engine_render(struct Render *re, int do_all);

bool RE_engine_is_external(struct Render *re);

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
                             int type);

/* Engine Types */

void RE_engines_init(void);
void RE_engines_exit(void);
void RE_engines_register(RenderEngineType *render_type);

bool RE_engine_is_opengl(RenderEngineType *render_type);

RenderEngineType *RE_engines_find(const char *idname);

rcti *RE_engine_get_current_tiles(struct Render *re, int *r_total_tiles, bool *r_needs_free);
struct RenderData *RE_engine_get_render_data(struct Render *re);
void RE_bake_engine_set_engine_parameters(struct Render *re,
                                          struct Main *bmain,
                                          struct Scene *scene);

void RE_engine_free_blender_memory(struct RenderEngine *engine);

#endif /* __RE_ENGINE_H__ */
