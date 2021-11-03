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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
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

#include "effects.h"
#include "image_cache.h"
#include "utils.h"

/* check whether sequence cur depends on seq */
static bool seq_relations_check_depend(Sequence *seq, Sequence *cur)
{
  if (cur->seq1 == seq || cur->seq2 == seq || cur->seq3 == seq) {
    return true;
  }

  /* sequences are not intersecting in time, assume no dependency exists between them */
  if (cur->enddisp < seq->startdisp || cur->startdisp > seq->enddisp) {
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
       (cur->blend_mode == SEQ_TYPE_CROSS && cur->blend_opacity == 100.0f))) {
    return false;
  }

  return true;
}

static void sequence_do_invalidate_dependent(Scene *scene, Sequence *seq, ListBase *seqbase)
{
  Sequence *cur;

  for (cur = seqbase->first; cur; cur = cur->next) {
    if (cur == seq) {
      continue;
    }

    if (seq_relations_check_depend(seq, cur)) {
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

  if (meta_seq == NULL) {
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
    if (seq == invalidated_seq && meta_seq != NULL) {
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
  seq_relations_find_and_invalidate_metas(scene, seq, NULL);
}

void SEQ_relations_invalidate_cache_raw(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene, seq, true, SEQ_CACHE_ALL_TYPES);
  seq_relations_find_and_invalidate_metas(scene, seq, NULL);
}

void SEQ_relations_invalidate_cache_preprocessed(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene,
                            seq,
                            true,
                            SEQ_CACHE_STORE_PREPROCESSED | SEQ_CACHE_STORE_COMPOSITE |
                                SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, NULL);
}

void SEQ_relations_invalidate_cache_composite(Scene *scene, Sequence *seq)
{
  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    return;
  }

  sequence_invalidate_cache(
      scene, seq, true, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, NULL);
}

void SEQ_relations_invalidate_dependent(Scene *scene, Sequence *seq)
{
  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    return;
  }

  sequence_invalidate_cache(
      scene, seq, false, SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT);
  seq_relations_find_and_invalidate_metas(scene, seq, NULL);
}

static void invalidate_scene_strips(Scene *scene, Scene *scene_target, ListBase *seqbase)
{
  for (Sequence *seq = seqbase->first; seq != NULL; seq = seq->next) {
    if (seq->scene == scene_target) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }

    if (seq->seqbase.first != NULL) {
      invalidate_scene_strips(scene, scene_target, &seq->seqbase);
    }
  }
}

void SEQ_relations_invalidate_scene_strips(Main *bmain, Scene *scene_target)
{
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    if (scene->ed != NULL) {
      invalidate_scene_strips(scene, scene_target, &scene->ed->seqbase);
    }
  }
}

static void invalidate_movieclip_strips(Scene *scene, MovieClip *clip_target, ListBase *seqbase)
{
  for (Sequence *seq = seqbase->first; seq != NULL; seq = seq->next) {
    if (seq->clip == clip_target) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }

    if (seq->seqbase.first != NULL) {
      invalidate_movieclip_strips(scene, clip_target, &seq->seqbase);
    }
  }
}

void SEQ_relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target)
{
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    if (scene->ed != NULL) {
      invalidate_movieclip_strips(scene, clip_target, &scene->ed->seqbase);
    }
  }
}

