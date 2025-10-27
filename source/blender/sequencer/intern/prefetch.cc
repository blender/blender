/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"

#include "SEQ_time.hh"
#include "image_cache.hh"
#include "prefetch.hh"
#include "render.hh"

struct PrefetchJob {
  PrefetchJob *next, *prev;

  Main *bmain;
  Main *bmain_eval;
  Scene *scene;
  Scene *scene_eval;
  Depsgraph *depsgraph;

  ThreadMutex prefetch_suspend_mutex;
  ThreadCondition prefetch_suspend_cond;

  ListBase threads;

  /* context */
  SeqRenderData context;
  SeqRenderData context_cpy;
  ListBase *seqbasep;
  ListBase *seqbasep_cpy;

  /* prefetch area */
  float cfra;
  int num_frames_prefetched;

  /* control */
  bool running;
  bool waiting;
  bool stop;
};

static bool seq_prefetch_is_playing(const Main *bmain)
{
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    if (screen->animtimer) {
      return true;
    }
  }
  return false;
}

static bool seq_prefetch_is_scrubbing(const Main *bmain)
{

  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    if (screen->scrubbing) {
      return true;
    }
  }
  return false;
}

static PrefetchJob *seq_prefetch_job_get(Scene *scene)
{
  if (scene && scene->ed) {
    return scene->ed->prefetch_job;
  }
  return nullptr;
}

bool seq_prefetch_job_is_running(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return false;
  }

  return pfjob->running;
}

static bool seq_prefetch_job_is_waiting(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return false;
  }

  return pfjob->waiting;
}

static Sequence *sequencer_prefetch_get_original_sequence(Sequence *seq, ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq_orig, seqbase) {
    if (STREQ(seq->name, seq_orig->name)) {
      return seq_orig;
    }

    if (seq_orig->type == SEQ_TYPE_META) {
      Sequence *match = sequencer_prefetch_get_original_sequence(seq, &seq_orig->seqbase);
      if (match != nullptr) {
        return match;
      }
    }
  }

  return nullptr;
}

Sequence *seq_prefetch_get_original_sequence(Sequence *seq, Scene *scene)
{
  Editing *ed = scene->ed;
  return sequencer_prefetch_get_original_sequence(seq, &ed->seqbase);
}

SeqRenderData *seq_prefetch_get_original_context(const SeqRenderData *context)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(context->scene);

  return &pfjob->context;
}

static bool seq_prefetch_is_cache_full(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!seq_cache_is_full()) {
    return false;
  }

  return seq_cache_recycle_item(pfjob->scene) == false;
}

static float seq_prefetch_cfra(PrefetchJob *pfjob)
{
  return pfjob->cfra + pfjob->num_frames_prefetched;
}
static AnimationEvalContext seq_prefetch_anim_eval_context(PrefetchJob *pfjob)
{
  return BKE_animsys_eval_context_construct(pfjob->depsgraph, seq_prefetch_cfra(pfjob));
}

void seq_prefetch_get_time_range(Scene *scene, int *r_start, int *r_end)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  *r_start = pfjob->cfra;
  *r_end = seq_prefetch_cfra(pfjob);
}

static void seq_prefetch_free_depsgraph(PrefetchJob *pfjob)
{
  if (pfjob->depsgraph != nullptr) {
    DEG_graph_free(pfjob->depsgraph);
  }
  pfjob->depsgraph = nullptr;
  pfjob->scene_eval = nullptr;
}

static void seq_prefetch_update_depsgraph(PrefetchJob *pfjob)
{
  DEG_evaluate_on_framechange(pfjob->depsgraph, seq_prefetch_cfra(pfjob));
}

static void seq_prefetch_init_depsgraph(PrefetchJob *pfjob)
{
  Main *bmain = pfjob->bmain_eval;
  Scene *scene = pfjob->scene;
  ViewLayer *view_layer = BKE_view_layer_default_render(scene);

  pfjob->depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
  DEG_debug_name_set(pfjob->depsgraph, "SEQUENCER PREFETCH");

  /* Make sure there is a correct evaluated scene pointer. */
  DEG_graph_build_for_render_pipeline(pfjob->depsgraph);

  /* Update immediately so we have proper evaluated scene. */
  seq_prefetch_update_depsgraph(pfjob);

  pfjob->scene_eval = DEG_get_evaluated_scene(pfjob->depsgraph);
  pfjob->scene_eval->ed->cache_flag = 0;
}

