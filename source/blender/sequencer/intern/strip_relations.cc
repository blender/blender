/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_session_uid.h"

#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "MOV_read.hh"

#include "SEQ_iterator.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_preview_cache.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_thumbnail_cache.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "cache/final_image_cache.hh"
#include "cache/intra_frame_cache.hh"
#include "cache/source_image_cache.hh"
#include "effects/effects.hh"
#include "sequencer.hh"
#include "utils.hh"

namespace blender::seq {

bool relation_is_effect_of_strip(const Strip *effect, const Strip *input)
{
  return ELEM(input, effect->input1, effect->input2);
}

void cache_cleanup(Scene *scene, CacheCleanup mode)
{
  if (flag_is_set(mode, CacheCleanup::Thumbnails)) {
    thumbnail_cache_clear(scene);
  }
  if (flag_is_set(mode, CacheCleanup::SourceImage)) {
    source_image_cache_clear(scene);
  }
  if (flag_is_set(mode, CacheCleanup::FinalImage)) {
    final_image_cache_clear(scene);
  }
  if (flag_is_set(mode, CacheCleanup::IntraFrame)) {
    intra_frame_cache_invalidate(scene);
    preview_cache_invalidate(scene);
  }
}

void cache_settings_changed(Scene *scene)
{
  if (!(scene->ed->cache_flag & SEQ_CACHE_STORE_RAW)) {
    /* RAW caches has been disabled, clear them out. */
    source_image_cache_clear(scene);
  }
  if (!(scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT)) {
    /* Final caches has been disabled, clear them out. */
    final_image_cache_clear(scene);
  }
}

bool is_cache_full(const Scene *scene)
{
  size_t cache_limit = size_t(U.memcachelimit) * 1024 * 1024;
  return source_image_cache_calc_memory_size(scene) + final_image_cache_calc_memory_size(scene) >
         cache_limit;
}

bool evict_caches_if_full(Scene *scene)
{
  if (!is_cache_full(scene)) {
    /* Cache is not full, we don't have to evict anything. */
    return false;
  }

  /* Cache is full, so we want to remove some images. We always try to remove one final image,
   * and some amount of source images for each final image, so that ratio of cached images
   * stays the same. Depending on the frame composition complexity, there can be lots of
   * source images cached for a single final frame; if we only removed one source image
   * we'd eventually have the cache still filled only with source images. */
  bool evicted_final = false;
  bool evicted_source = false;
  do {
    const size_t count_final = final_image_cache_get_image_count(scene);
    const size_t count_source = source_image_cache_get_image_count(scene);
    evicted_final = false;
    evicted_source = false;
    const bool final_active = scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT;
    /* Evict one final item, and as much from source as needed to maintain ratio. */
    if (count_final != 0) {
      evicted_final = final_image_cache_evict(scene);
    }
    /* Only remove source images if there's more of them than final ones. */
    if (count_source != 0 && (!final_active || count_source > count_final)) {
      evicted_source = source_image_cache_evict(scene);
      /* Only try to enforce the ratio when the final cache is active. */
      if (evicted_source && final_active) {
        const size_t items = divide_ceil_ul(count_source, std::max<size_t>(count_final, 1));
        /* Start at "1" to make sure we only try to evict more frames if the ratio is above 1:1. */
        for (size_t i = 1; i < items; i++) {
          if (!source_image_cache_evict(scene)) {
            /* Can't evict any more frames, stop. */
            break;
          }
        }
      }
    }

  } while (is_cache_full(scene) && (evicted_final || evicted_source));

  /* Did we evict anything to free up the cache? */
  return !(evicted_final || evicted_source);
}

static void invalidate_final_cache_strip_range(Scene *scene, const Strip *strip)
{
  const int strip_left = time_left_handle_frame_get(scene, strip);
  const int strip_right = time_right_handle_frame_get(scene, strip);
  final_image_cache_invalidate_frame_range(scene, strip_left, strip_right);
}

static void invalidate_raw_cache_of_parent_meta(Scene *scene, Strip *strip)
{
  Strip *meta = lookup_meta_by_strip(editing_get(scene), strip);
  if (meta == nullptr) {
    return;
  }

  relations_invalidate_cache_raw(scene, meta);
}

void relations_invalidate_cache_raw(Scene *scene, Strip *strip)
{
  source_image_cache_invalidate_strip(scene, strip);
  relations_invalidate_cache(scene, strip);
}

void relations_invalidate_cache(Scene *scene, Strip *strip)
{
  if (strip->effectdata && strip->type == STRIP_TYPE_SPEED) {
    strip_effect_speed_rebuild_map(scene, strip);
  }

  media_presence_invalidate_strip(scene, strip);

  invalidate_final_cache_strip_range(scene, strip);
  intra_frame_cache_invalidate(scene, strip);
  preview_cache_invalidate(scene);
  invalidate_raw_cache_of_parent_meta(scene, strip);

  /* Needed to update VSE sound. */
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  prefetch_stop(scene);
}

void relations_invalidate_scene_strips(const Main *bmain, const Scene *scene_target)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      for (Strip *strip : lookup_strips_by_scene(editing_get(scene), scene_target)) {
        relations_invalidate_cache_raw(scene, strip);
      }
    }
  }
}

