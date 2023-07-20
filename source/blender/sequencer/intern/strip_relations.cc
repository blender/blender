/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Foundation
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
#include "BLI_math.h"
#include "BLI_session_uuid.h"

#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_imbuf.h"

#include "SEQ_iterator.h"
#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

#include "effects.h"
#include "image_cache.h"
#include "utils.h"

bool SEQ_relation_is_effect_of_strip(const Sequence *effect, const Sequence *input)
{
  return ELEM(input, effect->seq1, effect->seq2);
}

/* check whether sequence cur depends on seq */
static bool seq_relations_check_depend(const Scene *scene, Sequence *seq, Sequence *cur)
{
  if (SEQ_relation_is_effect_of_strip(cur, seq)) {
    return true;
  }

  /* sequences are not intersecting in time, assume no dependency exists between them */
  if (SEQ_time_right_handle_frame_get(scene, cur) < SEQ_time_left_handle_frame_get(scene, seq) ||
      SEQ_time_left_handle_frame_get(scene, cur) > SEQ_time_right_handle_frame_get(scene, seq))
  {
    return false;
  }

  /* checking sequence is below reference one, not dependent on it */
  if (cur->machine < seq->machine) {
    return false;
  }

  /* sequence is not blending with lower machines, no dependency here occurs
   * check for non-effects only since effect could use lower machines as input
   */
  if ((cur->type & SEQ_TYPE_EFFECT) == 0 &&
      ((cur->blend_mode == SEQ_BLEND_REPLACE) ||
       (cur->blend_mode == SEQ_TYPE_CROSS && cur->blend_opacity == 100.0f)))
  {
    return false;
  }

  return true;
}

static void sequence_do_invalidate_dependent(Scene *scene, Sequence *seq, ListBase *seqbase)
{
  Sequence *cur;

  for (cur = static_cast<Sequence *>(seqbase->first); cur; cur = cur->next) {
    if (cur == seq) {
      continue;
    }

    if (seq_relations_check_depend(scene, seq, cur)) {
      /* Effect must be invalidated completely if they depend on invalidated seq. */
      if ((cur->type & SEQ_TYPE_EFFECT) != 0) {
        seq_cache_cleanup_sequence(scene, cur, seq, SEQ_CACHE_ALL_TYPES, false);
      }
      else {
        /* In case of alpha over for example only invalidate composite image */
        seq_cache_cleanup_sequence(
            scene, cur, seq, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT, false);
      }
    }

    if (cur->seqbase.first) {
      sequence_do_invalidate_dependent(scene, seq, &cur->seqbase);
    }
  }
}

static void sequence_invalidate_cache(Scene *scene,
                                      Sequence *seq,
                                      bool invalidate_self,
                                      int invalidate_types)
{
  Editing *ed = scene->ed;

  if (invalidate_self) {
    seq_cache_cleanup_sequence(scene, seq, seq, invalidate_types, false);
  }

  if (seq->effectdata && seq->type == SEQ_TYPE_SPEED) {
    seq_effect_speed_rebuild_map(scene, seq);
  }

  sequence_do_invalidate_dependent(scene, seq, &ed->seqbase);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  SEQ_prefetch_stop(scene);
}

/* Find metastrips that contain invalidated_seq and invalidate them. */
static bool seq_relations_find_and_invalidate_metas(Scene *scene,
                                                    Sequence *invalidated_seq,
                                                    Sequence *meta_seq)
{
  ListBase *seqbase;

  if (meta_seq == nullptr) {
    Editing *ed = SEQ_editing_get(scene);
    seqbase = &ed->seqbase;
  }
  else {
    seqbase = &meta_seq->seqbase;
  }

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_META) {
      if (seq_relations_find_and_invalidate_metas(scene, invalidated_seq, seq)) {
        sequence_invalidate_cache(scene, seq, true, SEQ_CACHE_ALL_TYPES);
        return true;
      }
    }
    if (seq == invalidated_seq && meta_seq != nullptr) {
      sequence_invalidate_cache(scene, meta_seq, true, SEQ_CACHE_ALL_TYPES);
      return true;
    }
  }
  return false;
}

