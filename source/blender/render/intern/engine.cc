/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_global.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_context.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "IMB_imbuf_types.hh"

#include "RE_bake.h"
#include "RE_engine.h"
#include "RE_pipeline.h"

#include "DRW_engine.hh"

#include "WM_api.hh"

#include "CLG_log.h"

#include "pipeline.hh"
#include "render_result.h"
#include "render_types.h"

/* Render Engine Types */

ListBase R_engines = {nullptr, nullptr};

static CLG_LogRef LOG = {"render"};

void RE_engines_init()
{
  DRW_engines_register();
  DRW_module_init();
}

void RE_engines_exit()
{
  RenderEngineType *type, *next;

  if (DRW_gpu_context_try_enable()) {
    /* Clean resources if the DRW context exists.
     * We need a context bound even when dealing with non context dependent GPU resources,
     * since GL functions may be null otherwise (See #141233). */
    DRW_engines_free();
    DRW_module_exit();
    DRW_gpu_context_disable();
  }

  for (type = static_cast<RenderEngineType *>(R_engines.first); type; type = next) {
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
  BLI_addtail(&R_engines, render_type);
}

RenderEngineType *RE_engines_find(const char *idname)
{
  RenderEngineType *type = static_cast<RenderEngineType *>(
      BLI_findstring(&R_engines, idname, offsetof(RenderEngineType, idname)));
  if (!type) {
    type = static_cast<RenderEngineType *>(
        BLI_findstring(&R_engines, "BLENDER_EEVEE", offsetof(RenderEngineType, idname)));
  }

  return type;
}

bool RE_engine_is_external(const Render *re)
{
  return (re->engine && re->engine->type && re->engine->type->render);
}

/* Create, Free */

RenderEngine *RE_engine_create(RenderEngineType *type)
{
  RenderEngine *engine = MEM_callocN<RenderEngine>("RenderEngine");
  engine->type = type;

  BLI_mutex_init(&engine->update_render_passes_mutex);
  BLI_mutex_init(&engine->blender_gpu_context_mutex);

  return engine;
}

static void engine_depsgraph_free(RenderEngine *engine)
{
  if (engine->depsgraph) {
    /* Need GPU context since this might free GPU buffers. */
    const bool use_gpu_context = (engine->type->flag & RE_USE_GPU_CONTEXT);
    if (use_gpu_context) {
      /* This function can be called on the main thread before RenderEngine is destroyed.
       * In this case, just bind the main draw context to gather the deleted GPU buffers.
       * Binding the same GPU context as the render engine is not needed (see #129019). */
      if (BLI_thread_is_main()) {
        DRW_gpu_context_enable();
      }
      else {
        DRW_render_context_enable(engine->re);
      }
    }

    DEG_graph_free(engine->depsgraph);
    engine->depsgraph = nullptr;

    if (use_gpu_context) {
      if (BLI_thread_is_main()) {
        DRW_gpu_context_disable();
      }
      else {
        DRW_render_context_disable(engine->re);
      }
    }
  }
}

void RE_engine_free(RenderEngine *engine)
{
#ifdef WITH_PYTHON
  if (engine->py_instance) {
    BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
  }
#endif

  engine_depsgraph_free(engine);

  BLI_mutex_end(&engine->blender_gpu_context_mutex);
  BLI_mutex_end(&engine->update_render_passes_mutex);

  MEM_freeN(engine);
}

/* Bake Render Results */

static RenderResult *render_result_from_bake(
    RenderEngine *engine, int x, int y, int w, int h, const char *layername)
{
  BakeImage *image = &engine->bake.targets->images[engine->bake.image_id];
  const BakePixel *pixels = engine->bake.pixels + image->offset;
  const size_t channels_num = engine->bake.targets->channels_num;

  /* Remember layer name for to match images in render_frame_finish. */
  if (image->render_layer_name[0] == '\0') {
    STRNCPY(image->render_layer_name, layername);
  }

  /* Create render result with specified size. */
  RenderResult *rr = MEM_callocN<RenderResult>(__func__);

  rr->rectx = w;
  rr->recty = h;
  rr->tilerect.xmin = x;
  rr->tilerect.ymin = y;
  rr->tilerect.xmax = x + w;
  rr->tilerect.ymax = y + h;

  BKE_scene_ppm_get(&engine->re->r, rr->ppm);

  /* Add single baking render layer. */
  RenderLayer *rl = MEM_callocN<RenderLayer>("bake render layer");
  STRNCPY(rl->name, layername);
  rl->rectx = w;
  rl->recty = h;
  BLI_addtail(&rr->layers, rl);

  /* Add render passes. */
  render_layer_add_pass(rr, rl, channels_num, RE_PASSNAME_COMBINED, "", "RGBA", true);

  RenderPass *primitive_pass = render_layer_add_pass(rr, rl, 3, "BakePrimitive", "", "RGB", true);
  RenderPass *differential_pass = render_layer_add_pass(
      rr, rl, 4, "BakeDifferential", "", "RGBA", true);

  /* Per-pixel seeds are only needed for baking to vertex colors, see
   * bake_targets_populate_pixels_color_attributes for more details. */
  RenderPass *seed_pass = (image->image == nullptr) ?
                              render_layer_add_pass(rr, rl, 1, "BakeSeed", "", "X", true) :
                              nullptr;

  /* Fill render passes from bake pixel array, to be read by the render engine. */
  for (int ty = 0; ty < h; ty++) {
    size_t offset = ty * w;
    float *primitive = primitive_pass->ibuf->float_buffer.data + 3 * offset;
    float *seed = (seed_pass != nullptr) ? (seed_pass->ibuf->float_buffer.data + offset) : nullptr;
    float *differential = differential_pass->ibuf->float_buffer.data + 4 * offset;

    size_t bake_offset = (y + ty) * image->width + x;
    const BakePixel *bake_pixel = pixels + bake_offset;

    for (int tx = 0; tx < w; tx++) {
      if (bake_pixel->object_id != engine->bake.object_id) {
        primitive[0] = int_as_float(-1);
        primitive[1] = int_as_float(-1);
        primitive[2] = int_as_float(-1);
      }
      else {
        primitive[0] = bake_pixel->uv[0];
        primitive[1] = bake_pixel->uv[1];
        primitive[2] = int_as_float(bake_pixel->primitive_id);

        differential[0] = bake_pixel->du_dx;
        differential[1] = bake_pixel->du_dy;
        differential[2] = bake_pixel->dv_dx;
        differential[3] = bake_pixel->dv_dy;
      }

      if (seed_pass != nullptr) {
        *seed = int_as_float(bake_pixel->seed);
        seed += 1;
      }

      primitive += 3;
      differential += 4;
      bake_pixel++;
    }
  }

  return rr;
}

static void render_result_to_bake(RenderEngine *engine, RenderResult *rr)
{
  RenderLayer *rl = static_cast<RenderLayer *>(rr->layers.first);
  RenderPass *rpass = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, "");
  if (!rpass) {
    return;
  }

  /* Find bake image corresponding to layer. */
  int image_id = 0;
  for (; image_id < engine->bake.targets->images_num; image_id++) {
    if (STREQ(engine->bake.targets->images[image_id].render_layer_name, rl->name)) {
      break;
    }
  }
  if (image_id == engine->bake.targets->images_num) {
    return;
  }

  const BakeImage *image = &engine->bake.targets->images[image_id];
  const BakePixel *pixels = engine->bake.pixels + image->offset;
  const size_t channels_num = engine->bake.targets->channels_num;
  const size_t channels_size = channels_num * sizeof(float);
  float *result = engine->bake.result + image->offset * channels_num;

  /* Copy from tile render result to full image bake result. Just the pixels for the
   * object currently being baked, to preserve other objects when baking multiple. */
  const int x = rr->tilerect.xmin;
  const int y = rr->tilerect.ymin;
  const int w = rr->tilerect.xmax - rr->tilerect.xmin;
  const int h = rr->tilerect.ymax - rr->tilerect.ymin;

  for (int ty = 0; ty < h; ty++) {
    const size_t offset = ty * w;
    const size_t bake_offset = (y + ty) * image->width + x;

    const float *pass_rect = rpass->ibuf->float_buffer.data + offset * channels_num;
    const BakePixel *bake_pixel = pixels + bake_offset;
    float *bake_result = result + bake_offset * channels_num;

    for (int tx = 0; tx < w; tx++) {
      if (bake_pixel->object_id == engine->bake.object_id) {
        memcpy(bake_result, pass_rect, channels_size);
      }
      pass_rect += channels_num;
      bake_result += channels_num;
      bake_pixel++;
    }
  }
}

