/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

#include "IMB_imbuf.hh"

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
#include "SEQ_prefetch.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"

#include "SEQ_time.hh"
#include "prefetch.hh"
#include "render.hh"

namespace blender::seq {

struct PrefetchJob {
  PrefetchJob *next = nullptr;
  PrefetchJob *prev = nullptr;

  Main *bmain = nullptr;
  Main *bmain_eval = nullptr;
  Scene *scene = nullptr;
  Scene *scene_eval = nullptr;
  Depsgraph *depsgraph = nullptr;

  ThreadMutex prefetch_suspend_mutex = {};
  ThreadCondition prefetch_suspend_cond = {};

  ListBase threads = {};

  /* context */
  RenderData context = {};
  RenderData context_cpy = {};

  /* prefetch area */
  int cfra = 0;
  int timeline_start = 0;
  int timeline_end = 0;
  int timeline_length = 0;
  int num_frames_prefetched = 0;
  int cache_flags = 0; /* Only used to detect cache flag changes. */

  /* Control: */
  /* Set by prefetch. */
  bool running = false;
  bool waiting = false;
  bool stop = false;
  /* Set from outside. */
  bool is_scrubbing = false;
};

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

static void seq_prefetch_job_scrubbing_set(Scene *scene, bool is_scrubbing)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return;
  }

  pfjob->is_scrubbing = is_scrubbing;
}

static bool seq_prefetch_job_is_waiting(Scene *scene)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(scene);

  if (!pfjob) {
    return false;
  }

  return pfjob->waiting;
}

static Strip *original_strip_get(const Strip *strip, ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip_orig, seqbase) {
    if (STREQ(strip->name, strip_orig->name)) {
      return strip_orig;
    }

    if (strip_orig->type == STRIP_TYPE_META) {
      Strip *match = original_strip_get(strip, &strip_orig->seqbase);
      if (match != nullptr) {
        return match;
      }
    }
  }

  return nullptr;
}

static Strip *original_strip_get(const Strip *strip, Scene *scene)
{
  Editing *ed = scene->ed;
  return original_strip_get(strip, &ed->seqbase);
}

static RenderData *get_original_context(const RenderData *context)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(context->scene);
  return pfjob ? &pfjob->context : nullptr;
}

Scene *prefetch_get_original_scene(const RenderData *context)
{
  Scene *scene = context->scene;
  if (context->is_prefetch_render) {
    context = get_original_context(context);
    if (context != nullptr) {
      scene = context->scene;
    }
  }
  return scene;
}

Scene *prefetch_get_original_scene_and_strip(const RenderData *context, const Strip *&strip)
{
  Scene *scene = context->scene;
  if (context->is_prefetch_render) {
    context = get_original_context(context);
    if (context != nullptr) {
      scene = context->scene;
      strip = original_strip_get(strip, scene);
    }
  }
  return scene;
}

static bool seq_prefetch_is_cache_full(Scene *scene)
{
  return evict_caches_if_full(scene);
}

static int seq_prefetch_cfra(PrefetchJob *pfjob)
{
  int new_frame = pfjob->cfra + pfjob->num_frames_prefetched;
  Scene *scene = pfjob->scene; /* For the start/end frame macros. */
  int timeline_start = PSFRA;
  int timeline_end = PEFRA;
  if (new_frame >= timeline_end) {
    /* Wrap around to where we will jump when we reach the end frame. */
    new_frame = timeline_start + new_frame - timeline_end;
  }
  return new_frame;
}

static AnimationEvalContext seq_prefetch_anim_eval_context(PrefetchJob *pfjob)
{
  return BKE_animsys_eval_context_construct(pfjob->depsgraph, seq_prefetch_cfra(pfjob));
}

void seq_prefetch_get_time_range(Scene *scene, int *r_start, int *r_end)
{
  /* When there is no prefetch job, return "impossible" negative values. */
  *r_start = std::numeric_limits<int>::min();
  *r_end = std::numeric_limits<int>::min();

  PrefetchJob *pfjob = seq_prefetch_job_get(scene);
  if (pfjob == nullptr) {
    return;
  }
  if ((scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) == 0 || !pfjob->running) {
    return;
  }

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

    pfjob->num_frames_prefetched = std::max(pfjob->num_frames_prefetched, 1);
  }

  /* reset */
  if (cfra < pfjob->cfra) {
    pfjob->cfra = cfra;
    pfjob->num_frames_prefetched = 1;
  }

  /* timeline span changes */
  Scene *scene = pfjob->scene; /* For the start/end frame macros. */
  if (pfjob->timeline_start != PSFRA || pfjob->timeline_end != PEFRA) {
    pfjob->timeline_start = PSFRA;
    pfjob->timeline_end = PEFRA;
    pfjob->timeline_length = PEFRA - PSFRA;
    /* Reset the number of prefetched frames as we need to re-evaluate which
     * frames to keep in the cache.
     */
    pfjob->num_frames_prefetched = 1;
  }

  /* cache flag changes */
  if (pfjob->cache_flags != scene->ed->cache_flag) {
    pfjob->cache_flags = scene->ed->cache_flag;
    pfjob->num_frames_prefetched = 1;
  }
}

