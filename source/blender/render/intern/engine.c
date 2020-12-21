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

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
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

#include "RE_bake.h"
#include "RE_engine.h"
#include "RE_pipeline.h"

#include "DRW_engine.h"

#include "initrender.h"
#include "pipeline.h"
#include "render_result.h"
#include "render_types.h"

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
      if (type->rna_ext.free) {
        type->rna_ext.free(type->rna_ext.data);
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

bool RE_engine_is_external(const Render *re)
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
  RenderEngine *engine = MEM_callocN(sizeof(RenderEngine), "RenderEngine");
  engine->type = type;

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

  BLI_mutex_end(&engine->update_render_passes_mutex);

  MEM_freeN(engine);
}

/* Bake Render Results */

static RenderResult *render_result_from_bake(RenderEngine *engine, int x, int y, int w, int h)
{
  /* Create render result with specified size. */
  RenderResult *rr = MEM_callocN(sizeof(RenderResult), __func__);

  rr->rectx = w;
  rr->recty = h;
  rr->tilerect.xmin = x;
  rr->tilerect.ymin = y;
  rr->tilerect.xmax = x + w;
  rr->tilerect.ymax = y + h;

  /* Add single baking render layer. */
  RenderLayer *rl = MEM_callocN(sizeof(RenderLayer), "bake render layer");
  rl->rectx = w;
  rl->recty = h;
  BLI_addtail(&rr->layers, rl);

  /* Add render passes. */
  RenderPass *result_pass = render_layer_add_pass(
      rr, rl, engine->bake.depth, RE_PASSNAME_COMBINED, "", "RGBA");
  RenderPass *primitive_pass = render_layer_add_pass(rr, rl, 4, "BakePrimitive", "", "RGBA");
  RenderPass *differential_pass = render_layer_add_pass(rr, rl, 4, "BakeDifferential", "", "RGBA");

  /* Fill render passes from bake pixel array, to be read by the render engine. */
  for (int ty = 0; ty < h; ty++) {
    size_t offset = ty * w * 4;
    float *primitive = primitive_pass->rect + offset;
    float *differential = differential_pass->rect + offset;

    size_t bake_offset = (y + ty) * engine->bake.width + x;
    const BakePixel *bake_pixel = engine->bake.pixels + bake_offset;

    for (int tx = 0; tx < w; tx++) {
      if (bake_pixel->object_id != engine->bake.object_id) {
        primitive[0] = int_as_float(-1);
        primitive[1] = int_as_float(-1);
      }
      else {
        primitive[0] = int_as_float(bake_pixel->object_id);
        primitive[1] = int_as_float(bake_pixel->primitive_id);
        primitive[2] = bake_pixel->uv[0];
        primitive[3] = bake_pixel->uv[1];

        differential[0] = bake_pixel->du_dx;
        differential[1] = bake_pixel->du_dy;
        differential[2] = bake_pixel->dv_dx;
        differential[3] = bake_pixel->dv_dy;
      }

      primitive += 4;
      differential += 4;
      bake_pixel++;
    }
  }

  /* Initialize tile render result from full image bake result. */
  for (int ty = 0; ty < h; ty++) {
    size_t offset = ty * w * engine->bake.depth;
    size_t bake_offset = ((y + ty) * engine->bake.width + x) * engine->bake.depth;
    size_t size = w * engine->bake.depth * sizeof(float);

    memcpy(result_pass->rect + offset, engine->bake.result + bake_offset, size);
  }

  return rr;
}

