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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_global.h"
#include "BKE_colortools.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "RE_bake.h"

#include "DRW_engine.h"

#include "initrender.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "render_result.h"

/* Render Engine Types */

ListBase R_engines = {NULL, NULL};

void RE_engines_init(void)
{
  DRW_engines_register();
}

void RE_engines_exit(void)
{
  RenderEngineType *type, *next;

  DRW_engines_free();

  for (type = R_engines.first; type; type = next) {
    next = type->next;

    BLI_remlink(&R_engines, type);

    if (!(type->flag & RE_INTERNAL)) {
      if (type->ext.free) {
        type->ext.free(type->ext.data);
      }

      MEM_freeN(type);
    }
  }
}

void RE_engines_register(RenderEngineType *render_type)
{
  if (render_type->draw_engine) {
    DRW_engine_register(render_type->draw_engine);
  }
  BLI_addtail(&R_engines, render_type);
}

RenderEngineType *RE_engines_find(const char *idname)
{
  RenderEngineType *type;

  type = BLI_findstring(&R_engines, idname, offsetof(RenderEngineType, idname));
  if (!type) {
    type = BLI_findstring(&R_engines, "BLENDER_EEVEE", offsetof(RenderEngineType, idname));
  }

  return type;
}

bool RE_engine_is_external(Render *re)
{
  return (re->engine && re->engine->type && re->engine->type->render);
}

bool RE_engine_is_opengl(RenderEngineType *render_type)
{
  /* TODO refine? Can we have ogl render engine without ogl render pipeline? */
  return (render_type->draw_engine != NULL) && DRW_engine_render_support(render_type->draw_engine);
}

/* Create, Free */

RenderEngine *RE_engine_create(RenderEngineType *type)
{
  return RE_engine_create_ex(type, false);
}

RenderEngine *RE_engine_create_ex(RenderEngineType *type, bool use_for_viewport)
{
  RenderEngine *engine = MEM_callocN(sizeof(RenderEngine), "RenderEngine");
  engine->type = type;

  if (use_for_viewport) {
    engine->flag |= RE_ENGINE_USED_FOR_VIEWPORT;

    BLI_threaded_malloc_begin();
  }

  BLI_mutex_init(&engine->update_render_passes_mutex);

  return engine;
}

void RE_engine_free(RenderEngine *engine)
{
#ifdef WITH_PYTHON
  if (engine->py_instance) {
    BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
  }
#endif

  if (engine->flag & RE_ENGINE_USED_FOR_VIEWPORT) {
    BLI_threaded_malloc_end();
  }

  BLI_mutex_end(&engine->update_render_passes_mutex);

  MEM_freeN(engine);
}

/* Render Results */

static RenderPart *get_part_from_result(Render *re, RenderResult *result)
{
  rcti key = result->tilerect;
  BLI_rcti_translate(&key, re->disprect.xmin, re->disprect.ymin);

  return BLI_ghash_lookup(re->parts, &key);
}

RenderResult *RE_engine_begin_result(
    RenderEngine *engine, int x, int y, int w, int h, const char *layername, const char *viewname)
{
  Render *re = engine->re;
  RenderResult *result;
  rcti disprect;

  /* ensure the coordinates are within the right limits */
  CLAMP(x, 0, re->result->rectx);
  CLAMP(y, 0, re->result->recty);
  CLAMP(w, 0, re->result->rectx);
  CLAMP(h, 0, re->result->recty);

  if (x + w > re->result->rectx) {
    w = re->result->rectx - x;
  }
  if (y + h > re->result->recty) {
    h = re->result->recty - y;
  }

  /* allocate a render result */
  disprect.xmin = x;
  disprect.xmax = x + w;
  disprect.ymin = y;
  disprect.ymax = y + h;

  result = render_result_new(re, &disprect, 0, RR_USE_MEM, layername, viewname);

  /* todo: make this thread safe */

  /* can be NULL if we CLAMP the width or height to 0 */
  if (result) {
    render_result_clone_passes(re, result, viewname);

    RenderPart *pa;

    /* Copy EXR tile settings, so pipeline knows whether this is a result
     * for Save Buffers enabled rendering.
     */
    result->do_exr_tile = re->result->do_exr_tile;

    BLI_addtail(&engine->fullresult, result);

    result->tilerect.xmin += re->disprect.xmin;
    result->tilerect.xmax += re->disprect.xmin;
    result->tilerect.ymin += re->disprect.ymin;
    result->tilerect.ymax += re->disprect.ymin;

    pa = get_part_from_result(re, result);

    if (pa) {
      pa->status = PART_STATUS_IN_PROGRESS;
    }
  }

  return result;
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
  Render *re = engine->re;

  if (result) {
    result->renlay = result->layers.first; /* weak, draws first layer always */
    re->display_update(re->duh, result, NULL);
  }
}