void relations_invalidate_compositor_modifiers(const Main *bmain, const bNodeTree *node_tree)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      for (Strip *strip : lookup_strips_by_compositor_node_group(editing_get(scene), node_tree)) {
        relations_invalidate_cache(scene, strip);
      }
    }
  }
}

static void invalidate_movieclip_strips(Scene *scene, MovieClip *clip_target, ListBase *seqbase)
{
  for (Strip *strip = static_cast<Strip *>(seqbase->first); strip != nullptr; strip = strip->next)
  {
    if (strip->clip == clip_target) {
      relations_invalidate_cache_raw(scene, strip);
    }

    if (strip->seqbase.first != nullptr) {
      invalidate_movieclip_strips(scene, clip_target, &strip->seqbase);
    }
  }
}

void relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target)
{
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene != nullptr;
       scene = static_cast<Scene *>(scene->id.next))
  {
    if (scene->ed != nullptr) {
      invalidate_movieclip_strips(scene, clip_target, &scene->ed->seqbase);
    }
  }
}

void relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render)
{
  if (scene->ed == nullptr) {
    return;
  }

  prefetch_stop(scene);

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (for_render && time_strip_intersects_frame(scene, strip, scene->r.cfra)) {
      continue;
    }

    if (strip->data) {
      if (strip->type == STRIP_TYPE_MOVIE) {
        relations_strip_free_anim(strip);
      }
      if (strip->type == STRIP_TYPE_SPEED) {
        strip_effect_speed_rebuild_map(scene, strip);
      }
    }
    if (strip->type == STRIP_TYPE_META) {
      relations_free_imbuf(scene, &strip->seqbase, for_render);
    }
    if (strip->type == STRIP_TYPE_SCENE) {
      /* FIXME: recurse downwards,
       * but do recurse protection somehow! */
    }
  }
}

static void sequencer_all_free_anim_ibufs(const Scene *scene,
                                          ListBase *seqbase,
                                          int timeline_frame,
                                          const int frame_range[2])
{
  Editing *ed = editing_get(scene);
  for (Strip *strip = static_cast<Strip *>(seqbase->first); strip != nullptr; strip = strip->next)
  {
    if (!time_strip_intersects_frame(scene, strip, timeline_frame) ||
        !((frame_range[0] <= timeline_frame) && (frame_range[1] > timeline_frame)))
    {
      relations_strip_free_anim(strip);
    }
    if (strip->type == STRIP_TYPE_META) {
      int meta_range[2];

      MetaStack *ms = meta_stack_active_get(ed);
      if (ms != nullptr && ms->parent_strip == strip) {
        meta_range[0] = -MAXFRAME;
        meta_range[1] = MAXFRAME;
      }
      else {
        /* Limit frame range to meta strip. */
        meta_range[0] = max_ii(frame_range[0], time_left_handle_frame_get(scene, strip));
        meta_range[1] = min_ii(frame_range[1], time_right_handle_frame_get(scene, strip));
      }

      sequencer_all_free_anim_ibufs(scene, &strip->seqbase, timeline_frame, meta_range);
    }
  }
}

