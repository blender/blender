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
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "effects/effects.hh"
#include "image_cache.hh"
#include "utils.hh"

bool SEQ_relation_is_effect_of_strip(const Strip *effect, const Strip *input)
{
  return ELEM(input, effect->seq1, effect->seq2);
}

/* check whether sequence cur depends on seq */
static bool strip_relations_check_depend(const Scene *scene, Strip *strip, Strip *cur)
{
  if (SEQ_relation_is_effect_of_strip(cur, strip)) {
    return true;
  }

  /* sequences are not intersecting in time, assume no dependency exists between them */
  if (SEQ_time_right_handle_frame_get(scene, cur) < SEQ_time_left_handle_frame_get(scene, strip) ||
      SEQ_time_left_handle_frame_get(scene, cur) > SEQ_time_right_handle_frame_get(scene, strip))
  {
    return false;
  }

  /* checking sequence is below reference one, not dependent on it */
  if (cur->machine < strip->machine) {
    return false;
  }

  /* sequence is not blending with lower machines, no dependency here occurs
   * check for non-effects only since effect could use lower machines as input
   */
  if ((cur->type & STRIP_TYPE_EFFECT) == 0 &&
      ((cur->blend_mode == SEQ_BLEND_REPLACE) ||
       (cur->blend_mode == STRIP_TYPE_CROSS && cur->blend_opacity == 100.0f)))
  {
    return false;
  }

  return true;
}

static void sequence_do_invalidate_dependent(Scene *scene, Strip *strip, ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, cur, seqbase) {
    if (cur == strip) {
      continue;
    }

    if (strip_relations_check_depend(scene, strip, cur)) {
      /* Effect must be invalidated completely if they depend on invalidated strip. */
      if ((cur->type & STRIP_TYPE_EFFECT) != 0) {
        seq_cache_cleanup_sequence(scene, cur, strip, SEQ_CACHE_ALL_TYPES, false);
      }
      else {
        /* In case of alpha over for example only invalidate composite image */
        seq_cache_cleanup_sequence(
            scene, cur, strip, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT, false);
      }
    }

    if (cur->seqbase.first) {
      sequence_do_invalidate_dependent(scene, strip, &cur->seqbase);
    }
  }
}

static void sequence_invalidate_cache(Scene *scene,
                                      Strip *strip,
                                      bool invalidate_self,
                                      int invalidate_types)
{
  Editing *ed = scene->ed;

  if (invalidate_self) {
    seq_cache_cleanup_sequence(scene, strip, strip, invalidate_types, false);
  }

  if (strip->effectdata && strip->type == STRIP_TYPE_SPEED) {
    strip_effect_speed_rebuild_map(scene, strip);
  }

  blender::seq::media_presence_invalidate_strip(scene, strip);
  sequence_do_invalidate_dependent(scene, strip, &ed->seqbase);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  SEQ_prefetch_stop(scene);
}

/* Find meta-strips that contain invalidated_seq and invalidate them. */
static bool strip_relations_find_and_invalidate_metas(Scene *scene,
                                                      Strip *invalidated_seq,
                                                      Strip *meta_seq)
{
  ListBase *seqbase;

  if (meta_seq == nullptr) {
    Editing *ed = SEQ_editing_get(scene);
    seqbase = &ed->seqbase;
  }
  else {
    seqbase = &meta_seq->seqbase;
  }

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->type == STRIP_TYPE_META) {
      if (strip_relations_find_and_invalidate_metas(scene, invalidated_seq, strip)) {
        sequence_invalidate_cache(scene, strip, true, SEQ_CACHE_ALL_TYPES);
        return true;
      }
    }
    if (strip == invalidated_seq && meta_seq != nullptr) {
      sequence_invalidate_cache(scene, meta_seq, true, SEQ_CACHE_ALL_TYPES);
      return true;
    }
  }
  return false;
}

void SEQ_relations_invalidate_cache_in_range(Scene *scene,
                                             Strip *strip,
                                             Strip *range_mask,
                                             int invalidate_types)
{
  seq_cache_cleanup_sequence(scene, strip, range_mask, invalidate_types, true);
  strip_relations_find_and_invalidate_metas(scene, strip, nullptr);
}