void SEQ_relations_invalidate_cache_in_range(Scene *scene,
                                             Sequence *seq,
                                             Sequence *range_mask,
                                             int invalidate_types)
{
  seq_cache_cleanup_sequence(scene, seq, range_mask, invalidate_types, true);
  seq_relations_find_and_invalidate_metas(scene, seq, nullptr);
}

void SEQ_relations_invalidate_cache_raw(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene, seq, true, SEQ_CACHE_ALL_TYPES);
  seq_relations_find_and_invalidate_metas(scene, seq, nullptr);
}

void SEQ_relations_invalidate_cache_preprocessed(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene,
                            seq,
                            true,
                            SEQ_CACHE_STORE_PREPROCESSED | SEQ_CACHE_STORE_COMPOSITE |
                                SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, nullptr);
}

void SEQ_relations_invalidate_cache_composite(Scene *scene, Sequence *seq)
{
  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    return;
  }

  sequence_invalidate_cache(
      scene, seq, true, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, nullptr);
}

void SEQ_relations_invalidate_dependent(Scene *scene, Sequence *seq)
{
  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    return;
  }

  sequence_invalidate_cache(
      scene, seq, false, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, nullptr);
}

static void invalidate_scene_strips(Scene *scene, Scene *scene_target, ListBase *seqbase)
{
  for (Sequence *seq = static_cast<Sequence *>(seqbase->first); seq != nullptr; seq = seq->next) {
    if (seq->scene == scene_target) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }

    if (seq->seqbase.first != nullptr) {
      invalidate_scene_strips(scene, scene_target, &seq->seqbase);
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
  for (Sequence *seq = static_cast<Sequence *>(seqbase->first); seq != nullptr; seq = seq->next) {
    if (seq->clip == clip_target) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }

    if (seq->seqbase.first != nullptr) {
      invalidate_movieclip_strips(scene, clip_target, &seq->seqbase);
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

  Sequence *seq;

  SEQ_cache_cleanup(scene);
  SEQ_prefetch_stop(scene);

  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    if (for_render && SEQ_time_strip_intersects_frame(scene, seq, scene->r.cfra)) {
      continue;
    }

    if (seq->strip) {
      if (seq->type == SEQ_TYPE_MOVIE) {
        SEQ_relations_sequence_free_anim(seq);
      }
      if (seq->type == SEQ_TYPE_SPEED) {
        seq_effect_speed_rebuild_map(scene, seq);
      }
    }
    if (seq->type == SEQ_TYPE_META) {
      SEQ_relations_free_imbuf(scene, &seq->seqbase, for_render);
    }
    if (seq->type == SEQ_TYPE_SCENE) {
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
  for (Sequence *seq = static_cast<Sequence *>(seqbase->first); seq != nullptr; seq = seq->next) {
    if (!SEQ_time_strip_intersects_frame(scene, seq, timeline_frame) ||
        !((frame_range[0] <= timeline_frame) && (frame_range[1] > timeline_frame)))
    {
      SEQ_relations_sequence_free_anim(seq);
    }
    if (seq->type == SEQ_TYPE_META) {
      int meta_range[2];

      MetaStack *ms = SEQ_meta_stack_active_get(ed);
      if (ms != nullptr && ms->parseq == seq) {
        meta_range[0] = -MAXFRAME;
        meta_range[1] = MAXFRAME;
      }
      else {
        /* Limit frame range to meta strip. */
        meta_range[0] = max_ii(frame_range[0], SEQ_time_left_handle_frame_get(scene, seq));
        meta_range[1] = min_ii(frame_range[1], SEQ_time_right_handle_frame_get(scene, seq));
      }

      sequencer_all_free_anim_ibufs(scene, &seq->seqbase, timeline_frame, meta_range);
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

static Sequence *sequencer_check_scene_recursion(Scene *scene, ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_SCENE && seq->scene == scene) {
      return seq;
    }

    if (seq->type == SEQ_TYPE_SCENE && (seq->flag & SEQ_SCENE_STRIPS)) {
      if (sequencer_check_scene_recursion(scene, &seq->scene->ed->seqbase)) {
        return seq;
      }
    }

    if (seq->type == SEQ_TYPE_META && sequencer_check_scene_recursion(scene, &seq->seqbase)) {
      return seq;
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

  Sequence *recursive_seq = sequencer_check_scene_recursion(scene, &ed->seqbase);

  if (recursive_seq != nullptr) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Recursion detected in video sequencer. Strip %s at frame %d will not be rendered",
                recursive_seq->name + 2,
                SEQ_time_left_handle_frame_get(scene, recursive_seq));

    LISTBASE_FOREACH (Sequence *, seq, &ed->seqbase) {
      if (seq->type != SEQ_TYPE_SCENE && sequencer_seq_generates_image(seq)) {
        /* There are other strips to render, so render them. */
        return false;
      }
    }
    /* No other strips to render - cancel operator. */
    return true;
  }

  return false;
}

bool SEQ_relations_render_loop_check(Sequence *seq_main, Sequence *seq)
{
  if (seq_main == nullptr || seq == nullptr) {
    return false;
  }

  if (seq_main == seq) {
    return true;
  }

  if ((seq_main->seq1 && SEQ_relations_render_loop_check(seq_main->seq1, seq)) ||
      (seq_main->seq2 && SEQ_relations_render_loop_check(seq_main->seq2, seq)) ||
      (seq_main->seq3 && SEQ_relations_render_loop_check(seq_main->seq3, seq)))
  {
    return true;
  }

  SequenceModifierData *smd;
  for (smd = static_cast<SequenceModifierData *>(seq_main->modifiers.first); smd; smd = smd->next)
  {
    if (smd->mask_sequence && SEQ_relations_render_loop_check(smd->mask_sequence, seq)) {
      return true;
    }
  }

  return false;
}

void SEQ_relations_sequence_free_anim(Sequence *seq)
{
  while (seq->anims.last) {
    StripAnim *sanim = static_cast<StripAnim *>(seq->anims.last);

    if (sanim->anim) {
      IMB_free_anim(sanim->anim);
      sanim->anim = nullptr;
    }

    BLI_freelinkN(&seq->anims, sanim);
  }
  BLI_listbase_clear(&seq->anims);
}

void SEQ_relations_session_uuid_generate(Sequence *sequence)
{
  sequence->runtime.session_uuid = BLI_session_uuid_generate();
}

static bool get_uuids_cb(Sequence *seq, void *user_data)
{
  GSet *used_uuids = (GSet *)user_data;
  const SessionUUID *session_uuid = &seq->runtime.session_uuid;
  if (!BLI_session_uuid_is_generated(session_uuid)) {
    printf("Sequence %s does not have UUID generated.\n", seq->name);
    return true;
  }

  if (BLI_gset_lookup(used_uuids, session_uuid) != nullptr) {
    printf("Sequence %s has duplicate UUID generated.\n", seq->name);
    return true;
  }

  BLI_gset_insert(used_uuids, (void *)session_uuid);
  return true;
}

void SEQ_relations_check_uuids_unique_and_report(const Scene *scene)
{
  if (scene->ed == nullptr) {
    return;
  }

  GSet *used_uuids = BLI_gset_new(
      BLI_session_uuid_ghash_hash, BLI_session_uuid_ghash_compare, "sequencer used uuids");

  SEQ_for_each_callback(&scene->ed->seqbase, get_uuids_cb, used_uuids);

  BLI_gset_free(used_uuids, nullptr);
}

Sequence *SEQ_find_metastrip_by_sequence(ListBase *seqbase, Sequence *meta, Sequence *seq)
{
  Sequence *iseq;

  for (iseq = static_cast<Sequence *>(seqbase->first); iseq; iseq = iseq->next) {
    Sequence *rval;

    if (seq == iseq) {
      return meta;
    }
    if (iseq->seqbase.first && (rval = SEQ_find_metastrip_by_sequence(&iseq->seqbase, iseq, seq)))
    {
      return rval;
    }
  }

  return nullptr;
}

bool SEQ_exists_in_seqbase(const Sequence *seq, const ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->type == SEQ_TYPE_META && SEQ_exists_in_seqbase(seq, &seq_test->seqbase)) {
      return true;
    }
    if (seq_test == seq) {
      return true;
    }
  }
  return false;
}