/* Render Results */

RenderResult *RE_engine_begin_result(
    RenderEngine *engine, int x, int y, int w, int h, const char *layername, const char *viewname)
{
  if (engine->bake.targets) {
    RenderResult *result = render_result_from_bake(engine, x, y, w, h, layername);
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

  result = render_result_new(re, &disprect, layername, viewname);

  /* TODO: make this thread safe. */

  /* can be nullptr if we CLAMP the width or height to 0 */
  if (result) {
    render_result_clone_passes(re, result, viewname);
    render_result_passes_allocated_ensure(result);

    BLI_addtail(&engine->fullresult, result);

    result->tilerect.xmin += re->disprect.xmin;
    result->tilerect.xmax += re->disprect.xmin;
    result->tilerect.ymin += re->disprect.ymin;
    result->tilerect.ymax += re->disprect.ymin;
  }

  return result;
}

static void re_ensure_passes_allocated_thread_safe(Render *re)
{
  if (!re->result->passes_allocated) {
    BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
    if (!re->result->passes_allocated) {
      render_result_passes_allocated_ensure(re->result);
    }
    BLI_rw_mutex_unlock(&re->resultmutex);
  }
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
  if (engine->bake.targets) {
    /* No interactive baking updates for now. */
    return;
  }

  Render *re = engine->re;

  if (result) {
    re_ensure_passes_allocated_thread_safe(re);
    render_result_merge(re->result, result);
    result->renlay = static_cast<RenderLayer *>(
        result->layers.first); /* weak, draws first layer always */
    re->display_update(result, nullptr);
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

  RE_create_render_pass(re->result, name, channels, chan_id, layername, nullptr, false);
}

void RE_engine_end_result(
    RenderEngine *engine, RenderResult *result, bool cancel, bool highlight, bool merge_results)
{
  Render *re = engine->re;

  if (!result) {
    return;
  }

  if (engine->bake.targets) {
    if (!cancel || merge_results) {
      render_result_to_bake(engine, result);
    }
    BLI_remlink(&engine->fullresult, result);
    render_result_free(result);
    return;
  }

  if (re->engine && (re->engine->flag & RE_ENGINE_HIGHLIGHT_TILES)) {
    blender::render::TilesHighlight *tile_highlight = re->get_tile_highlight();

    if (tile_highlight) {
      if (highlight) {
        tile_highlight->highlight_tile_for_result(result);
      }
      else {
        tile_highlight->unhighlight_tile_for_result(result);
      }
    }
  }

  if (!cancel || merge_results) {
    if (!(re->test_break() && (re->r.scemode & R_BUTS_PREVIEW))) {
      re_ensure_passes_allocated_thread_safe(re);
      render_result_merge(re->result, result);
    }

    /* draw */
    if (!re->test_break()) {
      result->renlay = static_cast<RenderLayer *>(
          result->layers.first); /* weak, draws first layer always */
      re->display_update(result, nullptr);
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
    return re->test_break();
  }

  return false;
}

/* Statistics */

void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info)
{
  Render *re = engine->re;

  /* stats draw callback */
  if (re) {
    re->i.statstr = stats;
    re->i.infostr = info;
    re->stats_draw(&re->i);
    re->i.infostr = nullptr;
    re->i.statstr = nullptr;
  }

  /* set engine text */
  engine->text[0] = '\0';

  if (stats && stats[0] && info && info[0]) {
    SNPRINTF(engine->text, "%s | %s", stats, info);
  }
  else if (info && info[0]) {
    STRNCPY(engine->text, info);
  }
  else if (stats && stats[0]) {
    STRNCPY(engine->text, stats);
  }
}

void RE_engine_update_progress(RenderEngine *engine, float progress)
{
  Render *re = engine->re;

  if (re) {
    CLAMP(progress, 0.0f, 1.0f);
    re->progress(progress);
  }
}

void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak)
{
  Render *re = engine->re;

  if (re) {
    re->i.mem_used = int(ceilf(mem_used));
    re->i.mem_peak = int(ceilf(mem_peak));
  }
}

void RE_engine_report(RenderEngine *engine, int type, const char *msg)
{
  Render *re = engine->re;

  if (re) {
    BKE_report(engine->re->reports, (eReportType)type, msg);
  }
  else if (engine->reports) {
    BKE_report(engine->reports, (eReportType)type, msg);
  }
}

void RE_engine_set_error_message(RenderEngine *engine, const char *msg)
{
  Render *re = engine->re;
  if (re != nullptr) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr) {
      if (rr->error != nullptr) {
        MEM_freeN(rr->error);
      }
      rr->error = BLI_strdup(msg);
    }
    RE_ReleaseResult(re);
  }
}