void RE_engine_add_pass(RenderEngine *engine,
                        const char *name,
                        int channels,
                        const char *chan_id,
                        const char *layername)
{
  Render *re = engine->re;

  if (!re || !re->result) {
    return;
  }

  render_result_add_pass(re->result, name, channels, chan_id, layername, NULL);
}

void RE_engine_end_result(
    RenderEngine *engine, RenderResult *result, bool cancel, bool highlight, bool merge_results)
{
  Render *re = engine->re;

  if (!result) {
    return;
  }

  /* merge. on break, don't merge in result for preview renders, looks nicer */
  if (!highlight) {
    /* for exr tile render, detect tiles that are done */
    RenderPart *pa = get_part_from_result(re, result);

    if (pa) {
      pa->status = (!cancel && merge_results) ? PART_STATUS_MERGED : PART_STATUS_RENDERED;
    }
    else if (re->result->do_exr_tile) {
      /* if written result does not match any tile and we are using save
       * buffers, we are going to get openexr save errors */
      fprintf(stderr, "RenderEngine.end_result: dimensions do not match any OpenEXR tile.\n");
    }
  }

  if (!cancel || merge_results) {
    if (re->result->do_exr_tile) {
      if (!cancel && merge_results) {
        render_result_exr_file_merge(re->result, result, re->viewname);
      }
    }
    else if (!(re->test_break(re->tbh) && (re->r.scemode & R_BUTS_PREVIEW))) {
      render_result_merge(re->result, result);
    }

    /* draw */
    if (!re->test_break(re->tbh)) {
      result->renlay = result->layers.first; /* weak, draws first layer always */
      re->display_update(re->duh, result, NULL);
    }
  }

  /* free */
  BLI_remlink(&engine->fullresult, result);
  render_result_free(result);
}

RenderResult *RE_engine_get_result(RenderEngine *engine)
{
  return engine->re->result;
}

/* Cancel */

bool RE_engine_test_break(RenderEngine *engine)
{
  Render *re = engine->re;

  if (re) {
    return re->test_break(re->tbh);
  }

  return 0;
}

/* Statistics */

void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info)
{
  Render *re = engine->re;

  /* stats draw callback */
  if (re) {
    re->i.statstr = stats;
    re->i.infostr = info;
    re->stats_draw(re->sdh, &re->i);
    re->i.infostr = NULL;
    re->i.statstr = NULL;
  }

  /* set engine text */
  engine->text[0] = '\0';

  if (stats && stats[0] && info && info[0]) {
    BLI_snprintf(engine->text, sizeof(engine->text), "%s | %s", stats, info);
  }
  else if (info && info[0]) {
    BLI_strncpy(engine->text, info, sizeof(engine->text));
  }
  else if (stats && stats[0]) {
    BLI_strncpy(engine->text, stats, sizeof(engine->text));
  }
}

void RE_engine_update_progress(RenderEngine *engine, float progress)
{
  Render *re = engine->re;

  if (re) {
    CLAMP(progress, 0.0f, 1.0f);
    re->progress(re->prh, progress);
  }
}

void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak)
{
  Render *re = engine->re;

  if (re) {
    re->i.mem_used = mem_used;
    re->i.mem_peak = mem_peak;
  }
}

void RE_engine_report(RenderEngine *engine, int type, const char *msg)
{
  Render *re = engine->re;

  if (re) {
    BKE_report(engine->re->reports, type, msg);
  }
  else if (engine->reports) {
    BKE_report(engine->reports, type, msg);
  }
}

void RE_engine_set_error_message(RenderEngine *engine, const char *msg)
{
  Render *re = engine->re;
  if (re != NULL) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr) {
      if (rr->error != NULL) {
        MEM_freeN(rr->error);
      }
      rr->error = BLI_strdup(msg);
    }
    RE_ReleaseResult(re);
  }
}

const char *RE_engine_active_view_get(RenderEngine *engine)
{
  Render *re = engine->re;
  return RE_GetActiveRenderView(re);
}

void RE_engine_active_view_set(RenderEngine *engine, const char *viewname)
{
  Render *re = engine->re;
  RE_SetActiveRenderView(re, viewname);
}

float RE_engine_get_camera_shift_x(RenderEngine *engine, Object *camera, bool use_spherical_stereo)
{
  Render *re = engine->re;

  /* When using spherical stereo, get camera shift without multiview,
   * leaving stereo to be handled by the engine. */
  if (use_spherical_stereo) {
    re = NULL;
  }

  return BKE_camera_multiview_shift_x(re ? &re->r : NULL, camera, re->viewname);
}