void SEQ_relations_invalidate_cache_raw(Scene *scene, Strip *strip)
{
  sequence_invalidate_cache(scene, strip, true, SEQ_CACHE_ALL_TYPES);
  strip_relations_find_and_invalidate_metas(scene, strip, nullptr);
}

void SEQ_relations_invalidate_cache_preprocessed(Scene *scene, Strip *strip)
{
  sequence_invalidate_cache(scene,
                            strip,
                            true,
                            SEQ_CACHE_STORE_PREPROCESSED | SEQ_CACHE_STORE_COMPOSITE |
                                SEQ_CACHE_STORE_FINAL_OUT);
  strip_relations_find_and_invalidate_metas(scene, strip, nullptr);
}

void SEQ_relations_invalidate_cache_composite(Scene *scene, Strip *strip)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    return;
  }

  sequence_invalidate_cache(
      scene, strip, true, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  strip_relations_find_and_invalidate_metas(scene, strip, nullptr);
}

void SEQ_relations_invalidate_dependent(Scene *scene, Strip *strip)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    return;
  }

  sequence_invalidate_cache(
      scene, strip, false, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  strip_relations_find_and_invalidate_metas(scene, strip, nullptr);
}

static void invalidate_scene_strips(Scene *scene, Scene *scene_target, ListBase *seqbase)
{
  for (Strip *strip = static_cast<Strip *>(seqbase->first); strip != nullptr; strip = strip->next)
  {
    if (strip->scene == scene_target) {
      SEQ_relations_invalidate_cache_raw(scene, strip);
    }

    if (strip->seqbase.first != nullptr) {
      invalidate_scene_strips(scene, scene_target, &strip->seqbase);
    }
  }
}

void SEQ_relations_invalidate_scene_strips(Main *bmain, Scene *scene_target)
{
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene != nullptr;
       scene = static_cast<Scene *>(scene->id.next))
  {
    if (scene->ed != nullptr) {
      invalidate_scene_strips(scene, scene_target, &scene->ed->seqbase);
    }
  }
}

static void invalidate_movieclip_strips(Scene *scene, MovieClip *clip_target, ListBase *seqbase)
{
  for (Strip *strip = static_cast<Strip *>(seqbase->first); strip != nullptr; strip = strip->next)
  {
    if (strip->clip == clip_target) {
      SEQ_relations_invalidate_cache_raw(scene, strip);
    }

    if (strip->seqbase.first != nullptr) {
      invalidate_movieclip_strips(scene, clip_target, &strip->seqbase);
    }
  }
}

void SEQ_relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target)
{
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene != nullptr;
       scene = static_cast<Scene *>(scene->id.next))
  {
    if (scene->ed != nullptr) {
      invalidate_movieclip_strips(scene, clip_target, &scene->ed->seqbase);
    }
  }
}