void prefetch_stop_all()
{
  /* TODO(Richard): Use wm_jobs for prefetch, or pass main. */
  for (Scene *scene = static_cast<Scene *>(G.main->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    prefetch_stop(scene);
  }
}

void prefetch_stop(Scene *scene)
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

static void seq_prefetch_update_context(const RenderData *context)
{
  PrefetchJob *pfjob;
  pfjob = seq_prefetch_job_get(context->scene);

  render_new_render_data(pfjob->bmain_eval,
                         pfjob->depsgraph,
                         pfjob->scene_eval,
                         context->rectx,
                         context->recty,
                         context->preview_render_size,
                         false,
                         &pfjob->context_cpy);
  pfjob->context_cpy.is_prefetch_render = true;
  pfjob->context_cpy.task_id = SEQ_TASK_PREFETCH_RENDER;

  render_new_render_data(pfjob->bmain,
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
  MetaStack *ms_orig = meta_stack_active_get(editing_get(pfjob->scene));
  Editing *ed_eval = editing_get(pfjob->scene_eval);

  if (ms_orig != nullptr) {
    Strip *meta_eval = original_strip_get(ms_orig->parent_strip, pfjob->scene_eval);
    ed_eval->current_meta_strip = meta_eval;
  }
  else {
    ed_eval->current_meta_strip = nullptr;
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

  prefetch_stop(scene);

  BLI_threadpool_remove(&pfjob->threads, pfjob);
  BLI_threadpool_end(&pfjob->threads);
  BLI_mutex_end(&pfjob->prefetch_suspend_mutex);
  BLI_condition_end(&pfjob->prefetch_suspend_cond);
  seq_prefetch_free_depsgraph(pfjob);
  BKE_main_free(pfjob->bmain_eval);
  scene->ed->prefetch_job = nullptr;
  MEM_delete(pfjob);
}

static VectorSet<Strip *> query_scene_strips(Editing *ed)
{
  Map<const Scene *, VectorSet<Strip *>> &strips_by_scene = lookup_strips_by_scene_map_get(ed);

  VectorSet<Strip *> scene_strips;
  for (VectorSet<Strip *> strips : strips_by_scene.values()) {
    scene_strips.add_multiple(strips);
  }
  return scene_strips;
}

static bool seq_prefetch_scene_strip_is_rendered(const Scene *scene,
                                                 ListBase *channels,
                                                 ListBase *seqbase,
                                                 Span<Strip *> scene_strips,
                                                 int timeline_frame,
                                                 SeqRenderState state)
{
  Vector<Strip *> rendered_strips = seq_shown_strips_get(
      scene, channels, seqbase, timeline_frame, 0);

  /* Iterate over rendered strips. */
  for (Strip *strip : rendered_strips) {
    if (strip->type == STRIP_TYPE_META &&
        seq_prefetch_scene_strip_is_rendered(
            scene, &strip->channels, &strip->seqbase, scene_strips, timeline_frame, state))
    {
      return true;
    }

    /* Recursive "sequencer-type" scene strip detected, no point in attempting to render it. */
    if (state.strips_rendering_seqbase.contains(strip)) {
      return true;
    }

    if (strip->type == STRIP_TYPE_SCENE && (strip->flag & SEQ_SCENE_STRIPS) != 0 &&
        strip->scene != nullptr && editing_get(strip->scene))
    {
      state.strips_rendering_seqbase.add(strip);

      const Scene *target_scene = strip->scene;
      Editing *target_ed = editing_get(target_scene);
      if (target_ed == nullptr) {
        continue;
      }

      VectorSet<Strip *> target_scene_strips = query_scene_strips(target_ed);
      int target_timeline_frame = give_frame_index(scene, strip, timeline_frame) +
                                  target_scene->r.sfra;

      return seq_prefetch_scene_strip_is_rendered(target_scene,
                                                  target_ed->current_channels(),
                                                  target_ed->current_strips(),
                                                  target_scene_strips,
                                                  target_timeline_frame,
                                                  state);
    }

    /* Check if strip is effect of scene strip or uses it as modifier.
     * This also checks if `strip == seq_scene`. */
    for (Strip *seq_scene : scene_strips) {
      if (relations_render_loop_check(strip, seq_scene)) {
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
  /* Pass in state to check for infinite recursion of "sequencer-type" scene strips. */
  SeqRenderState state = {};

  VectorSet<Strip *> scene_strips = query_scene_strips(editing_get(pfjob->scene_eval));
  return seq_prefetch_scene_strip_is_rendered(
      pfjob->scene_eval, channels, seqbase, scene_strips, seq_prefetch_cfra(pfjob), state);
}

static bool seq_prefetch_need_suspend(PrefetchJob *pfjob)
{
  return seq_prefetch_is_cache_full(pfjob->scene) || pfjob->is_scrubbing ||
         (pfjob->num_frames_prefetched >= pfjob->timeline_length);
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

  while (true) {
    if (pfjob->cfra < pfjob->timeline_start || pfjob->cfra > pfjob->timeline_end) {
      /* Don't try to prefetch anything when we are outside of the timeline range. */
      break;
    }
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

    ListBase *seqbase = active_seqbase_get(editing_get(pfjob->scene_eval));
    ListBase *channels = channels_displayed_get(editing_get(pfjob->scene_eval));
    if (seq_prefetch_must_skip_frame(pfjob, channels, seqbase)) {
      pfjob->num_frames_prefetched++;
      /* Break instead of keep looping if the job should be terminated. */
      if (!(pfjob->scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) ||
          !(pfjob->scene->ed->cache_flag & SEQ_CACHE_ALL_TYPES) || pfjob->stop)
      {
        break;
      }
      continue;
    }

    ImBuf *ibuf = render_give_ibuf(&pfjob->context_cpy, seq_prefetch_cfra(pfjob), 0);
    pfjob->num_frames_prefetched++;
    IMB_freeImBuf(ibuf);

    /* Suspend thread if there is nothing to be prefetched. */
    seq_prefetch_do_suspend(pfjob);

    if (!(pfjob->scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) ||
        !(pfjob->scene->ed->cache_flag & SEQ_CACHE_ALL_TYPES) || pfjob->stop)
    {
      break;
    }

    seq_prefetch_update_area(pfjob);
  }

  pfjob->running = false;
  pfjob->scene_eval->ed->prefetch_job = nullptr;

  return nullptr;
}

static PrefetchJob *seq_prefetch_start_ex(const RenderData *context, float cfra)
{
  PrefetchJob *pfjob = seq_prefetch_job_get(context->scene);

  if (!pfjob) {
    if (!context->scene->ed) {
      return nullptr;
    }
    pfjob = MEM_new<PrefetchJob>("PrefetchJob");
    context->scene->ed->prefetch_job = pfjob;

    BLI_threadpool_init(&pfjob->threads, seq_prefetch_frames, 1);
    BLI_mutex_init(&pfjob->prefetch_suspend_mutex);
    BLI_condition_init(&pfjob->prefetch_suspend_cond);

    pfjob->bmain_eval = BKE_main_new();
    pfjob->scene = context->scene;
    seq_prefetch_init_depsgraph(pfjob);
  }
  pfjob->bmain = context->bmain;

  Scene *scene = pfjob->scene; /* For the start/end frame macros. */
  pfjob->cfra = cfra;
  pfjob->timeline_start = PSFRA;
  pfjob->timeline_end = PEFRA;
  pfjob->timeline_length = PEFRA - PSFRA;
  pfjob->num_frames_prefetched = 1;
  pfjob->cache_flags = scene->ed->cache_flag;

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

void seq_prefetch_start(const RenderData *context, float timeline_frame)
{
  Scene *scene = context->scene;
  Editing *ed = scene->ed;
  bool has_strips = bool(ed->current_strips()->first);

  if (!context->is_prefetch_render && !context->is_proxy_render) {
    bool playing = context->is_playing;
    bool scrubbing = context->is_scrubbing;
    bool running = seq_prefetch_job_is_running(scene);
    seq_prefetch_job_scrubbing_set(scene, scrubbing);
    seq_prefetch_resume(scene);

    /* conditions to start:
     * prefetch enabled, prefetch not running, not scrubbing, not playing,
     * cache storage enabled, has strips to render, not rendering, not doing modal transform -
     * important, see D7820. */
    if ((ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE) && !running && !scrubbing && !playing &&
        (ed->cache_flag & SEQ_CACHE_ALL_TYPES) && has_strips && !G.is_rendering && !G.moving)
    {
      seq_prefetch_start_ex(context, timeline_frame);
    }
  }
}

bool prefetch_need_redraw(const bContext *C, Scene *scene)
{
  bScreen *screen = CTX_wm_screen(C);
  bool playing = screen->animtimer != nullptr;
  bool scrubbing = screen->scrubbing;
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

}  // namespace blender::seq