static void seq_prefetch_update_area(PrefetchJob *pfjob)
{
  int cfra = pfjob->scene->r.cfra;

  /* rebase */
  if (cfra > pfjob->cfra) {
    int delta = cfra - pfjob->cfra;
    pfjob->cfra = cfra;
    pfjob->num_frames_prefetched -= delta;

    if (pfjob->num_frames_prefetched <= 1) {
      pfjob->num_frames_prefetched = 1;
    }
  }

  /* reset */
  if (cfra < pfjob->cfra) {
    pfjob->cfra = cfra;
    pfjob->num_frames_prefetched = 1;
  }
}

void SEQ_prefetch_stop_all()
{
  /* TODO(Richard): Use wm_jobs for prefetch, or pass main. */
  for (Scene *scene = static_cast<Scene *>(G.main->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    SEQ_prefetch_stop(scene);
  }
}

void SEQ_prefetch_stop(Scene *scene)
{
  PrefetchJob *pfjob;
  pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return;
  }

  pfjob->stop = true;

  while (pfjob->running) {
    BLI_condition_notify_one(&pfjob->prefetch_suspend_cond);
  }
}

static void seq_prefetch_update_context(const SeqRenderData *context)
{
  PrefetchJob *pfjob;
  pfjob = seq_prefetch_job_get(context->scene);

  SEQ_render_new_render_data(pfjob->bmain_eval,
                             pfjob->depsgraph,
                             pfjob->scene_eval,
                             context->rectx,
                             context->recty,
                             context->preview_render_size,
                             false,
                             &pfjob->context_cpy);
  pfjob->context_cpy.is_prefetch_render = true;
  pfjob->context_cpy.task_id = SEQ_TASK_PREFETCH_RENDER;

  SEQ_render_new_render_data(pfjob->bmain,
                             pfjob->depsgraph,
                             pfjob->scene,
                             context->rectx,
                             context->recty,
                             context->preview_render_size,
                             false,
                             &pfjob->context);
  pfjob->context.is_prefetch_render = false;

  /* Same ID as prefetch context, because context will be swapped, but we still
   * want to assign this ID to cache entries created in this thread.
   * This is to allow "temp cache" work correctly for both threads.
   */
  pfjob->context.task_id = SEQ_TASK_PREFETCH_RENDER;
}

static void seq_prefetch_update_scene(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return;
  }

  pfjob->scene = scene;
  seq_prefetch_free_depsgraph(pfjob);
  seq_prefetch_init_depsgraph(pfjob);
}

static void seq_prefetch_update_active_seqbase(PrefetchJob *pfjob)
{
  MetaStack *ms_orig = SEQ_meta_stack_active_get(SEQ_editing_get(pfjob->scene));
  Editing *ed_eval = SEQ_editing_get(pfjob->scene_eval);

  if (ms_orig != nullptr) {
    Sequence *meta_eval = seq_prefetch_get_original_sequence(ms_orig->parseq, pfjob->scene_eval);
    SEQ_seqbase_active_set(ed_eval, &meta_eval->seqbase);
  }
  else {
    SEQ_seqbase_active_set(ed_eval, &ed_eval->seqbase);
  }
}

static void seq_prefetch_resume(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (pfjob && pfjob->waiting) {
    BLI_condition_notify_one(&pfjob->prefetch_suspend_cond);
  }
}

void seq_prefetch_free(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);
  if (!pfjob) {
    return;
  }

  SEQ_prefetch_stop(scene);

  BLI_threadpool_remove(&pfjob->threads, pfjob);
  BLI_threadpool_end(&pfjob->threads);
  BLI_mutex_end(&pfjob->prefetch_suspend_mutex);
  BLI_condition_end(&pfjob->prefetch_suspend_cond);
  seq_prefetch_free_depsgraph(pfjob);
  BKE_main_free(pfjob->bmain_eval);
  MEM_freeN(pfjob);
  scene->ed->prefetch_job = nullptr;
}

static blender::VectorSet<Sequence *> query_scene_strips(ListBase *seqbase)
{
  blender::VectorSet<Sequence *> strips;
  LISTBASE_FOREACH (Sequence *, strip, seqbase) {
    if (strip->type == SEQ_TYPE_SCENE && (strip->flag & SEQ_SCENE_STRIPS) == 0) {
      strips.add(strip);
    }
  }
  return strips;
}