void RE_engine_get_camera_model_matrix(RenderEngine *engine,
                                       Object *camera,
                                       bool use_spherical_stereo,
                                       float *r_modelmat)
{
  Render *re = engine->re;

  /* When using spherical stereo, get model matrix without multiview,
   * leaving stereo to be handled by the engine. */
  if (use_spherical_stereo) {
    re = NULL;
  }

  BKE_camera_multiview_model_matrix(
      re ? &re->r : NULL, camera, re->viewname, (float(*)[4])r_modelmat);
}

bool RE_engine_get_spherical_stereo(RenderEngine *engine, Object *camera)
{
  Render *re = engine->re;
  return BKE_camera_multiview_spherical_stereo(re ? &re->r : NULL, camera) ? 1 : 0;
}

rcti *RE_engine_get_current_tiles(Render *re, int *r_total_tiles, bool *r_needs_free)
{
  static rcti tiles_static[BLENDER_MAX_THREADS];
  const int allocation_step = BLENDER_MAX_THREADS;
  int total_tiles = 0;
  rcti *tiles = tiles_static;
  int allocation_size = BLENDER_MAX_THREADS;

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_READ);

  *r_needs_free = false;

  if (!re->parts || (re->engine && (re->engine->flag & RE_ENGINE_HIGHLIGHT_TILES) == 0)) {
    *r_total_tiles = 0;
    BLI_rw_mutex_unlock(&re->partsmutex);
    return NULL;
  }

  GHashIterator pa_iter;
  GHASH_ITER (pa_iter, re->parts) {
    RenderPart *pa = BLI_ghashIterator_getValue(&pa_iter);
    if (pa->status == PART_STATUS_IN_PROGRESS) {
      if (total_tiles >= allocation_size) {
        /* Just in case we're using crazy network rendering with more
         * workers than BLENDER_MAX_THREADS.
         */
        allocation_size += allocation_step;
        if (tiles == tiles_static) {
          /* Can not realloc yet, tiles are pointing to a
           * stack memory.
           */
          tiles = MEM_mallocN(allocation_size * sizeof(rcti), "current engine tiles");
        }
        else {
          tiles = MEM_reallocN(tiles, allocation_size * sizeof(rcti));
        }
        *r_needs_free = true;
      }
      tiles[total_tiles] = pa->disprect;

      total_tiles++;
    }
  }
  BLI_rw_mutex_unlock(&re->partsmutex);
  *r_total_tiles = total_tiles;
  return tiles;
}

RenderData *RE_engine_get_render_data(Render *re)
{
  return &re->r;
}

/* Depsgraph */
static void engine_depsgraph_init(RenderEngine *engine, ViewLayer *view_layer)
{
  Main *bmain = engine->re->main;
  Scene *scene = engine->re->scene;

  engine->depsgraph = DEG_graph_new(scene, view_layer, DAG_EVAL_RENDER);
  DEG_debug_name_set(engine->depsgraph, "RENDER");

  BKE_scene_graph_update_for_newframe(engine->depsgraph, bmain);
}

static void engine_depsgraph_free(RenderEngine *engine)
{
  DEG_graph_free(engine->depsgraph);

  engine->depsgraph = NULL;
}

void RE_engine_frame_set(RenderEngine *engine, int frame, float subframe)
{
  if (!engine->depsgraph) {
    return;
  }

#ifdef WITH_PYTHON
  BPy_BEGIN_ALLOW_THREADS;
#endif

  Render *re = engine->re;
  double cfra = (double)frame + (double)subframe;

  CLAMP(cfra, MINAFRAME, MAXFRAME);
  BKE_scene_frame_set(re->scene, cfra);
  BKE_scene_graph_update_for_newframe(engine->depsgraph, re->main);

  BKE_scene_camera_switch_update(re->scene);

#ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#endif
}

/* Bake */
void RE_bake_engine_set_engine_parameters(Render *re, Main *bmain, Scene *scene)
{
  re->scene = scene;
  re->main = bmain;
  render_copy_renderdata(&re->r, &scene->r);
}

bool RE_bake_has_engine(Render *re)
{
  RenderEngineType *type = RE_engines_find(re->r.engine);
  return (type->bake != NULL);
}

