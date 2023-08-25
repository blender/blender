/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"

#include "ED_screen.hh"
#include "ED_transform.hh"

#include "UI_view2d.hh"

#include "SEQ_channels.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

struct TransSeqSnapData {
  int *source_snap_points;
  int *target_snap_points;
  int source_snap_point_count;
  int target_snap_point_count;
};

/* -------------------------------------------------------------------- */
/** \name Snap sources
 * \{ */

static int seq_get_snap_source_points_len(SeqCollection *snap_sources)
{
  return SEQ_collection_len(snap_sources) * 2;
}

static void seq_snap_source_points_alloc(TransSeqSnapData *snap_data, SeqCollection *snap_sources)
{
  const size_t point_count = seq_get_snap_source_points_len(snap_sources);
  snap_data->source_snap_points = static_cast<int *>(
      MEM_callocN(sizeof(int) * point_count, __func__));
  memset(snap_data->source_snap_points, 0, sizeof(int));
  snap_data->source_snap_point_count = point_count;
}

static int cmp_fn(const void *a, const void *b)
{
  return (*(int *)a - *(int *)b);
}

static void seq_snap_source_points_build(const Scene *scene,
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_sources)
{
  int i = 0;
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, snap_sources) {
    int left = 0, right = 0;
    if (seq->flag & SEQ_LEFTSEL) {
      left = right = SEQ_time_left_handle_frame_get(scene, seq);
    }
    else if (seq->flag & SEQ_RIGHTSEL) {
      left = right = SEQ_time_right_handle_frame_get(scene, seq);
    }
    else {
      left = SEQ_time_left_handle_frame_get(scene, seq);
      right = SEQ_time_right_handle_frame_get(scene, seq);
    }

    snap_data->source_snap_points[i] = left;
    snap_data->source_snap_points[i + 1] = right;
    i += 2;
    BLI_assert(i <= snap_data->source_snap_point_count);
  }

  qsort(snap_data->source_snap_points, snap_data->source_snap_point_count, sizeof(int), cmp_fn);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap targets
 * \{ */

/* Add effect strips directly or indirectly connected to `seq_reference` to `collection`. */
static void query_strip_effects_fn(const Scene *scene,
                                   Sequence *seq_reference,
                                   ListBase *seqbase,
                                   SeqCollection *collection)
{
  if (!SEQ_collection_append_strip(seq_reference, collection)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  /* Find all strips connected to `seq_reference`. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (SEQ_relation_is_effect_of_strip(seq_test, seq_reference)) {
      query_strip_effects_fn(scene, seq_test, seqbase, collection);
    }
  }
}

static SeqCollection *seq_collection_extract_effects(SeqCollection *collection)
{
  SeqCollection *effects = SEQ_collection_create(__func__);
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if (SEQ_effect_get_num_inputs(seq->type) > 0) {
      SEQ_collection_append_strip(seq, effects);
    }
  }
  return effects;
}

static SeqCollection *query_snap_targets(Scene *scene,
                                         SeqCollection *snap_sources,
                                         bool exclude_selected)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  const short snap_flag = SEQ_tool_settings_snap_flag_get(scene);
  SeqCollection *snap_targets = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (exclude_selected && seq->flag & SELECT) {
      continue; /* Selected are being transformed. */
    }
    if (SEQ_render_is_muted(channels, seq) && (snap_flag & SEQ_SNAP_IGNORE_MUTED)) {
      continue;
    }
    if (seq->type == SEQ_TYPE_SOUND_RAM && (snap_flag & SEQ_SNAP_IGNORE_SOUND)) {
      continue;
    }
    SEQ_collection_append_strip(seq, snap_targets);
  }

  /* Effects will always change position with strip to which they are connected and they don't have
   * to be selected. Remove such strips from `snap_targets` collection. */
  SeqCollection *snap_sources_temp = SEQ_collection_duplicate(snap_sources);
  SEQ_collection_expand(scene, seqbase, snap_sources_temp, query_strip_effects_fn);
  SeqCollection *snap_sources_effects = seq_collection_extract_effects(snap_sources_temp);
  SEQ_collection_exclude(snap_targets, snap_sources_effects);
  SEQ_collection_free(snap_sources_temp);

  return snap_targets;
}