static bool seq_prefetch_scene_strip_is_rendered(const Scene *scene,
                                                 ListBase *channels,
                                                 ListBase *seqbase,
                                                 blender::Span<Sequence *> scene_strips,
                                                 int timeline_frame)
{
  blender::VectorSet<Sequence *> rendered_strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, timeline_frame, 0);

  /* Iterate over rendered strips. */
  for (Sequence *strip : rendered_strips) {
    if (strip->type == SEQ_TYPE_META &&
        seq_prefetch_scene_strip_is_rendered(
            scene, &strip->channels, &strip->seqbase, scene_strips, timeline_frame))
    {
      return true;
    }

    if (strip->type == SEQ_TYPE_SCENE && (strip->flag & SEQ_SCENE_STRIPS) != 0 &&
        strip->scene != nullptr)
    {
      const Scene *target_scene = strip->scene;
      Editing *target_ed = SEQ_editing_get(target_scene);
      ListBase *target_seqbase = SEQ_active_seqbase_get(target_ed);
      blender::VectorSet<Sequence *> target_scene_strips = query_scene_strips(target_seqbase);
      int target_timeline_frame = SEQ_give_frame_index(scene, strip, timeline_frame) +
                                  target_scene->r.sfra;

      return seq_prefetch_scene_strip_is_rendered(target_scene,
                                                  SEQ_channels_displayed_get(target_ed),
                                                  target_seqbase,
                                                  target_scene_strips,
                                                  target_timeline_frame);
    }

    /* Check if strip is effect of scene strip or uses it as modifier.
     * This also checks if `strip == seq_scene`. */
    for (Sequence *seq_scene : scene_strips) {
      if (SEQ_relations_render_loop_check(strip, seq_scene)) {
        return true;
      }
    }
  }
  return false;
}

/* Prefetch must avoid rendering scene strips, because rendering in background locks UI and can
 * make it unresponsive for long time periods. */
static bool seq_prefetch_must_skip_frame(PrefetchJob *pfjob, ListBase *channels, ListBase *seqbase)
{
  blender::VectorSet<Sequence *> scene_strips = query_scene_strips(seqbase);
  if (seq_prefetch_scene_strip_is_rendered(
          pfjob->scene_eval, channels, seqbase, scene_strips, seq_prefetch_cfra(pfjob)))
  {
    return true;
  }
  return false;
}

static bool seq_prefetch_need_suspend(PrefetchJob *pfjob)
{
  return seq_prefetch_is_cache_full(pfjob->scene) || seq_prefetch_is_scrubbing(pfjob->bmain) ||
         (seq_prefetch_cfra(pfjob) >= pfjob->scene->r.efra);
}

static void seq_prefetch_do_suspend(PrefetchJob *pfjob)
{
  BLI_mutex_lock(&pfjob->prefetch_suspend_mutex);
  while (seq_prefetch_need_suspend(pfjob) &&
         (pfjob->scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) && !pfjob->stop)
  {
    pfjob->waiting = true;
    BLI_condition_wait(&pfjob->prefetch_suspend_cond, &pfjob->prefetch_suspend_mutex);
    seq_prefetch_update_area(pfjob);
  }
  pfjob->waiting = false;
  BLI_mutex_unlock(&pfjob->prefetch_suspend_mutex);
}

static void *seq_prefetch_frames(void *job)
{
  PrefetchJob *pfjob = (PrefetchJob *)job;

  while (seq_prefetch_cfra(pfjob) <= pfjob->scene->r.efra) {
    pfjob->scene_eval->ed->prefetch_job = nullptr;

    seq_prefetch_update_depsgraph(pfjob);
    AnimData *adt = BKE_animdata_from_id(&pfjob->context_cpy.scene->id);
    AnimationEvalContext anim_eval_context = seq_prefetch_anim_eval_context(pfjob);
    BKE_animsys_evaluate_animdata(
        &pfjob->context_cpy.scene->id, adt, &anim_eval_context, ADT_RECALC_ALL, false);

    /* This is quite hacky solution:
     * We need cross-reference original scene with copy for cache.
     * However depsgraph must not have this data, because it will try to kill this job.
     * Scene copy don't reference original scene. Perhaps, this could be done by depsgraph.
     * Set to nullptr before return!
     */
    pfjob->scene_eval->ed->prefetch_job = pfjob;

    ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(pfjob->scene_eval));
    ListBase *channels = SEQ_channels_displayed_get(SEQ_editing_get(pfjob->scene_eval));
    if (seq_prefetch_must_skip_frame(pfjob, channels, seqbase)) {
      pfjob->num_frames_prefetched++;
      /* Break instead of keep looping if the job should be terminated. */
      if (!(pfjob->scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) || pfjob->stop) {
        break;
      }
      continue;
    }

    ImBuf *ibuf = SEQ_render_give_ibuf(&pfjob->context_cpy, seq_prefetch_cfra(pfjob), 0);
    seq_cache_free_temp_cache(pfjob->scene, pfjob->context.task_id, seq_prefetch_cfra(pfjob));
    IMB_freeImBuf(ibuf);

    /* Suspend thread if there is nothing to be prefetched. */
    seq_prefetch_do_suspend(pfjob);

    /* Avoid "collision" with main thread, but make sure to fetch at least few frames */
    if (pfjob->num_frames_prefetched > 5 && (seq_prefetch_cfra(pfjob) - pfjob->scene->r.cfra) < 2)
    {
      break;
    }

    if (!(pfjob->scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) || pfjob->stop) {
      break;
    }

    seq_prefetch_update_area(pfjob);
    pfjob->num_frames_prefetched++;
  }

  seq_cache_free_temp_cache(pfjob->scene, pfjob->context.task_id, seq_prefetch_cfra(pfjob));
  pfjob->running = false;
  pfjob->scene_eval->ed->prefetch_job = nullptr;

  return nullptr;
}