RenderPass *RE_engine_pass_by_index_get(RenderEngine *engine, const char *layer_name, int index)
{
  Render *re = engine->re;
  if (re == nullptr) {
    return nullptr;
  }

  RenderPass *pass = nullptr;

  RenderResult *rr = RE_AcquireResultRead(re);
  if (rr != nullptr) {
    const RenderLayer *layer = RE_GetRenderLayer(rr, layer_name);
    if (layer != nullptr) {
      pass = static_cast<RenderPass *>(BLI_findlink(&layer->passes, index));
    }
  }
  RE_ReleaseResult(re);

  return pass;
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
  if (use_spherical_stereo || re == nullptr) {
    return BKE_camera_multiview_shift_x(nullptr, camera, nullptr);
  }

  return BKE_camera_multiview_shift_x(&re->r, camera, re->viewname);
}

void RE_engine_get_camera_model_matrix(RenderEngine *engine,
                                       Object *camera,
                                       bool use_spherical_stereo,
                                       float r_modelmat[16])
{
  /* When using spherical stereo, get model matrix without multiview,
   * leaving stereo to be handled by the engine. */
  Render *re = engine->re;
  if (use_spherical_stereo || re == nullptr) {
    BKE_camera_multiview_model_matrix(nullptr, camera, nullptr, (float (*)[4])r_modelmat);
  }
  else {
    BKE_camera_multiview_model_matrix(&re->r, camera, re->viewname, (float (*)[4])r_modelmat);
  }
}

bool RE_engine_get_spherical_stereo(RenderEngine *engine, Object *camera)
{
  Render *re = engine->re;
  return BKE_camera_multiview_spherical_stereo(re ? &re->r : nullptr, camera) ? true : false;
}

