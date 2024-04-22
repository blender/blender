/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"

#include "ED_screen.hh"
#include "ED_transform.hh"

#include "UI_view2d.hh"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

struct TransSeqSnapData {
  blender::Array<int> source_snap_points;
  blender::Array<int> target_snap_points;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("TransSeqSnapData")
#endif
};

/* -------------------------------------------------------------------- */
/** \name Snap sources
 * \{ */

static int seq_get_snap_source_points_len(blender::Span<Sequence *> snap_sources)
{
  return snap_sources.size() * 2;
}

static int cmp_fn(const void *a, const void *b)
{
  return (*(int *)a - *(int *)b);
}

static bool seq_snap_source_points_build(const Scene *scene,
                                         TransSeqSnapData *snap_data,
                                         blender::Span<Sequence *> snap_sources)
{
  const size_t point_count_source = seq_get_snap_source_points_len(snap_sources);
  if (point_count_source == 0) {
    return false;
  }

  snap_data->source_snap_points.reinitialize(point_count_source);
  int i = 0;
  for (Sequence *seq : snap_sources) {
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
    BLI_assert(i <= snap_data->source_snap_points.size());
  }

  qsort(snap_data->source_snap_points.data(),
        snap_data->source_snap_points.size(),
        sizeof(int),
        cmp_fn);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap targets
 * \{ */

/* Add effect strips directly or indirectly connected to `seq_reference` to `collection`. */
static void query_strip_effects_fn(const Scene *scene,
                                   Sequence *seq_reference,
                                   ListBase *seqbase,
                                   blender::VectorSet<Sequence *> &strips)
{
  if (strips.contains(seq_reference)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }
  strips.add(seq_reference);

  /* Find all strips connected to `seq_reference`. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (SEQ_relation_is_effect_of_strip(seq_test, seq_reference)) {
      query_strip_effects_fn(scene, seq_test, seqbase, strips);
    }
  }
}

static blender::VectorSet<Sequence *> query_snap_targets(Scene *scene,
                                                         blender::Span<Sequence *> snap_sources,
                                                         bool exclude_selected)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  const short snap_flag = SEQ_tool_settings_snap_flag_get(scene);

  /* Effects will always change position with strip to which they are connected and they don't have
   * to be selected. Remove such strips from `snap_targets` collection. */
  blender::VectorSet effects_of_snap_sources = snap_sources;
  SEQ_iterator_set_expand(scene, seqbase, effects_of_snap_sources, query_strip_effects_fn);
  effects_of_snap_sources.remove_if(
      [&](Sequence *seq) { return SEQ_effect_get_num_inputs(seq->type) == 0; });

  blender::VectorSet<Sequence *> snap_targets;
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
    if (effects_of_snap_sources.contains(seq)) {
      continue;
    }

    snap_targets.add(seq);
  }

  return snap_targets;
}

static int seq_get_snap_target_points_count(const Scene *scene,
                                            short snap_mode,
                                            blender::Span<Sequence *> snap_targets)
{
  int count = 2; /* Strip start and end are always used. */

  if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
    count += 2;
  }

  count *= snap_targets.size();

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    count++;
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    count += BLI_listbase_count(&scene->markers);
  }

  return count;
}

static bool seq_snap_target_points_build(Scene *scene,
                                         short snap_mode,
                                         TransSeqSnapData *snap_data,
                                         blender::Span<Sequence *> snap_targets)
{
  const size_t point_count_target = seq_get_snap_target_points_count(
      scene, snap_mode, snap_targets);
  if (point_count_target == 0) {
    return false;
  }

  snap_data->target_snap_points.reinitialize(point_count_target);
  int i = 0;

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->target_snap_points[i] = scene->r.cfra;
    i++;
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
      snap_data->target_snap_points[i] = marker->frame;
      i++;
    }
  }

  for (Sequence *seq : snap_targets) {
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
  BLI_assert(i <= snap_data->target_snap_points.size());
  qsort(snap_data->target_snap_points.data(),
        snap_data->target_snap_points.size(),
        sizeof(int),
        cmp_fn);
  return true;
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
  if (ELEM(t->data_type, &TransConvertType_SequencerImage, &TransConvertType_SequencerRetiming)) {
    return nullptr;
  }

  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  Scene *scene = t->scene;
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  short snap_mode = t->tsnap.mode;

  blender::VectorSet<Sequence *> snap_sources = SEQ_query_selected_strips(seqbase);
  blender::VectorSet<Sequence *> snap_targets = query_snap_targets(scene, snap_sources, true);

  /* Build arrays of snap points. */
  if (!seq_snap_source_points_build(scene, snap_data, snap_sources) ||
      !seq_snap_target_points_build(scene, snap_mode, snap_data, snap_targets))
  {
    MEM_delete(snap_data);
    return nullptr;
  }

  return snap_data;
}

void transform_snap_sequencer_data_free(TransSeqSnapData *data)
{
  MEM_delete(data);
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

  for (int frame_src : snap_data->source_snap_points) {
    int snap_source_frame = frame_src + round_fl_to_int(t->values[0]);
    for (int snap_target_frame : snap_data->target_snap_points) {
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
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  blender::VectorSet<Sequence *> empty_col;
  blender::VectorSet<Sequence *> snap_targets = query_snap_targets(scene, empty_col, false);

  BLI_assert(frame_1 <= frame_2);
  snap_data->source_snap_points.reinitialize(2);
  snap_data->source_snap_points[0] = frame_1;
  snap_data->source_snap_points[1] = frame_2;

  short snap_mode = t->tsnap.mode;

  /* Build arrays of snap points. */
  seq_snap_target_points_build(scene, snap_mode, snap_data, snap_targets);

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