void SEQ_relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render)
{
  if (scene->ed == nullptr) {
    return;
  }

  SEQ_cache_cleanup(scene);
  SEQ_prefetch_stop(scene);

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (for_render && SEQ_time_strip_intersects_frame(scene, strip, scene->r.cfra)) {
      continue;
    }

    if (strip->data) {
      if (strip->type == STRIP_TYPE_MOVIE) {
        SEQ_relations_sequence_free_anim(strip);
      }
      if (strip->type == STRIP_TYPE_SPEED) {
        strip_effect_speed_rebuild_map(scene, strip);
      }
    }
    if (strip->type == STRIP_TYPE_META) {
      SEQ_relations_free_imbuf(scene, &strip->seqbase, for_render);
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
  Editing *ed = SEQ_editing_get(scene);
  for (Strip *strip = static_cast<Strip *>(seqbase->first); strip != nullptr; strip = strip->next)
  {
    if (!SEQ_time_strip_intersects_frame(scene, strip, timeline_frame) ||
        !((frame_range[0] <= timeline_frame) && (frame_range[1] > timeline_frame)))
    {
      SEQ_relations_sequence_free_anim(strip);
    }
    if (strip->type == STRIP_TYPE_META) {
      int meta_range[2];

      MetaStack *ms = SEQ_meta_stack_active_get(ed);
      if (ms != nullptr && ms->parseq == strip) {
        meta_range[0] = -MAXFRAME;
        meta_range[1] = MAXFRAME;
      }
      else {
        /* Limit frame range to meta strip. */
        meta_range[0] = max_ii(frame_range[0], SEQ_time_left_handle_frame_get(scene, strip));
        meta_range[1] = min_ii(frame_range[1], SEQ_time_right_handle_frame_get(scene, strip));
      }

      sequencer_all_free_anim_ibufs(scene, &strip->seqbase, timeline_frame, meta_range);
    }
  }
}

void SEQ_relations_free_all_anim_ibufs(Scene *scene, int timeline_frame)
{
  Editing *ed = SEQ_editing_get(scene);
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

bool SEQ_relations_check_scene_recursion(Scene *scene, ReportList *reports)
{
  Editing *ed = SEQ_editing_get(scene);
  if (ed == nullptr) {
    return false;
  }

  Strip *recursive_seq = sequencer_check_scene_recursion(scene, &ed->seqbase);

  if (recursive_seq != nullptr) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Recursion detected in video sequencer. Strip %s at frame %d will not be rendered",
                recursive_seq->name + 2,
                SEQ_time_left_handle_frame_get(scene, recursive_seq));

    LISTBASE_FOREACH (Strip *, strip, &ed->seqbase) {
      if (strip->type != STRIP_TYPE_SCENE && sequencer_seq_generates_image(strip)) {
        /* There are other strips to render, so render them. */
        return false;
      }
    }
    /* No other strips to render - cancel operator. */
    return true;
  }

  return false;
}

bool SEQ_relations_render_loop_check(Strip *strip_main, Strip *strip)
{
  if (strip_main == nullptr || strip == nullptr) {
    return false;
  }

  if (strip_main == strip) {
    return true;
  }

  if ((strip_main->seq1 && SEQ_relations_render_loop_check(strip_main->seq1, strip)) ||
      (strip_main->seq2 && SEQ_relations_render_loop_check(strip_main->seq2, strip)))
  {
    return true;
  }

  LISTBASE_FOREACH (SequenceModifierData *, smd, &strip_main->modifiers) {
    if (smd->mask_sequence && SEQ_relations_render_loop_check(smd->mask_sequence, strip)) {
      return true;
    }
  }

  return false;
}

void SEQ_relations_sequence_free_anim(Strip *strip)
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

void SEQ_relations_session_uid_generate(Strip *sequence)
{
  sequence->runtime.session_uid = BLI_session_uid_generate();
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

void SEQ_relations_check_uids_unique_and_report(const Scene *scene)
{
  if (scene->ed == nullptr) {
    return;
  }

  GSet *used_uids = BLI_gset_new(
      BLI_session_uid_ghash_hash, BLI_session_uid_ghash_compare, "sequencer used uids");

  SEQ_for_each_callback(&scene->ed->seqbase, get_uids_cb, used_uids);

  BLI_gset_free(used_uids, nullptr);
}

Strip *SEQ_find_metastrip_by_sequence(ListBase *seqbase, Strip *meta, Strip *strip)
{
  LISTBASE_FOREACH (Strip *, iseq, seqbase) {
    Strip *rval;

    if (strip == iseq) {
      return meta;
    }
    if (iseq->seqbase.first &&
        (rval = SEQ_find_metastrip_by_sequence(&iseq->seqbase, iseq, strip)))
    {
      return rval;
    }
  }

  return nullptr;
}

bool SEQ_exists_in_seqbase(const Strip *strip, const ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip_test, seqbase) {
    if (strip_test->type == STRIP_TYPE_META && SEQ_exists_in_seqbase(strip, &strip_test->seqbase))
    {
      return true;
    }
    if (strip_test == strip) {
      return true;
    }
  }
  return false;
}