static int seq_get_snap_target_points_count(short snap_mode, SeqCollection *snap_targets)
{
  int count = 2; /* Strip start and end are always used. */

  if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
    count += 2;
  }

  count *= SEQ_collection_len(snap_targets);

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    count++;
  }

  return count;
}

static void seq_snap_target_points_alloc(short snap_mode,
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_targets)
{
  const size_t point_count = seq_get_snap_target_points_count(snap_mode, snap_targets);
  snap_data->target_snap_points = static_cast<int *>(
      MEM_callocN(sizeof(int) * point_count, __func__));
  memset(snap_data->target_snap_points, 0, sizeof(int));
  snap_data->target_snap_point_count = point_count;
}

static void seq_snap_target_points_build(Scene *scene,
                                         short snap_mode,
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_targets)
{
  int i = 0;

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->target_snap_points[i] = scene->r.cfra;
    i++;
  }

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, snap_targets) {
    snap_data->target_snap_points[i] = SEQ_time_left_handle_frame_get(scene, seq);
    snap_data->target_snap_points[i + 1] = SEQ_time_right_handle_frame_get(scene, seq);
    i += 2;

    if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
      int content_start = min_ii(SEQ_time_left_handle_frame_get(scene, seq),
                                 SEQ_time_start_frame_get(seq));
      int content_end = max_ii(SEQ_time_right_handle_frame_get(scene, seq),
                               SEQ_time_content_end_frame_get(scene, seq));
      /* Effects and single image strips produce incorrect content length. Skip these strips. */
      if ((seq->type & SEQ_TYPE_EFFECT) != 0 || seq->len == 1) {
        content_start = SEQ_time_left_handle_frame_get(scene, seq);
        content_end = SEQ_time_right_handle_frame_get(scene, seq);
      }

      CLAMP(content_start,
            SEQ_time_left_handle_frame_get(scene, seq),
            SEQ_time_right_handle_frame_get(scene, seq));
      CLAMP(content_end,
            SEQ_time_left_handle_frame_get(scene, seq),
            SEQ_time_right_handle_frame_get(scene, seq));

      snap_data->target_snap_points[i] = content_start;
      snap_data->target_snap_points[i + 1] = content_end;
      i += 2;
    }
  }
  BLI_assert(i <= snap_data->target_snap_point_count);
  qsort(snap_data->target_snap_points, snap_data->target_snap_point_count, sizeof(int), cmp_fn);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap utilities
 * \{ */

static int seq_snap_threshold_get_frame_distance(const TransInfo *t)
{
  const int snap_distance = SEQ_tool_settings_snap_distance_get(t->scene);
  const View2D *v2d = &t->region->v2d;
  return round_fl_to_int(UI_view2d_region_to_view_x(v2d, snap_distance) -
                         UI_view2d_region_to_view_x(v2d, 0));
}

/** \} */

TransSeqSnapData *transform_snap_sequencer_data_alloc(const TransInfo *t)
{
  if (t->data_type == &TransConvertType_SequencerImage) {
    return nullptr;
  }

  Scene *scene = t->scene;
  TransSeqSnapData *snap_data = static_cast<TransSeqSnapData *>(
      MEM_callocN(sizeof(TransSeqSnapData), __func__));
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));

  SeqCollection *snap_sources = SEQ_query_selected_strips(seqbase);
  SeqCollection *snap_targets = query_snap_targets(scene, snap_sources, true);

  if (SEQ_collection_len(snap_sources) == 0) {
    SEQ_collection_free(snap_targets);
    SEQ_collection_free(snap_sources);
    MEM_freeN(snap_data);
    return nullptr;
  }

  /* Build arrays of snap points. */
  seq_snap_source_points_alloc(snap_data, snap_sources);
  seq_snap_source_points_build(scene, snap_data, snap_sources);
  SEQ_collection_free(snap_sources);

  short snap_mode = t->tsnap.mode;
  seq_snap_target_points_alloc(snap_mode, snap_data, snap_targets);
  seq_snap_target_points_build(scene, snap_mode, snap_data, snap_targets);
  SEQ_collection_free(snap_targets);

  return snap_data;
}

void transform_snap_sequencer_data_free(TransSeqSnapData *data)
{
  MEM_freeN(data->source_snap_points);
  MEM_freeN(data->target_snap_points);
  MEM_freeN(data);
}