bool RE_bake_engine(Render *re,
                    Depsgraph *depsgraph,
                    Object *object,
                    const int object_id,
                    const BakePixel pixel_array[],
                    const size_t num_pixels,
                    const int depth,
                    const eScenePassType pass_type,
                    const int pass_filter,
                    float result[])
{
  RenderEngineType *type = RE_engines_find(re->r.engine);
  RenderEngine *engine;
  bool persistent_data = (re->r.mode & R_PERSISTENT_DATA) != 0;

  /* set render info */
  re->i.cfra = re->scene->r.cfra;
  BLI_strncpy(re->i.scene_name, re->scene->id.name + 2, sizeof(re->i.scene_name) - 2);
  re->i.totface = re->i.totvert = re->i.totstrand = re->i.totlamp = re->i.tothalo = 0;

  /* render */
  engine = re->engine;

  if (!engine) {
    engine = RE_engine_create(type);
    re->engine = engine;
  }

  engine->flag |= RE_ENGINE_RENDERING;

  /* TODO: actually link to a parent which shouldn't happen */
  engine->re = re;

  engine->resolution_x = re->winx;
  engine->resolution_y = re->winy;

  RE_parts_init(re);
  engine->tile_x = re->r.tilex;
  engine->tile_y = re->r.tiley;

  if (type->bake) {
    engine->depsgraph = depsgraph;

    /* update is only called so we create the engine.session */
    if (type->update) {
      type->update(engine, re->main, engine->depsgraph);
    }

    type->bake(engine,
               engine->depsgraph,
               object,
               pass_type,
               pass_filter,
               object_id,
               pixel_array,
               num_pixels,
               depth,
               result);

    engine->depsgraph = NULL;
  }

  engine->tile_x = 0;
  engine->tile_y = 0;
  engine->flag &= ~RE_ENGINE_RENDERING;

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);

  /* re->engine becomes zero if user changed active render engine during render */
  if (!persistent_data || !re->engine) {
    RE_engine_free(engine);
    re->engine = NULL;
  }

  RE_parts_free(re);
  BLI_rw_mutex_unlock(&re->partsmutex);

  if (BKE_reports_contain(re->reports, RPT_ERROR)) {
    G.is_break = true;
  }

  return true;
}

/* Render */