static void render_result_to_bake(RenderEngine *engine, RenderResult *rr)
{
  RenderPass *rpass = RE_pass_find_by_name(rr->layers.first, RE_PASSNAME_COMBINED, "");

  if (!rpass) {
    return;
  }

  /* Copy from tile render result to full image bake result. */
  int x = rr->tilerect.xmin;
  int y = rr->tilerect.ymin;
  int w = rr->tilerect.xmax - rr->tilerect.xmin;
  int h = rr->tilerect.ymax - rr->tilerect.ymin;

  for (int ty = 0; ty < h; ty++) {
    size_t offset = ty * w * engine->bake.depth;
    size_t bake_offset = ((y + ty) * engine->bake.width + x) * engine->bake.depth;
    size_t size = w * engine->bake.depth * sizeof(float);

    memcpy(engine->bake.result + bake_offset, rpass->rect + offset, size);
  }
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
  if (engine->bake.pixels) {
    RenderResult *result = render_result_from_bake(engine, x, y, w, h);
    BLI_addtail(&engine->fullresult, result);
    return result;
  }

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

  result = render_result_new(re, &disprect, RR_USE_MEM, layername, viewname);

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
  if (engine->bake.pixels) {
    /* No interactive baking updates for now. */
    return;
  }

  Render *re = engine->re;

  if (result) {
    render_result_merge(re->result, result);
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

  RE_create_render_pass(re->result, name, channels, chan_id, layername, NULL);
}

void RE_engine_end_result(
    RenderEngine *engine, RenderResult *result, bool cancel, bool highlight, bool merge_results)
{
  Render *re = engine->re;

  if (!result) {
    return;
  }

  if (engine->bake.pixels) {
    render_result_to_bake(engine, result);
    BLI_remlink(&engine->fullresult, result);
    render_result_free(result);
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
      /* If written result does not match any tile and we are using save
       * buffers, we are going to get OpenEXR save errors. */
      fprintf(stderr, "RenderEngine.end_result: dimensions do not match any OpenEXR tile.\n");
    }
  }

  if (!cancel || merge_results) {
    if (re->result->do_exr_tile) {
      if (!cancel && merge_results) {
        render_result_exr_file_merge(re->result, result, re->viewname);
        render_result_merge(re->result, result);
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
  /* When using spherical stereo, get camera shift without multiview,
   * leaving stereo to be handled by the engine. */
  Render *re = engine->re;
  if (use_spherical_stereo || re == NULL) {
    return BKE_camera_multiview_shift_x(NULL, camera, NULL);
  }

  return BKE_camera_multiview_shift_x(&re->r, camera, re->viewname);
}

void RE_engine_get_camera_model_matrix(RenderEngine *engine,
                                       Object *camera,
                                       bool use_spherical_stereo,
                                       float *r_modelmat)
{
  /* When using spherical stereo, get model matrix without multiview,
   * leaving stereo to be handled by the engine. */
  Render *re = engine->re;
  if (use_spherical_stereo || re == NULL) {
    BKE_camera_multiview_model_matrix(NULL, camera, NULL, (float(*)[4])r_modelmat);
  }
  else {
    BKE_camera_multiview_model_matrix(&re->r, camera, re->viewname, (float(*)[4])r_modelmat);
  }
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

  engine->depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
  DEG_debug_name_set(engine->depsgraph, "RENDER");

  if (engine->re->r.scemode & R_BUTS_PREVIEW) {
    Depsgraph *depsgraph = engine->depsgraph;
    DEG_graph_relations_update(depsgraph);
    DEG_evaluate_on_framechange(depsgraph, CFRA);
    DEG_ids_check_recalc(bmain, depsgraph, scene, view_layer, true);
    DEG_ids_clear_recalc(bmain, depsgraph);
  }
  else {
    BKE_scene_graph_update_for_newframe(engine->depsgraph);
  }

  engine->has_grease_pencil = DRW_render_check_grease_pencil(engine->depsgraph);
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

  Render *re = engine->re;
  double cfra = (double)frame + (double)subframe;

  CLAMP(cfra, MINAFRAME, MAXFRAME);
  BKE_scene_frame_set(re->scene, cfra);
  BKE_scene_graph_update_for_newframe(engine->depsgraph);

  BKE_scene_camera_switch_update(re->scene);
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
                    const BakeImages *bake_images,
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

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);
  RE_parts_init(re);
  engine->tile_x = re->r.tilex;
  engine->tile_y = re->r.tiley;
  BLI_rw_mutex_unlock(&re->partsmutex);

  if (type->bake) {
    engine->depsgraph = depsgraph;

    /* update is only called so we create the engine.session */
    if (type->update) {
      type->update(engine, re->main, engine->depsgraph);
    }

    for (int i = 0; i < bake_images->size; i++) {
      const BakeImage *image = bake_images->data + i;

      engine->bake.pixels = pixel_array + image->offset;
      engine->bake.result = result + image->offset * depth;
      engine->bake.width = image->width;
      engine->bake.height = image->height;
      engine->bake.depth = depth;
      engine->bake.object_id = object_id;

      type->bake(
          engine, engine->depsgraph, object, pass_type, pass_filter, image->width, image->height);

      memset(&engine->bake, 0, sizeof(engine->bake));
    }

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

static void engine_render_view_layer(Render *re,
                                     RenderEngine *engine,
                                     ViewLayer *view_layer_iter,
                                     const bool use_engine,
                                     const bool use_grease_pencil)
{
  /* Lock UI so scene can't be edited while we read from it in this render thread. */
  if (re->draw_lock) {
    re->draw_lock(re->dlh, 1);
  }

  /* Create depsgraph with scene evaluated at render resolution. */
  ViewLayer *view_layer = BLI_findstring(
      &re->scene->view_layers, view_layer_iter->name, offsetof(ViewLayer, name));
  engine_depsgraph_init(engine, view_layer);

  /* Sync data to engine, within draw lock so scene data can be accessed safely. */
  if (use_engine) {
    if (engine->type->update) {
      engine->type->update(engine, re->main, engine->depsgraph);
    }
  }

  if (re->draw_lock) {
    re->draw_lock(re->dlh, 0);
  }

  /* Perform render with engine. */
  if (use_engine) {
    if (engine->type->flag & RE_USE_GPU_CONTEXT) {
      DRW_render_context_enable(engine->re);
    }

    engine->type->render(engine, engine->depsgraph);

    if (engine->type->flag & RE_USE_GPU_CONTEXT) {
      DRW_render_context_disable(engine->re);
    }
  }

  /* Optionally composite grease pencil over render result. */
  if (engine->has_grease_pencil && use_grease_pencil && !re->result->do_exr_tile) {
    /* NOTE: External engine might have been requested to free its
     * dependency graph, which is only allowed if there is no grease
     * pencil (pipeline is taking care of that). */
    if (!RE_engine_test_break(engine) && engine->depsgraph != NULL) {
      DRW_render_gpencil(engine, engine->depsgraph);
    }
  }

  /* Free dependency graph, if engine has not done it already. */
  engine_depsgraph_free(engine);
}

int RE_engine_render(Render *re, int do_all)
{
  RenderEngineType *type = RE_engines_find(re->r.engine);
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
    re->result = render_result_new(re, &re->disprect, savebuffers, RR_ALL_LAYERS, RR_ALL_VIEWS);
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

  /* render */
  RenderEngine *engine = re->engine;

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

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);
  RE_parts_init(re);
  engine->tile_x = re->partx;
  engine->tile_y = re->party;
  BLI_rw_mutex_unlock(&re->partsmutex);

  if (re->result->do_exr_tile) {
    render_result_exr_file_begin(re, engine);
  }

  /* Clear UI drawing locks. */
  if (re->draw_lock) {
    re->draw_lock(re->dlh, 0);
  }

  /* Render view layers. */
  bool delay_grease_pencil = false;

  if (type->render) {
    FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer_iter) {
      engine_render_view_layer(re, engine, view_layer_iter, true, true);

      /* With save buffers there is no render buffer in memory for compositing, delay
       * grease pencil in that case. */
      delay_grease_pencil = engine->has_grease_pencil && re->result->do_exr_tile;

      if (RE_engine_test_break(engine)) {
        break;
      }
    }
    FOREACH_VIEW_LAYER_TO_RENDER_END;
  }

  /* Clear tile data */
  engine->tile_x = 0;
  engine->tile_y = 0;
  engine->flag &= ~RE_ENGINE_RENDERING;

  render_result_free_list(&engine->fullresult, engine->fullresult.first);

  BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);

  /* For save buffers, read back from disk. */
  if (re->result->do_exr_tile) {
    render_result_exr_file_end(re, engine);
  }

  /* Perform delayed grease pencil rendering. */
  if (delay_grease_pencil) {
    BLI_rw_mutex_unlock(&re->partsmutex);

    FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer_iter) {
      engine_render_view_layer(re, engine, view_layer_iter, false, true);
      if (RE_engine_test_break(engine)) {
        break;
      }
    }
    FOREACH_VIEW_LAYER_TO_RENDER_END;

    BLI_rw_mutex_lock(&re->partsmutex, THREAD_LOCK_WRITE);
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
                             eNodeSocketDatatype type)
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
   */
  if (engine->has_grease_pencil) {
    return;
  }
  DEG_graph_free(engine->depsgraph);
  engine->depsgraph = NULL;
}
