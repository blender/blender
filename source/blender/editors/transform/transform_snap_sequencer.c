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
 */

/** \file
 * \ingroup edtransform
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "ED_screen.h"

#include "UI_view2d.h"

#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_sequencer.h"

#include "transform.h"
#include "transform_snap.h"

typedef struct TransSeqSnapData {
  int *source_snap_points;
  int *target_snap_points;
  int source_snap_point_count;
  int target_snap_point_count;
  int final_snap_frame;
} TransSeqSnapData;

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
  snap_data->source_snap_points = MEM_callocN(sizeof(int) * point_count, __func__);
  memset(snap_data->source_snap_points, 0, sizeof(int));
  snap_data->source_snap_point_count = point_count;
}

static int cmp_fn(const void *a, const void *b)
{
  return (*(int *)a - *(int *)b);
}

static void seq_snap_source_points_build(const TransInfo *UNUSED(t),
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_sources)
{
  int i = 0;
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, snap_sources) {
    int left = 0, right = 0;
    if (seq->flag & SEQ_LEFTSEL) {
      left = right = seq->startdisp;
    }
    else if (seq->flag & SEQ_RIGHTSEL) {
      left = right = seq->enddisp;
    }
    else {
      left = seq->startdisp;
      right = seq->enddisp;
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
static void query_strip_effects_fn(Sequence *seq_reference,
                                   ListBase *seqbase,
                                   SeqCollection *collection)
{
  if (!SEQ_collection_append_strip(seq_reference, collection)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  /* Find all strips connected to `seq_reference`. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->seq1 == seq_reference || seq_test->seq2 == seq_reference ||
        seq_test->seq3 == seq_reference) {
      query_strip_effects_fn(seq_test, seqbase, collection);
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

static SeqCollection *query_snap_targets(const TransInfo *t, SeqCollection *snap_sources)
{
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(t->scene));
  const short snap_flag = SEQ_tool_settings_snap_flag_get(t->scene);
  SeqCollection *snap_targets = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->flag & SELECT) {
      continue; /* Selected are being transformed. */
    }
    if ((seq->flag & SEQ_MUTE) && (snap_flag & SEQ_SNAP_IGNORE_MUTED)) {
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
  SEQ_collection_expand(seqbase, snap_sources_temp, query_strip_effects_fn);
  SeqCollection *snap_sources_effects = seq_collection_extract_effects(snap_sources_temp);
  SEQ_collection_exclude(snap_targets, snap_sources_effects);
  SEQ_collection_free(snap_sources_temp);

  return snap_targets;
}

static int seq_get_snap_target_points_count(const TransInfo *t,
                                            TransSeqSnapData *UNUSED(snap_data),
                                            SeqCollection *snap_targets)
{
  const short snap_mode = t->tsnap.mode;

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

static void seq_snap_target_points_alloc(const TransInfo *t,
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_targets)
{
  const size_t point_count = seq_get_snap_target_points_count(t, snap_data, snap_targets);
  snap_data->target_snap_points = MEM_callocN(sizeof(int) * point_count, __func__);
  memset(snap_data->target_snap_points, 0, sizeof(int));
  snap_data->target_snap_point_count = point_count;
}

static void seq_snap_target_points_build(const TransInfo *t,
                                         TransSeqSnapData *snap_data,
                                         SeqCollection *snap_targets)
{
  const Scene *scene = t->scene;
  const short snap_mode = t->tsnap.mode;

  int i = 0;

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->target_snap_points[i] = CFRA;
    i++;
  }

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, snap_targets) {
    snap_data->target_snap_points[i] = seq->startdisp;
    snap_data->target_snap_points[i + 1] = seq->enddisp;
    i += 2;

    if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
      int content_start = min_ii(seq->enddisp, seq->start);
      int content_end = max_ii(seq->startdisp, seq->start + seq->len);
      /* Effects and single image strips produce incorrect content length. Skip these strips. */
      if ((seq->type & SEQ_TYPE_EFFECT) != 0 || seq->len == 1) {
        content_start = seq->startdisp;
        content_end = seq->enddisp;
      }

      CLAMP(content_start, seq->startdisp, seq->enddisp);
      CLAMP(content_end, seq->startdisp, seq->enddisp);

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
  const struct View2D *v2d = &t->region->v2d;
  return round_fl_to_int(UI_view2d_region_to_view_x(v2d, snap_distance) -
                         UI_view2d_region_to_view_x(v2d, 0));
}

/** \} */

TransSeqSnapData *transform_snap_sequencer_data_alloc(const TransInfo *t)
{
  if (t->data_type == TC_SEQ_IMAGE_DATA) {
    return NULL;
  }

  TransSeqSnapData *snap_data = MEM_callocN(sizeof(TransSeqSnapData), __func__);
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(t->scene));

  SeqCollection *snap_sources = SEQ_query_selected_strips(seqbase);
  SeqCollection *snap_targets = query_snap_targets(t, snap_sources);

  if (SEQ_collection_len(snap_sources) == 0) {
    SEQ_collection_free(snap_targets);
    SEQ_collection_free(snap_sources);
    MEM_freeN(snap_data);
    return NULL;
  }

  /* Build arrays of snap points. */
  seq_snap_source_points_alloc(snap_data, snap_sources);
  seq_snap_source_points_build(t, snap_data, snap_sources);
  SEQ_collection_free(snap_sources);

  seq_snap_target_points_alloc(t, snap_data, snap_targets);
  seq_snap_target_points_build(t, snap_data, snap_targets);
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
  if (snap_data == NULL) {
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

  t->tsnap.snapPoint[0] = best_target_frame;
  t->tsnap.snapTarget[0] = best_source_frame;
  return true;
}

void transform_snap_sequencer_apply_translate(TransInfo *t, float *vec)
{
  *vec += t->tsnap.snapPoint[0] - t->tsnap.snapTarget[0];
}