void SEQ_relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render)
{
  if (scene->ed == NULL) {
    return;
  }

  Sequence *seq;

  SEQ_cache_cleanup(scene);
  SEQ_prefetch_stop(scene);

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (for_render && SEQ_time_strip_intersects_frame(seq, CFRA)) {
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

static bool update_changed_seq_recurs(
    Scene *scene, Sequence *seq, Sequence *changed_seq, int len_change, int ibuf_change)
{
  Sequence *subseq;
  bool free_imbuf = false;

  /* recurse downwards to see if this seq depends on the changed seq */

  if (seq == NULL) {
    return false;
  }

  if (seq == changed_seq) {
    free_imbuf = true;
  }

  for (subseq = seq->seqbase.first; subseq; subseq = subseq->next) {
    if (update_changed_seq_recurs(scene, subseq, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }

  if (seq->seq1) {
    if (update_changed_seq_recurs(scene, seq->seq1, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }
  if (seq->seq2 && (seq->seq2 != seq->seq1)) {
    if (update_changed_seq_recurs(scene, seq->seq2, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }
  if (seq->seq3 && (seq->seq3 != seq->seq1) && (seq->seq3 != seq->seq2)) {
    if (update_changed_seq_recurs(scene, seq->seq3, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }

  if (free_imbuf) {
    if (ibuf_change) {
      if (seq->type == SEQ_TYPE_MOVIE) {
        SEQ_relations_sequence_free_anim(seq);
      }
      else if (seq->type == SEQ_TYPE_SPEED) {
        seq_effect_speed_rebuild_map(scene, seq);
      }
    }

    if (len_change) {
      ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
      SEQ_time_update_sequence(scene, seqbase, seq);
    }
  }

  return free_imbuf;
}

void SEQ_relations_update_changed_seq_and_deps(Scene *scene,
                                               Sequence *changed_seq,
                                               int len_change,
                                               int ibuf_change)
{
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq;

  if (ed == NULL) {
    return;
  }

  for (seq = ed->seqbase.first; seq; seq = seq->next) {
    update_changed_seq_recurs(scene, seq, changed_seq, len_change, ibuf_change);
  }
}

static void sequencer_all_free_anim_ibufs(ListBase *seqbase, int timeline_frame)
{
  for (Sequence *seq = seqbase->first; seq != NULL; seq = seq->next) {
    if (!SEQ_time_strip_intersects_frame(seq, timeline_frame)) {
      SEQ_relations_sequence_free_anim(seq);
    }
    if (seq->type == SEQ_TYPE_META) {
      sequencer_all_free_anim_ibufs(&seq->seqbase, timeline_frame);
    }
  }
}

void SEQ_relations_free_all_anim_ibufs(Scene *scene, int timeline_frame)
{
  Editing *ed = SEQ_editing_get(scene);
  if (ed == NULL) {
    return;
  }
  sequencer_all_free_anim_ibufs(&ed->seqbase, timeline_frame);
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

  return NULL;
}

bool SEQ_relations_check_scene_recursion(Scene *scene, ReportList *reports)
{
  Editing *ed = SEQ_editing_get(scene);
  if (ed == NULL) {
    return false;
  }

  Sequence *recursive_seq = sequencer_check_scene_recursion(scene, &ed->seqbase);

  if (recursive_seq != NULL) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Recursion detected in video sequencer. Strip %s at frame %d will not be rendered",
                recursive_seq->name + 2,
                recursive_seq->startdisp);

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

/* Check if "seq_main" (indirectly) uses strip "seq". */
bool SEQ_relations_render_loop_check(Sequence *seq_main, Sequence *seq)
{
  if (seq_main == NULL || seq == NULL) {
    return false;
  }

  if (seq_main == seq) {
    return true;
  }

  if ((seq_main->seq1 && SEQ_relations_render_loop_check(seq_main->seq1, seq)) ||
      (seq_main->seq2 && SEQ_relations_render_loop_check(seq_main->seq2, seq)) ||
      (seq_main->seq3 && SEQ_relations_render_loop_check(seq_main->seq3, seq))) {
    return true;
  }

  SequenceModifierData *smd;
  for (smd = seq_main->modifiers.first; smd; smd = smd->next) {
    if (smd->mask_sequence && SEQ_relations_render_loop_check(smd->mask_sequence, seq)) {
      return true;
    }
  }

  return false;
}

/* Function to free imbuf and anim data on changes */
void SEQ_relations_sequence_free_anim(Sequence *seq)
{
  while (seq->anims.last) {
    StripAnim *sanim = seq->anims.last;

    if (sanim->anim) {
      IMB_free_anim(sanim->anim);
      sanim->anim = NULL;
    }

    BLI_freelinkN(&seq->anims, sanim);
  }
  BLI_listbase_clear(&seq->anims);
}

void SEQ_relations_session_uuid_generate(struct Sequence *sequence)
{
  sequence->runtime.session_uuid = BLI_session_uuid_generate();
}

static bool get_uuids_cb(Sequence *seq, void *user_data)
{
  struct GSet *used_uuids = (struct GSet *)user_data;
  const SessionUUID *session_uuid = &seq->runtime.session_uuid;
  if (!BLI_session_uuid_is_generated(session_uuid)) {
    printf("Sequence %s does not have UUID generated.\n", seq->name);
    return true;
  }

  if (BLI_gset_lookup(used_uuids, session_uuid) != NULL) {
    printf("Sequence %s has duplicate UUID generated.\n", seq->name);
    return true;
  }

  BLI_gset_insert(used_uuids, (void *)session_uuid);
  return true;
}

void SEQ_relations_check_uuids_unique_and_report(const Scene *scene)
{
  if (scene->ed == NULL) {
    return;
  }

  struct GSet *used_uuids = BLI_gset_new(
      BLI_session_uuid_ghash_hash, BLI_session_uuid_ghash_compare, "sequencer used uuids");

  SEQ_for_each_callback(&scene->ed->seqbase, get_uuids_cb, used_uuids);

  BLI_gset_free(used_uuids, NULL);
}

/* Return immediate parent meta of sequence */
struct Sequence *SEQ_find_metastrip_by_sequence(ListBase *seqbase, Sequence *meta, Sequence *seq)
{
  Sequence *iseq;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    Sequence *rval;

    if (seq == iseq) {
      return meta;
    }
    if (iseq->seqbase.first &&
        (rval = SEQ_find_metastrip_by_sequence(&iseq->seqbase, iseq, seq))) {
      return rval;
    }
  }

  return NULL;
}