const rcti *RE_engine_get_current_tiles(Render *re, int *r_total_tiles)
{
  blender::render::TilesHighlight *tiles_highlight = re->get_tile_highlight();
  if (!tiles_highlight) {
    *r_total_tiles = 0;
    return nullptr;
  };

  blender::Span<rcti> highlighted_tiles = tiles_highlight->get_all_highlighted_tiles();

  *r_total_tiles = highlighted_tiles.size();
  return highlighted_tiles.data();
}

RenderData *RE_engine_get_render_data(Render *re)
{
  return &re->r;
}

bool RE_engine_use_persistent_data(RenderEngine *engine)
{
  /* Re-rendering is not supported with GPU contexts, since the GPU context
   * is destroyed when the render thread exists. */
  return (engine->re->r.mode & R_PERSISTENT_DATA) && !(engine->type->flag & RE_USE_GPU_CONTEXT);
}

static bool engine_keep_depsgraph(RenderEngine *engine)
{
  /* For persistent data or GPU engines like Eevee, reuse the depsgraph between
   * view layers and animation frames. For renderers like Cycles that create
   * their own copy of the scene, persistent data must be explicitly enabled to
   * keep memory usage low by default. */
  return (engine->re->r.mode & R_PERSISTENT_DATA) || (engine->type->flag & RE_USE_GPU_CONTEXT);
}

/* Depsgraph */
static void engine_depsgraph_init(RenderEngine *engine, ViewLayer *view_layer)
{
  Main *bmain = engine->re->main;
  Scene *scene = engine->re->scene;
  bool reuse_depsgraph = false;

  /* Reuse depsgraph from persistent data if possible. */
  if (engine->depsgraph) {
    if (DEG_get_bmain(engine->depsgraph) != bmain ||
        DEG_get_input_scene(engine->depsgraph) != scene)
    {
      /* If bmain or scene changes, we need a completely new graph. */
      engine_depsgraph_free(engine);
    }
    else if (DEG_get_input_view_layer(engine->depsgraph) != view_layer) {
      /* If only view layer changed, reuse depsgraph in the hope of reusing
       * objects shared between view layers. */
      DEG_graph_replace_owners(engine->depsgraph, bmain, scene, view_layer);
      DEG_graph_tag_relations_update(engine->depsgraph);
    }

    reuse_depsgraph = true;
  }

  if (!engine->depsgraph) {
    /* Ensure we only use persistent data for one scene / view layer at a time,
     * to avoid excessive memory usage. */
    RE_FreePersistentData(nullptr);

    /* Create new depsgraph if not cached with persistent data. */
    engine->depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
    DEG_debug_name_set(engine->depsgraph, "RENDER");
  }

  if (engine->re->r.scemode & R_BUTS_PREVIEW) {
    /* Update for preview render. */
    Depsgraph *depsgraph = engine->depsgraph;
    DEG_graph_relations_update(depsgraph);

    /* Need GPU context since this might free GPU buffers. */
    const bool use_gpu_context = (engine->type->flag & RE_USE_GPU_CONTEXT) && reuse_depsgraph;
    if (use_gpu_context) {
      DRW_render_context_enable(engine->re);
    }

    DEG_evaluate_on_framechange(depsgraph, BKE_scene_frame_get(scene));

    if (use_gpu_context) {
      DRW_render_context_disable(engine->re);
    }
  }
  else {
    /* Go through update with full Python callbacks for regular render. */
    BKE_scene_graph_update_for_newframe_ex(engine->depsgraph, false);
  }

  engine->has_grease_pencil = DRW_render_check_grease_pencil(engine->depsgraph);
}

static void engine_depsgraph_exit(RenderEngine *engine)
{
  if (engine->depsgraph) {
    if (engine_keep_depsgraph(engine)) {
      /* Clear recalc flags since the engine should have handled the updates for the currently
       * rendered framed by now. */
      DEG_ids_clear_recalc(engine->depsgraph, false);
    }
    else {
      /* Free immediately to save memory. */
      engine_depsgraph_free(engine);
    }
  }
}

void RE_engine_frame_set(RenderEngine *engine, int frame, float subframe)
{
  if (!engine->depsgraph) {
    return;
  }

  /* Clear recalc flags before update so engine can detect what changed. */
  DEG_ids_clear_recalc(engine->depsgraph, false);

  Render *re = engine->re;
  double cfra = double(frame) + double(subframe);

  CLAMP(cfra, MINAFRAME, MAXFRAME);
  BKE_scene_frame_set(re->scene, cfra);
  BKE_scene_graph_update_for_newframe_ex(engine->depsgraph, false);

  BKE_scene_camera_switch_update(re->scene);
}

/* Bake */

void RE_bake_engine_set_engine_parameters(Render *re, Main *bmain, Scene *scene)
{
  re->scene = scene;
  re->main = bmain;
  render_copy_renderdata(&re->r, &scene->r);
}