void relations_free_all_anim_ibufs(Scene *scene, int timeline_frame)
{
  Editing *ed = editing_get(scene);
  if (ed == nullptr) {
    return;
  }

  const int frame_range[2] = {-MAXFRAME, MAXFRAME};
  sequencer_all_free_anim_ibufs(scene, &ed->seqbase, timeline_frame, frame_range);
}

static Strip *sequencer_check_scene_recursion(Scene *scene, ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->type == STRIP_TYPE_SCENE && strip->scene == scene) {
      return strip;
    }

    if (strip->type == STRIP_TYPE_SCENE && (strip->flag & SEQ_SCENE_STRIPS)) {
      if (strip->scene && strip->scene->ed &&
          sequencer_check_scene_recursion(scene, &strip->scene->ed->seqbase))
      {
        return strip;
      }
    }

    if (strip->type == STRIP_TYPE_META && sequencer_check_scene_recursion(scene, &strip->seqbase))
    {
      return strip;
    }
  }

  return nullptr;
}

bool relations_check_scene_recursion(Scene *scene, ReportList *reports)
{
  Editing *ed = editing_get(scene);
  if (ed == nullptr) {
    return false;
  }

  Strip *recursive_seq = sequencer_check_scene_recursion(scene, &ed->seqbase);

  if (recursive_seq != nullptr) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Recursion detected in video sequencer. Strip %s at frame %d will not be rendered",
                recursive_seq->name + 2,
                time_left_handle_frame_get(scene, recursive_seq));

    LISTBASE_FOREACH (Strip *, strip, &ed->seqbase) {
      if (strip->type != STRIP_TYPE_SCENE && sequencer_strip_generates_image(strip)) {
        /* There are other strips to render, so render them. */
        return false;
      }
    }
    /* No other strips to render - cancel operator. */
    return true;
  }

  return false;
}

bool relations_render_loop_check(Strip *strip_main, Strip *strip)
{
  if (strip_main == nullptr || strip == nullptr) {
    return false;
  }

  if (strip_main == strip) {
    return true;
  }

  if ((strip_main->input1 && relations_render_loop_check(strip_main->input1, strip)) ||
      (strip_main->input2 && relations_render_loop_check(strip_main->input2, strip)))
  {
    return true;
  }

  LISTBASE_FOREACH (StripModifierData *, smd, &strip_main->modifiers) {
    if (smd->mask_strip && relations_render_loop_check(smd->mask_strip, strip)) {
      return true;
    }
  }

  return false;
}

void relations_strip_free_anim(Strip *strip)
{
  while (strip->anims.last) {
    StripAnim *sanim = static_cast<StripAnim *>(strip->anims.last);

    if (sanim->anim) {
      MOV_close(sanim->anim);
      sanim->anim = nullptr;
    }

    BLI_freelinkN(&strip->anims, sanim);
  }
  BLI_listbase_clear(&strip->anims);
}

void relations_session_uid_generate(Strip *strip)
{
  strip->runtime.session_uid = BLI_session_uid_generate();
}

static bool get_uids_cb(Strip *strip, void *user_data)
{
  GSet *used_uids = (GSet *)user_data;
  const SessionUID *session_uid = &strip->runtime.session_uid;
  if (!BLI_session_uid_is_generated(session_uid)) {
    printf("Sequence %s does not have UID generated.\n", strip->name);
    return true;
  }

  if (BLI_gset_lookup(used_uids, session_uid) != nullptr) {
    printf("Sequence %s has duplicate UID generated.\n", strip->name);
    return true;
  }

  BLI_gset_insert(used_uids, (void *)session_uid);
  return true;
}

void relations_check_uids_unique_and_report(const Scene *scene)
{
  if (scene->ed == nullptr) {
    return;
  }

  GSet *used_uids = BLI_gset_new(
      BLI_session_uid_ghash_hash, BLI_session_uid_ghash_compare, "sequencer used uids");

  foreach_strip(&scene->ed->seqbase, get_uids_cb, used_uids);

  BLI_gset_free(used_uids, nullptr);
}

bool exists_in_seqbase(const Strip *strip, const ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip_test, seqbase) {
    if (strip_test->type == STRIP_TYPE_META && exists_in_seqbase(strip, &strip_test->seqbase)) {
      return true;
    }
    if (strip_test == strip) {
      return true;
    }
  }
  return false;
}

}  // namespace blender::seq