static PrefetchJob *seq_prefetch_start_ex(const SeqRenderData *context, float cfra)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(context->scene);

  if (!pfjob) {
    if (context->scene->ed) {
      pfjob = (PrefetchJob *)MEM_callocN(sizeof(PrefetchJob), "PrefetchJob");
      context->scene->ed->prefetch_job = pfjob;

      BLI_threadpool_init(&pfjob->threads, seq_prefetch_frames, 1);
      BLI_mutex_init(&pfjob->prefetch_suspend_mutex);
      BLI_condition_init(&pfjob->prefetch_suspend_cond);

      pfjob->bmain_eval = BKE_main_new();
      pfjob->scene = context->scene;
      seq_prefetch_init_depsgraph(pfjob);
    }
  }
  pfjob->bmain = context->bmain;

  pfjob->cfra = cfra;
  pfjob->num_frames_prefetched = 1;

  pfjob->waiting = false;
  pfjob->stop = false;
  pfjob->running = true;

  seq_prefetch_update_scene(context->scene);
  seq_prefetch_update_context(context);
  seq_prefetch_update_active_seqbase(pfjob);

  BLI_threadpool_remove(&pfjob->threads, pfjob);
  BLI_threadpool_insert(&pfjob->threads, pfjob);

  return pfjob;
}

void seq_prefetch_start(const SeqRenderData *context, float timeline_frame)
{
  Scene *scene = context->scene;
  Editing *ed = scene->ed;
  bool has_strips = bool(ed->seqbasep->first);

  if (!context->is_prefetch_render && !context->is_proxy_render) {
    bool playing = seq_prefetch_is_playing(context->bmain);
    bool scrubbing = seq_prefetch_is_scrubbing(context->bmain);
    bool running = seq_prefetch_job_is_running(scene);
    seq_prefetch_resume(scene);
    /* conditions to start:
     * prefetch enabled, prefetch not running, not scrubbing, not playing,
     * cache storage enabled, has strips to render, not rendering, not doing modal transform -
     * important, see D7820. */
    if ((ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) && !running && !scrubbing && !playing &&
        ed->cache_flag & SEQ_CACHE_ALL_TYPES && has_strips && !G.is_rendering && !G.moving)
    {

      seq_prefetch_start_ex(context, timeline_frame);
    }
  }
}

bool SEQ_prefetch_need_redraw(const bContext *C, Scene *scene)
{
  Main *bmain = CTX_data_main(C);
  bool playing = seq_prefetch_is_playing(bmain);
  bool scrubbing = seq_prefetch_is_scrubbing(bmain);
  bool running = seq_prefetch_job_is_running(scene);
  bool suspended = seq_prefetch_job_is_waiting(scene);

  SpaceSeq *sseq = CTX_wm_space_seq(C);
  bool showing_cache = sseq->cache_overlay.flag & SEQ_CACHE_SHOW;

  /* force redraw, when prefetching and using cache view. */
  if (running && !playing && !suspended && showing_cache) {
    return true;
  }
  /* Sometimes scrubbing flag is set when not scrubbing. In that case I want to catch "event" of
   * stopping scrubbing */
  if (scrubbing) {
    return true;
  }
  return false;
}