bool transform_snap_sequencer_calc(TransInfo *t)
{
  const TransSeqSnapData *snap_data = t->tsnap.seq_context;
  if (snap_data == nullptr) {
    return false;
  }

  /* Prevent snapping when constrained to Y axis. */
  if (t->con.mode & CON_APPLY && t->con.mode & CON_AXIS1) {
    return false;
  }

  int best_dist = MAXFRAME, best_target_frame = 0, best_source_frame = 0;

  for (int i = 0; i < snap_data->source_snap_point_count; i++) {
    int snap_source_frame = snap_data->source_snap_points[i] + round_fl_to_int(t->values[0]);
    for (int j = 0; j < snap_data->target_snap_point_count; j++) {
      int snap_target_frame = snap_data->target_snap_points[j];

      int dist = abs(snap_target_frame - snap_source_frame);
      if (dist > best_dist) {
        continue;
      }

      best_dist = dist;
      best_target_frame = snap_target_frame;
      best_source_frame = snap_source_frame;
    }
  }

  if (best_dist > seq_snap_threshold_get_frame_distance(t)) {
    return false;
  }

  t->tsnap.snap_target[0] = best_target_frame;
  t->tsnap.snap_source[0] = best_source_frame;
  return true;
}

void transform_snap_sequencer_apply_translate(TransInfo *t, float *vec)
{
  *vec += t->tsnap.snap_target[0] - t->tsnap.snap_source[0];
}

static int transform_snap_sequencer_to_closest_strip_ex(TransInfo *t, int frame_1, int frame_2)
{
  Scene *scene = t->scene;
  TransSeqSnapData *snap_data = static_cast<TransSeqSnapData *>(
      MEM_callocN(sizeof(TransSeqSnapData), __func__));

  SeqCollection *empty_col = SEQ_collection_create(__func__);
  SeqCollection *snap_targets = query_snap_targets(scene, empty_col, false);
  SEQ_collection_free(empty_col);

  snap_data->source_snap_points = static_cast<int *>(MEM_callocN(sizeof(int) * 2, __func__));
  snap_data->source_snap_point_count = 2;
  BLI_assert(frame_1 <= frame_2);
  snap_data->source_snap_points[0] = frame_1;
  snap_data->source_snap_points[1] = frame_2;

  short snap_mode = t->tsnap.mode;
  /* Build arrays of snap points. */
  seq_snap_target_points_alloc(snap_mode, snap_data, snap_targets);
  seq_snap_target_points_build(scene, snap_mode, snap_data, snap_targets);
  SEQ_collection_free(snap_targets);

  t->tsnap.seq_context = snap_data;
  bool snap_success = transform_snap_sequencer_calc(t);
  transform_snap_sequencer_data_free(snap_data);
  t->tsnap.seq_context = nullptr;

  float snap_offset = 0;
  if (snap_success) {
    t->tsnap.status |= (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
    transform_snap_sequencer_apply_translate(t, &snap_offset);
  }
  else {
    t->tsnap.status &= ~(SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }

  return snap_offset;
}

bool ED_transform_snap_sequencer_to_closest_strip_calc(Scene *scene,
                                                       ARegion *region,
                                                       int frame_1,
                                                       int frame_2,
                                                       int *r_snap_distance,
                                                       float *r_snap_frame)
{
  TransInfo t = {nullptr};
  t.scene = scene;
  t.region = region;
  t.values[0] = 0;
  t.data_type = &TransConvertType_Sequencer;

  t.tsnap.mode = eSnapMode(SEQ_tool_settings_snap_mode_get(scene));
  *r_snap_distance = transform_snap_sequencer_to_closest_strip_ex(&t, frame_1, frame_2);
  *r_snap_frame = t.tsnap.snap_target[0];
  return validSnap(&t);
}

void ED_draw_sequencer_snap_point(ARegion *region, const float snap_point)
{
  /* Reuse the snapping drawing code from the transform system. */
  TransInfo t = {nullptr};
  t.mode = TFM_SEQ_SLIDE;
  t.modifiers = MOD_SNAP;
  t.spacetype = SPACE_SEQ;
  t.tsnap.flag = SCE_SNAP;
  t.tsnap.status = (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  t.tsnap.snap_target[0] = snap_point;
  t.region = region;

  drawSnapping(&t);
}