bool RE_bake_has_engine(const Render *re)
{
  const RenderEngineType *type = RE_engines_find(re->r.engine);
  return (type->bake != nullptr);
}

bool RE_bake_engine(Render *re,
                    Depsgraph *depsgraph,
                    Object *object,
                    const int object_id,
                    const BakePixel pixel_array[],
                    const BakeTargets *targets,
                    const eScenePassType pass_type,
                    const int pass_filter,
                    float result[])
{
  RenderEngineType *type = RE_engines_find(re->r.engine);
  RenderEngine *engine;

  /* set render info */
  re->i.cfra = re->scene->r.cfra;
  STRNCPY(re->i.scene_name, re->scene->id.name + 2);

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

  if (type->bake) {
    engine->depsgraph = depsgraph;

    /* update is only called so we create the engine.session */
    if (type->update) {
      type->update(engine, re->main, engine->depsgraph);
    }

    /* Bake all images. */
    engine->bake.targets = targets;
    engine->bake.pixels = pixel_array;
    engine->bake.result = result;
    engine->bake.object_id = object_id;

    for (int i = 0; i < targets->images_num; i++) {
      const BakeImage *image = &targets->images[i];
      engine->bake.image_id = i;

      type->bake(
          engine, engine->depsgraph, object, pass_type, pass_filter, image->width, image->height);
    }

    /* Optionally let render images read bake images from disk delayed. */
    if (type->render_frame_finish) {
      engine->bake.image_id = 0;
      type->render_frame_finish(engine);
    }

    memset(&engine->bake, 0, sizeof(engine->bake));

    engine->depsgraph = nullptr;
  }

  engine->flag &= ~RE_ENGINE_RENDERING;

  engine_depsgraph_free(engine);

  RE_engine_free(engine);
  re->engine = nullptr;

  if (BKE_reports_contain(re->reports, RPT_ERROR)) {
    G.is_break = true;
  }

  return true;
}

/* Render */

static bool possibly_using_gpu_compositor(const Render *re)
{
  if (re->r.compositor_device != SCE_COMPOSITOR_DEVICE_GPU) {
    return false;
  }

  const Scene *scene = re->pipeline_scene_eval;
  return (scene->compositing_node_group && (scene->r.scemode & R_DOCOMP));
}

static void engine_render_view_layer(Render *re,
                                     RenderEngine *engine,
                                     ViewLayer *view_layer_iter,
                                     const bool use_engine,
                                     const bool use_grease_pencil)
{
  /* Lock UI so scene can't be edited while we read from it in this render thread. */
  re->draw_lock();

  /* Create depsgraph with scene evaluated at render resolution. */
  ViewLayer *view_layer = static_cast<ViewLayer *>(
      BLI_findstring(&re->scene->view_layers, view_layer_iter->name, offsetof(ViewLayer, name)));
  if (!re->prepare_viewlayer(view_layer, engine->depsgraph)) {
    re->draw_unlock();
    return;
  }
  engine_depsgraph_init(engine, view_layer);

  /* Sync data to engine, within draw lock so scene data can be accessed safely. */
  if (use_engine) {
    const bool use_gpu_context = (engine->type->flag & RE_USE_GPU_CONTEXT);
    if (use_gpu_context) {
      DRW_render_context_enable(engine->re);
    }
    else if (G.background && ((engine->has_grease_pencil && use_grease_pencil) ||
                              possibly_using_gpu_compositor(re)))
    {
      /* Workaround for specific NVidia drivers which crash on Linux when OptiX context is
       * initialized prior to OpenGL context. This affects driver versions 545.29.06, 550.54.14,
       * and 550.67 running on kernel 6.8.
       *
       * The idea here is to initialize GPU context before giving control to the render engine in
       * cases when we know that the GPU context will definitely be needed later on.
       *
       * Only do it for background renders to avoid possible extra global locking during the
       * context initialization. For the non-background renders the GPU context is already
       * initialized for the Blender interface and no workaround is needed.
       *
       * Technically it is enough to only call WM_init_gpu() here, but it expects to only be called
       * once, and from here it is not possible to know whether GPU sub-system is initialized or
       * not. So instead temporarily enable the render context, which will take care of the GPU
       * context initialization.
       *
       * For demo file and tracking progress of possible fixes on driver side refer to #120007. */
      DRW_render_context_enable(engine->re);
      DRW_render_context_disable(engine->re);
    }

    if (engine->type->update) {
      engine->type->update(engine, re->main, engine->depsgraph);
    }
    if (use_gpu_context) {
      DRW_render_context_disable(engine->re);
    }
  }

  re->draw_unlock();

  /* Perform render with engine. */
  if (use_engine) {
    const bool use_gpu_context = (engine->type->flag & RE_USE_GPU_CONTEXT);
    if (use_gpu_context) {
      DRW_render_context_enable(engine->re);
    }

    BLI_mutex_lock(&engine->re->engine_draw_mutex);
    re->engine->flag |= RE_ENGINE_CAN_DRAW;
    BLI_mutex_unlock(&engine->re->engine_draw_mutex);

    engine->type->render(engine, engine->depsgraph);

    BLI_mutex_lock(&engine->re->engine_draw_mutex);
    re->engine->flag &= ~RE_ENGINE_CAN_DRAW;
    BLI_mutex_unlock(&engine->re->engine_draw_mutex);

    if (use_gpu_context) {
      DRW_render_context_disable(engine->re);
    }
  }

  /* Optionally composite grease pencil over render result.
   * Only do it if the passes are allocated (and the engine will not override the grease pencil
   * when reading its result from EXR file and writing to the Blender side. */
  if (engine->has_grease_pencil && use_grease_pencil && re->result->passes_allocated) {
    /* NOTE: External engine might have been requested to free its
     * dependency graph, which is only allowed if there is no grease
     * pencil (pipeline is taking care of that). */
    if (!RE_engine_test_break(engine) && engine->depsgraph != nullptr) {
      CLOG_INFO(&LOG, "Rendering grease pencil");
      DRW_render_gpencil(engine, engine->depsgraph);
    }
  }

  /* Free dependency graph, if engine has not done it already. */
  engine_depsgraph_exit(engine);
}