int RE_engine_render(Render *re, int do_all)
{
  RenderEngineType *type = RE_engines_find(re->r.engine);
  RenderEngine *engine;
  bool persistent_data = (re->r.mode & R_PERSISTENT_DATA) != 0;

  /* verify if we can render */
  if (!type->render) {
    return 0;
  }
  if ((re->r.scemode & R_BUTS_PREVIEW) && !(type->flag & RE_USE_PREVIEW)) {
    return 0;
  }
  if (do_all && !(type->flag & RE_USE_POSTPROCESS)) {
    return 0;
  }
  if (!do_all && (type->flag & RE_USE_POSTPROCESS)) {
    return 0;
  }

  /* Lock drawing in UI during data phase. */
  if (re->draw_lock) {
    re->draw_lock(re->dlh, 1);
  }

  /* update animation here so any render layer animation is applied before
   * creating the render result */
  if ((re->r.scemode & (R_NO_FRAME_UPDATE | R_BUTS_PREVIEW)) == 0) {
    render_update_anim_renderdata(re, &re->scene->r, &re->scene->view_layers);
  }

  /* create render result */
  BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
  if (re->result == NULL || !(re->r.scemode & R_BUTS_PREVIEW)) {
    int savebuffers = RR_USE_MEM;

    if (re->result) {
      render_result_free(re->result);
    }

    if ((type->flag & RE_USE_SAVE_BUFFERS) && (re->r.scemode & R_EXR_TILE_FILE)) {
      savebuffers = RR_USE_EXR;
    }
    re->result = render_result_new(re, &re->disprect, 0, savebuffers, RR_ALL_LAYERS, RR_ALL_VIEWS);
  }
  BLI_rw_mutex_unlock(&re->resultmutex);

  if (re->result == NULL) {
    /* Clear UI drawing locks. */
    if (re->draw_lock) {
      re->draw_lock(re->dlh, 0);
    }
    /* Too small image is handled earlier, here it could only happen if
     * there was no sufficient memory to allocate all passes.
     */
    BKE_report(re->reports, RPT_ERROR, "Failed allocate render result, out of memory");
    G.is_break = true;
    return 1;
  }

  /* set render info */
  re->i.cfra = re->scene->r.cfra;
  BLI_strncpy(re->i.scene_name, re->scene->id.name + 2, sizeof(re->i.scene_name));
  re->i.totface = re->i.totvert = re->i.totstrand = re->i.totlamp = re->i.tothalo = 0;

  /* render */
  engine = re->engine;

  if (!engine) {
    engine = RE_engine_create(type);
    re->engine = engine;
  }

  engine->flag |= RE_ENGINE_RENDERING;

  /* TODO: actually link to a parent which shouldn't happen */
  engine->re = re;

  if (re->flag & R_ANIMATION) {
    engine->flag |= RE_ENGINE_ANIMATION;
  }
  if (re->r.scemode & R_BUTS_PREVIEW) {
    engine->flag |= RE_ENGINE_PREVIEW;
  }
  engine->camera_override = re->camera_override;

  engine->resolution_x = re->winx;
  engine->resolution_y = re->winy;

  RE_parts_init(re);
  engine->tile_x = re->partx;
  engine->tile_y = re->party;

  if (re->result->do_exr_tile) {
    render_result_exr_file_begin(re, engine);
  }

  /* Clear UI drawing locks. */
  if (re->draw_lock) {
    re->draw_lock(re->dlh, 0);
  }

  if (type->render) {
    FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer_iter) {
      if (re->draw_lock) {
        re->draw_lock(re->dlh, 1);
      }

      ViewLayer *view_layer = BLI_findstring(
          &re->scene->view_layers, view_layer_iter->name, offsetof(ViewLayer, name));
      engine_depsgraph_init(engine, view_layer);

      if (type->update) {
        type->update(engine, re->main, engine->depsgraph);
      }

      if (re->draw_lock) {
        re->draw_lock(re->dlh, 0);
      }

      type->render(engine, engine->depsgraph);

      /* Grease pencil render over previous render result.
       *
       * NOTE: External engine might have been requested to free its
       * dependency graph, which is only allowed if there is no grease
       * pencil (pipeline is taking care of that).
       */
      if (!RE_engine_test_break(engine) && engine->depsgraph != NULL) {
        DRW_render_gpencil(engine, engine->depsgraph);
      }

      engine_depsgraph_free(engine);

      if (RE_engine_test_break(engine)) {
        break;
      }
    }
    FOREACH_VIEW_LAYER_TO_RENDER_END;
  }

  engine->tile_x = 0;
  engine->tile_y = 0;
  engine->flag &= ~RE_ENGINE_RENDERING;

  render_result_free_list(&engine->fullresult, engine->fullresult.first);

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);

  if (re->result->do_exr_tile) {
    render_result_exr_file_end(re, engine);
  }

  /* re->engine becomes zero if user changed active render engine during render */
  if (!persistent_data || !re->engine) {
    RE_engine_free(engine);
    re->engine = NULL;
  }

  if (re->r.scemode & R_EXR_CACHE_FILE) {
    BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
    render_result_exr_file_cache_write(re);
    BLI_rw_mutex_unlock(&re->resultmutex);
  }

  RE_parts_free(re);
  BLI_rw_mutex_unlock(&re->partsmutex);

  if (BKE_reports_contain(re->reports, RPT_ERROR)) {
    G.is_break = true;
  }

#ifdef WITH_FREESTYLE
  if (re->r.mode & R_EDGE_FRS) {
    RE_RenderFreestyleExternal(re);
  }
#endif

  return 1;
}

void RE_engine_update_render_passes(struct RenderEngine *engine,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    update_render_passes_cb_t callback,
                                    void *callback_data)
{
  if (!(scene && view_layer && engine && callback && engine->type->update_render_passes)) {
    return;
  }

  BLI_mutex_lock(&engine->update_render_passes_mutex);

  engine->update_render_passes_cb = callback;
  engine->update_render_passes_data = callback_data;
  engine->type->update_render_passes(engine, scene, view_layer);
  engine->update_render_passes_cb = NULL;
  engine->update_render_passes_data = NULL;

  BLI_mutex_unlock(&engine->update_render_passes_mutex);
}

void RE_engine_register_pass(struct RenderEngine *engine,
                             struct Scene *scene,
                             struct ViewLayer *view_layer,
                             const char *name,
                             int channels,
                             const char *chanid,
                             int type)
{
  if (!(scene && view_layer && engine && engine->update_render_passes_cb)) {
    return;
  }

  engine->update_render_passes_cb(
      engine->update_render_passes_data, scene, view_layer, name, channels, chanid, type);
}

void RE_engine_free_blender_memory(RenderEngine *engine)
{
  /* Weak way to save memory, but not crash grease pencil.
   *
   * TODO(sergey): Find better solution for this.
   * TODO(sergey): Try to find solution which does not involve looping over
   * all the objects.
   */
  if (DRW_render_check_grease_pencil(engine->depsgraph)) {
    return;
  }
  DEG_graph_free(engine->depsgraph);
  engine->depsgraph = NULL;
}