/* Callback function for engine_render_create_result to add all render passes to the result. */
static void engine_render_add_result_pass_cb(void *user_data,
                                             Scene * /*scene*/,
                                             ViewLayer *view_layer,
                                             const char *name,
                                             int channels,
                                             const char *chanid,
                                             eNodeSocketDatatype /*type*/)
{
  RenderResult *rr = (RenderResult *)user_data;
  RE_create_render_pass(rr, name, channels, chanid, view_layer->name, RR_ALL_VIEWS, false);
}

static RenderResult *engine_render_create_result(Render *re)
{
  RenderResult *rr = render_result_new(re, &re->disprect, RR_ALL_LAYERS, RR_ALL_VIEWS);
  if (rr == nullptr) {
    return nullptr;
  }

  FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer) {
    RE_engine_update_render_passes(
        re->engine, re->scene, view_layer, engine_render_add_result_pass_cb, rr);
  }
  FOREACH_VIEW_LAYER_TO_RENDER_END;

  /* Preview does not support deferred render result allocation. */
  if (re->r.scemode & R_BUTS_PREVIEW) {
    render_result_passes_allocated_ensure(rr);
  }

  return rr;
}

bool RE_engine_render(Render *re, bool do_all)
{
  RenderEngineType *type = RE_engines_find(re->r.engine);

  /* verify if we can render */
  if (!type->render) {
    return false;
  }
  if ((re->r.scemode & R_BUTS_PREVIEW) && !(type->flag & RE_USE_PREVIEW)) {
    return false;
  }
  if (do_all && !(type->flag & RE_USE_POSTPROCESS)) {
    return false;
  }
  if (!do_all && (type->flag & RE_USE_POSTPROCESS)) {
    return false;
  }

  /* Lock drawing in UI during data phase. */
  re->draw_lock();

  if ((type->flag & RE_USE_GPU_CONTEXT) && !GPU_backend_supported()) {
    /* Clear UI drawing locks. */
    re->draw_unlock();
    BKE_report(re->reports, RPT_ERROR, "Cannot initialize the GPU");
    G.is_break = true;
    return true;
  }

  /* Create engine. */
  RenderEngine *engine = re->engine;

  if (!engine) {
    engine = RE_engine_create(type);
    re->engine = engine;
  }

  /* Create render result. Do this before acquiring lock, to avoid lock
   * inversion as this calls python to get the render passes, while python UI
   * code can also hold a lock on the render result. */
  const bool create_new_result = (re->result == nullptr || !(re->r.scemode & R_BUTS_PREVIEW));
  RenderResult *new_result = (create_new_result) ? engine_render_create_result(re) : nullptr;

  BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
  if (create_new_result) {
    if (re->result) {
      render_result_free(re->result);
    }

    re->result = new_result;
  }
  BLI_rw_mutex_unlock(&re->resultmutex);

  if (re->result == nullptr) {
    /* Clear UI drawing locks. */
    re->draw_unlock();
    /* Free engine. */
    RE_engine_free(engine);
    re->engine = nullptr;
    /* Too small image is handled earlier, here it could only happen if
     * there was no sufficient memory to allocate all passes.
     */
    BKE_report(re->reports, RPT_ERROR, "Failed allocate render result, out of memory");
    G.is_break = true;
    return true;
  }

  /* set render info */
  re->i.cfra = re->scene->r.cfra;
  STRNCPY(re->i.scene_name, re->scene->id.name + 2);

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

  /* Clear UI drawing locks. */
  re->draw_unlock();

  /* Render view layers. */
  bool delay_grease_pencil = false;

  if (type->render) {
    FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer_iter) {
      CLOG_INFO(&LOG, "Start rendering: %s, %s", re->scene->id.name + 2, view_layer_iter->name);
      CLOG_INFO(&LOG, "Engine: %s", engine->type->name);
      const bool use_grease_pencil = (view_layer_iter->layflag & SCE_LAY_GREASE_PENCIL) != 0;
      engine_render_view_layer(re, engine, view_layer_iter, true, use_grease_pencil);

      /* If render passes are not allocated the render engine deferred final pixels write for
       * later. Need to defer the grease pencil for until after the engine has written the
       * render result to Blender. */
      delay_grease_pencil = use_grease_pencil && engine->has_grease_pencil &&
                            !re->result->passes_allocated;

      if (RE_engine_test_break(engine)) {
        break;
      }
    }
    FOREACH_VIEW_LAYER_TO_RENDER_END;
  }

  if (type->render_frame_finish) {
    type->render_frame_finish(engine);
  }

  /* Perform delayed grease pencil rendering. */
  if (delay_grease_pencil) {
    FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer_iter) {
      const bool use_grease_pencil = (view_layer_iter->layflag & SCE_LAY_GREASE_PENCIL) != 0;
      if (!use_grease_pencil) {
        continue;
      }
      engine_render_view_layer(re, engine, view_layer_iter, false, true);
      if (RE_engine_test_break(engine)) {
        break;
      }
    }
    FOREACH_VIEW_LAYER_TO_RENDER_END;
  }

  /* Clear tile data */
  engine->flag &= ~RE_ENGINE_RENDERING;

  render_result_free_list(&engine->fullresult,
                          static_cast<RenderResult *>(engine->fullresult.first));

  /* re->engine becomes zero if user changed active render engine during render */
  if (!engine_keep_depsgraph(engine) || !re->engine) {
    engine_depsgraph_free(engine);

    RE_engine_free(engine);
    re->engine = nullptr;
  }

  if (re->r.scemode & R_EXR_CACHE_FILE) {
    BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
    render_result_exr_file_cache_write(re);
    BLI_rw_mutex_unlock(&re->resultmutex);
  }

  if (BKE_reports_contain(re->reports, RPT_ERROR)) {
    G.is_break = true;
  }

#ifdef WITH_FREESTYLE
  if (re->r.mode & R_EDGE_FRS) {
    CLOG_INFO(&LOG, "Rendering freestyle");
    RE_RenderFreestyleExternal(re);
  }
#endif

  return true;
}

void RE_engine_update_render_passes(RenderEngine *engine,
                                    Scene *scene,
                                    ViewLayer *view_layer,
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
  engine->update_render_passes_cb = nullptr;
  engine->update_render_passes_data = nullptr;

  BLI_mutex_unlock(&engine->update_render_passes_mutex);
}

void RE_engine_register_pass(RenderEngine *engine,
                             Scene *scene,
                             ViewLayer *view_layer,
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
  if (engine->has_grease_pencil || engine_keep_depsgraph(engine)) {
    return;
  }
  engine_depsgraph_free(engine);
}

RenderEngine *RE_engine_get(const Render *re)
{
  return re->engine;
}

RenderEngine *RE_view_engine_get(const ViewRender *view_render)
{
  return view_render->engine;
}

bool RE_engine_draw_acquire(Render *re)
{
  RenderEngine *engine = re->engine;

  if (!engine) {
    /* No engine-side drawing if the engine does not exist. */
    return false;
  }

  if (!engine->type->draw) {
    /* Required callbacks are not implemented on the engine side. */
    return false;
  }

  /* Lock before checking the flag, to avoid possible conflicts with the render thread. */
  BLI_mutex_lock(&re->engine_draw_mutex);

  if ((engine->flag & RE_ENGINE_CAN_DRAW) == 0) {
    /* The rendering is not started yet, or has finished.
     *
     * In the former case there will nothing to be drawn, so can simply use RenderResult drawing
     * pipeline. In the latter case the engine has destroyed its display-only resources (textures,
     * graphics interops, etc..) so need to use the #RenderResult drawing pipeline. */
    BLI_mutex_unlock(&re->engine_draw_mutex);
    return false;
  }

  return true;
}

void RE_engine_draw_release(Render *re)
{
  BLI_mutex_unlock(&re->engine_draw_mutex);
}

void RE_engine_tile_highlight_set(
    RenderEngine *engine, int x, int y, int width, int height, bool highlight)
{
  if (!engine->re) {
    /* No render on the engine, so nowhere to store the highlighted tiles information. */
    return;
  }
  if ((engine->flag & RE_ENGINE_HIGHLIGHT_TILES) == 0) {
    /* Engine reported it does not support tiles highlight, but attempted to set the highlight.
     * Technically it is a logic error, but there is no good way to inform an external engine about
     * it. */
    return;
  }

  blender::render::TilesHighlight *tile_highlight = engine->re->get_tile_highlight();
  if (!tile_highlight) {
    /* The renderer itself does not support tiles highlight. */
    return;
  }

  if (highlight) {
    tile_highlight->highlight_tile(x, y, width, height);
  }
  else {
    tile_highlight->unhighlight_tile(x, y, width, height);
  }
}

void RE_engine_tile_highlight_clear_all(RenderEngine *engine)
{
  if (!engine->re) {
    /* No render on the engine, so nowhere to store the highlighted tiles information. */
    return;
  }
  if ((engine->flag & RE_ENGINE_HIGHLIGHT_TILES) == 0) {
    /* Engine reported it does not support tiles highlight, but attempted to set the highlight.
     * Technically it is a logic error, but there is no good way to inform an external engine about
     * it. */
    return;
  }

  blender::render::TilesHighlight *tile_highlight = engine->re->get_tile_highlight();
  if (!tile_highlight) {
    /* The renderer itself does not support tiles highlight. */
    return;
  }

  tile_highlight->clear();
}

/* -------------------------------------------------------------------- */
/** \name GPU context manipulation.
 *
 * GPU context for engine to create and update GPU resources in its own thread,
 * without blocking the main thread. Used by Cycles' display driver to create
 * display textures.
 *
 * \{ */

bool RE_engine_gpu_context_create(RenderEngine *engine)
{
  /* If there already is a draw manager render context available, reuse it. */
  engine->use_drw_render_context = (engine->re && RE_system_gpu_context_get(engine->re));
  if (engine->use_drw_render_context) {
    return true;
  }

  /* Viewport render case where no render context is available. We are expected to be on
   * the main thread here to safely create a context. */
  BLI_assert(BLI_thread_is_main());

  const bool drw_state = DRW_gpu_context_release();
  engine->system_gpu_context = WM_system_gpu_context_create();

  if (engine->system_gpu_context) {
    /* Activate new GPU Context for GPUContext creation. */
    WM_system_gpu_context_activate(engine->system_gpu_context);
    /* Requires GPUContext for usage of GPU Module for displaying results. */
    engine->blender_gpu_context = GPU_context_create(nullptr, engine->system_gpu_context);
    GPU_context_active_set(nullptr);
    /* Deactivate newly created GPU Context, as it is not needed until
     * `RE_engine_gpu_context_enable` is called. */
    WM_system_gpu_context_release(engine->system_gpu_context);
  }
  else {
    engine->blender_gpu_context = nullptr;
  }

  DRW_gpu_context_activate(drw_state);

  return engine->system_gpu_context != nullptr;
}

void RE_engine_gpu_context_destroy(RenderEngine *engine)
{
  if (!engine->system_gpu_context) {
    return;
  }

  const bool drw_state = DRW_gpu_context_release();

  WM_system_gpu_context_activate(engine->system_gpu_context);
  if (engine->blender_gpu_context) {
    GPUContext *restore_context = GPU_context_active_get();
    GPU_context_active_set(engine->blender_gpu_context);
    GPU_context_discard(engine->blender_gpu_context);
    if (restore_context != engine->blender_gpu_context) {
      GPU_context_active_set(restore_context);
    }
    engine->blender_gpu_context = nullptr;
  }
  WM_system_gpu_context_dispose(engine->system_gpu_context);
  engine->system_gpu_context = nullptr;

  DRW_gpu_context_activate(drw_state);
}

bool RE_engine_gpu_context_enable(RenderEngine *engine)
{
  engine->gpu_restore_context = false;
  if (engine->use_drw_render_context) {
    DRW_render_context_enable(engine->re);
    return true;
  }
  if (engine->system_gpu_context) {
    BLI_mutex_lock(&engine->blender_gpu_context_mutex);
    /* If a previous GPU/GPUContext was active (DST.blender_gpu_context), we should later
     * restore this when disabling the RenderEngine context. */
    engine->gpu_restore_context = DRW_gpu_context_release();

    /* Activate RenderEngine System and Blender GPU Context. */
    WM_system_gpu_context_activate(engine->system_gpu_context);
    if (engine->blender_gpu_context) {
      GPU_render_begin();
      GPU_context_active_set(engine->blender_gpu_context);
    }
    return true;
  }
  return false;
}

void RE_engine_gpu_context_disable(RenderEngine *engine)
{
  if (engine->use_drw_render_context) {
    DRW_render_context_disable(engine->re);
  }
  else {
    if (engine->system_gpu_context) {
      if (engine->blender_gpu_context) {
        GPU_context_active_set(nullptr);
        GPU_render_end();
      }
      WM_system_gpu_context_release(engine->system_gpu_context);
      /* Restore DRW state context if previously active. */
      DRW_gpu_context_activate(engine->gpu_restore_context);
      BLI_mutex_unlock(&engine->blender_gpu_context_mutex);
    }
  }
}

void RE_engine_gpu_context_lock(RenderEngine *engine)
{
  if (engine->use_drw_render_context) {
    /* Locking already handled by the draw manager. */
  }
  else {
    if (engine->system_gpu_context) {
      BLI_mutex_lock(&engine->blender_gpu_context_mutex);
    }
  }
}

void RE_engine_gpu_context_unlock(RenderEngine *engine)
{
  if (engine->use_drw_render_context) {
    /* Locking already handled by the draw manager. */
  }
  else {
    if (engine->system_gpu_context) {
      BLI_mutex_unlock(&engine->blender_gpu_context_mutex);
    }
  }
}

/** \} */
