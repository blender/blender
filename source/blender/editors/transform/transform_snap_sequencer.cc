/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstddef>
#include <cstdlib>

#include "BLI_assert.h"
#include "BLI_map.hh"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"

#include "ED_transform.hh"

#include "SEQ_retiming.hh"
#include "UI_view2d.hh"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

struct TransSeqSnapData {
  blender::Array<blender::float2> source_snap_points;
  blender::Array<blender::float2> target_snap_points;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("TransSeqSnapData")
#endif
};

/* -------------------------------------------------------------------- */
/** \name Snap sources
 * \{ */

static blender::VectorSet<Sequence *> query_snap_sources_timeline(
    const Scene *scene, blender::Map<SeqRetimingKey *, Sequence *> &retiming_selection)
{
  blender::VectorSet<Sequence *> snap_sources;

  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  snap_sources = SEQ_query_selected_strips(seqbase);

  /* Add strips owned by retiming keys to exclude these from targets */
  for (Sequence *seq : retiming_selection.values()) {
    snap_sources.add(seq);
  }

  return snap_sources;
}

static blender::VectorSet<Sequence *> query_snap_sources_preview(const Scene *scene)
{
  blender::VectorSet<Sequence *> snap_sources;

  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  snap_sources = SEQ_query_rendered_strips(scene, channels, ed->seqbasep, scene->r.cfra, 0);
  snap_sources.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });

  return snap_sources;
}

static int seq_get_snap_source_points_count_timeline(const blender::Span<Sequence *> snap_sources)
{
  return snap_sources.size() * 2;
}

static int seq_get_snap_source_points_count_preview(const blender::Span<Sequence *> snap_sources)
{
  /* Source points are four corners and the center of an image quad. */
  return snap_sources.size() * 5;
}

static int cmp_fn(const void *a, const void *b)
{
  return round_fl_to_int((*(blender::float2 *)a)[0] - (*(blender::float2 *)b)[0]);
}

static void seq_snap_source_points_build_timeline_strips(
    const Scene *scene, TransSeqSnapData *snap_data, const blender::Span<Sequence *> snap_sources)
{

  const size_t point_count_source = seq_get_snap_source_points_count_timeline(snap_sources);
  if (point_count_source == 0) {
    return;
  }

  snap_data->source_snap_points.reinitialize(point_count_source);
  int i = 0;
  for (Sequence *seq : snap_sources) {
    int left = 0, right = 0;
    if (seq->flag & SEQ_LEFTSEL && !(seq->flag & SEQ_RIGHTSEL)) {
      left = right = SEQ_time_left_handle_frame_get(scene, seq);
    }
    else if (seq->flag & SEQ_RIGHTSEL && !(seq->flag & SEQ_LEFTSEL)) {
      left = right = SEQ_time_right_handle_frame_get(scene, seq);
    }
    else {
      left = SEQ_time_left_handle_frame_get(scene, seq);
      right = SEQ_time_right_handle_frame_get(scene, seq);
    }

    /* Set only the x-positions when snapping in the timeline. */
    snap_data->source_snap_points[i][0] = left;
    snap_data->source_snap_points[i + 1][0] = right;
    i += 2;
    BLI_assert(i <= snap_data->source_snap_points.size());
  }

  qsort(snap_data->source_snap_points.data(),
        snap_data->source_snap_points.size(),
        sizeof(blender::float2),
        cmp_fn);
}

static void seq_snap_source_points_build_timeline_retiming(
    const Scene *scene,
    TransSeqSnapData *snap_data,
    const blender::Map<SeqRetimingKey *, Sequence *> &retiming_selection)
{

  const size_t point_count_source = retiming_selection.size();
  if (point_count_source == 0) {
    return;
  }

  snap_data->source_snap_points.reinitialize(point_count_source);
  int i = 0;
  for (auto item : retiming_selection.items()) {
    const int key_frame = SEQ_retiming_key_timeline_frame_get(scene, item.value, item.key);
    snap_data->source_snap_points[i][0] = key_frame;
    i++;
  }

  qsort(snap_data->source_snap_points.data(),
        snap_data->source_snap_points.size(),
        sizeof(blender::float2),
        cmp_fn);
}

static bool seq_snap_source_points_build_preview(const Scene *scene,
                                                 TransSeqSnapData *snap_data,
                                                 const blender::Span<Sequence *> snap_sources)
{

  const size_t point_count_source = seq_get_snap_source_points_count_preview(snap_sources);
  if (point_count_source == 0) {
    return false;
  }

  snap_data->source_snap_points.reinitialize(point_count_source);
  int i = 0;
  for (Sequence *seq : snap_sources) {
    float seq_image_quad[4][2];
    SEQ_image_transform_final_quad_get(scene, seq, seq_image_quad);

    for (int j = 0; j < 4; j++) {
      snap_data->source_snap_points[i][0] = seq_image_quad[j][0];
      snap_data->source_snap_points[i][1] = seq_image_quad[j][1];
      i++;
    }

    /* Add origins last */
    float image_origin[2];
    SEQ_image_transform_origin_offset_pixelspace_get(scene, seq, image_origin);
    snap_data->source_snap_points[i][0] = image_origin[0];
    snap_data->source_snap_points[i][1] = image_origin[1];
    i++;

    BLI_assert(i <= snap_data->source_snap_points.size());
  }

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

static blender::VectorSet<Sequence *> query_snap_targets_timeline(
    Scene *scene, const blender::Span<Sequence *> snap_sources, const bool exclude_selected)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  const short snap_flag = SEQ_tool_settings_snap_flag_get(scene);

  /* Effects will always change position with strip to which they are connected and they don't
   * have to be selected. Remove such strips from `snap_targets` collection. */
  blender::VectorSet effects_of_snap_sources = snap_sources;
  SEQ_iterator_set_expand(scene, seqbase, effects_of_snap_sources, query_strip_effects_fn);
  effects_of_snap_sources.remove_if([&](Sequence *seq) {
    return (seq->type & SEQ_TYPE_EFFECT) != 0 && SEQ_effect_get_num_inputs(seq->type) == 0;
  });

  blender::VectorSet<Sequence *> snap_targets;
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (exclude_selected && seq->flag & SELECT) {
      continue; /* Selected are being transformed if there is no drag and drop. */
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

static blender::VectorSet<Sequence *> query_snap_targets_preview(Scene *scene,
                                                                 const short snap_mode)
{
  blender::VectorSet<Sequence *> snap_targets;

  /* We don't need to calculate strip snap targets if the option is unselected. */
  if ((snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) == 0) {
    return snap_targets;
  }

  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  snap_targets = SEQ_query_rendered_strips(scene, channels, ed->seqbasep, scene->r.cfra, 0);
  snap_targets.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 1; });

  return snap_targets;
}

static blender::Map<SeqRetimingKey *, Sequence *> visible_retiming_keys_get(
    const Scene *scene, blender::Span<Sequence *> snap_strip_targets)
{
  blender::Map<SeqRetimingKey *, Sequence *> visible_keys;

  for (Sequence *seq : snap_strip_targets) {
    for (SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
      const int key_frame = SEQ_retiming_key_timeline_frame_get(scene, seq, &key);
      if (SEQ_time_strip_intersects_frame(scene, seq, key_frame)) {
        visible_keys.add(&key, seq);
      }
    }
  }

  return visible_keys;
}

static int seq_get_snap_target_points_count_timeline(
    const Scene *scene,
    const short snap_mode,
    const blender::Span<Sequence *> snap_strip_targets,
    const blender::Map<SeqRetimingKey *, Sequence *> &retiming_targets)
{
  int count = 0;

  if (snap_mode & SEQ_SNAP_TO_STRIPS) {
    count += 2; /* Strip start and end are always used. */
  }

  if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
    count += 2;
  }

  count *= snap_strip_targets.size();

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    count++;
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    count += BLI_listbase_count(&scene->markers);
  }

  if (snap_mode & SEQ_SNAP_TO_RETIMING) {
    count += retiming_targets.size();
  }
  return count;
}

static int seq_get_snap_target_points_count_preview(const short snap_mode,
                                                    const blender::Span<Sequence *> snap_targets)
{
  int count = 0;

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_BORDERS) {
    /* Opposite corners of the view have enough information to snap to all four corners. */
    count += 2;
  }

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_CENTER) {
    count++;
  }

  if (snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) {
    /* Snap to other strips' corners and center. */
    count += snap_targets.size() * 5;
  }

  return count;
}

static void seq_snap_target_points_build_timeline(const Scene *scene,
                                                  const short snap_mode,
                                                  TransSeqSnapData *snap_data,
                                                  const blender::Span<Sequence *> strip_targets)
{
  blender::Map retiming_key_targets = visible_retiming_keys_get(scene, strip_targets);

  const size_t point_count_target = seq_get_snap_target_points_count_timeline(
      scene, snap_mode, strip_targets, retiming_key_targets);
  if (point_count_target == 0) {
    return;
  }

  snap_data->target_snap_points.reinitialize(point_count_target);
  int i = 0;

  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->target_snap_points[i][0] = scene->r.cfra;
    i++;
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
      snap_data->target_snap_points[i][0] = marker->frame;
      i++;
    }
  }

  for (Sequence *seq : strip_targets) {
    snap_data->target_snap_points[i][0] = SEQ_time_left_handle_frame_get(scene, seq);
    snap_data->target_snap_points[i + 1][0] = SEQ_time_right_handle_frame_get(scene, seq);
    i += 2;

    if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
      int content_start = SEQ_time_start_frame_get(seq);
      int content_end = SEQ_time_content_end_frame_get(scene, seq);

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

      snap_data->target_snap_points[i][0] = content_start;
      snap_data->target_snap_points[i + 1][0] = content_end;
      i += 2;
    }
  }

  if (snap_mode & SEQ_SNAP_TO_RETIMING) {
    for (auto item : retiming_key_targets.items()) {
      const int key_frame = SEQ_retiming_key_timeline_frame_get(scene, item.value, item.key);
      snap_data->target_snap_points[i][0] = key_frame;
      i++;
    }
  }

  BLI_assert(i <= snap_data->target_snap_points.size());
  qsort(snap_data->target_snap_points.data(),
        snap_data->target_snap_points.size(),
        sizeof(blender::float2),
        cmp_fn);
}

static bool seq_snap_target_points_build_preview(const Scene *scene,
                                                 const View2D *v2d,
                                                 const short snap_mode,
                                                 TransSeqSnapData *snap_data,
                                                 const blender::Span<Sequence *> snap_targets)
{

  const size_t point_count_target = seq_get_snap_target_points_count_preview(snap_mode,
                                                                             snap_targets);
  if (point_count_target == 0) {
    return false;
  }

  snap_data->target_snap_points.reinitialize(point_count_target);
  int i = 0;

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_BORDERS) {
    snap_data->target_snap_points[i][0] = v2d->tot.xmin;
    snap_data->target_snap_points[i][1] = v2d->tot.ymin;

    snap_data->target_snap_points[i + 1][0] = v2d->tot.xmax;
    snap_data->target_snap_points[i + 1][1] = v2d->tot.ymax;

    i += 2;
  }

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_CENTER) {
    snap_data->target_snap_points[i][0] = 0;
    snap_data->target_snap_points[i][1] = 0;

    i++;
  }

  if (snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) {
    for (Sequence *seq : snap_targets) {
      float seq_image_quad[4][2];
      SEQ_image_transform_final_quad_get(scene, seq, seq_image_quad);

      for (int j = 0; j < 4; j++) {
        snap_data->target_snap_points[i][0] = seq_image_quad[j][0];
        snap_data->target_snap_points[i][1] = seq_image_quad[j][1];
        i++;
      }

      float image_origin[2];
      SEQ_image_transform_origin_offset_pixelspace_get(scene, seq, image_origin);
      snap_data->target_snap_points[i][0] = image_origin[0];
      snap_data->target_snap_points[i][1] = image_origin[1];

      i++;
    }
  }
  BLI_assert(i <= snap_data->target_snap_points.size());

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap utilities
 * \{ */

static float seq_snap_threshold_get_view_distance(const TransInfo *t)
{
  const int snap_distance = SEQ_tool_settings_snap_distance_get(t->scene);
  const View2D *v2d = &t->region->v2d;
  return UI_view2d_region_to_view_x(v2d, snap_distance) - UI_view2d_region_to_view_x(v2d, 0);
}

static int seq_snap_threshold_get_frame_distance(const TransInfo *t)
{
  return round_fl_to_int(seq_snap_threshold_get_view_distance(t));
}

/** \} */

static TransSeqSnapData *transform_snap_sequencer_data_alloc_timeline_strips(const TransInfo *t)
{
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;

  blender::Map retiming_selection = SEQ_retiming_selection_get(SEQ_editing_get(scene));
  blender::VectorSet<Sequence *> snap_sources = query_snap_sources_timeline(scene,
                                                                            retiming_selection);
  blender::VectorSet<Sequence *> snap_targets = query_snap_targets_timeline(
      scene, snap_sources, true);

  /* Build arrays of snap points. */
  if (t->data_type == &TransConvertType_Sequencer) {
    seq_snap_source_points_build_timeline_strips(scene, snap_data, snap_sources);
  }
  else { /* &TransConvertType_SequencerRetiming */
    seq_snap_source_points_build_timeline_retiming(scene, snap_data, retiming_selection);
  }
  seq_snap_target_points_build_timeline(scene, snap_mode, snap_data, snap_targets);

  if (snap_data->source_snap_points.is_empty() || snap_data->target_snap_points.is_empty()) {
    MEM_delete(snap_data);
    return nullptr;
  }

  return snap_data;
}

static TransSeqSnapData *transform_snap_sequencer_data_alloc_preview(const TransInfo *t)
{
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;
  View2D *v2d = &t->region->v2d;

  blender::VectorSet<Sequence *> snap_sources = query_snap_sources_preview(scene);
  blender::VectorSet<Sequence *> snap_targets = query_snap_targets_preview(scene, snap_mode);

  /* Build arrays of snap points. */
  if (!seq_snap_source_points_build_preview(scene, snap_data, snap_sources) ||
      !seq_snap_target_points_build_preview(scene, v2d, snap_mode, snap_data, snap_targets))
  {
    MEM_delete(snap_data);
    return nullptr;
  }

  return snap_data;
}

TransSeqSnapData *transform_snap_sequencer_data_alloc(const TransInfo *t)
{
  if (ELEM(t->data_type, &TransConvertType_Sequencer, &TransConvertType_SequencerRetiming)) {
    return transform_snap_sequencer_data_alloc_timeline_strips(t);
  }
  return transform_snap_sequencer_data_alloc_preview(t);
}

void transform_snap_sequencer_data_free(TransSeqSnapData *data)
{
  MEM_delete(data);
}

static bool transform_snap_sequencer_calc_timeline(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Prevent snapping when constrained to Y axis. */
  if (t->con.mode & CON_APPLY && t->con.mode & CON_AXIS1) {
    return false;
  }

  int best_dist = MAXFRAME, best_target_frame = 0, best_source_frame = 0;

  for (const float *snap_source_point : snap_data->source_snap_points) {
    for (const float *snap_target_point : snap_data->target_snap_points) {
      int snap_source_frame = snap_source_point[0];
      int snap_target_frame = snap_target_point[0];
      int dist = abs(snap_target_frame - (snap_source_frame + round_fl_to_int(t->values[0])));
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

static bool transform_snap_sequencer_calc_preview(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Store best snap candidates in x and y directions separately. */
  blender::float2 best_dist(std::numeric_limits<float>::max());
  blender::float2 best_target_point(0.0f);
  blender::float2 best_source_point(0.0f);

  for (const float *snap_source_point : snap_data->source_snap_points) {
    for (const float *snap_target_point : snap_data->target_snap_points) {
      /* First update snaps in x direction, then y direction. */
      for (int i = 0; i < 2; i++) {
        int dist = abs(snap_target_point[i] - (snap_source_point[i] + t->values[i]));
        if (dist > best_dist[i]) {
          continue;
        }

        best_dist[i] = dist;
        best_target_point[i] = snap_target_point[i];
        best_source_point[i] = snap_source_point[i];
      }
    }
  }

  t->tsnap.direction &= ~(DIR_GLOBAL_X | DIR_GLOBAL_Y);
  float thr = seq_snap_threshold_get_view_distance(t);

  if (best_dist[0] <= thr) {
    t->tsnap.snap_target[0] = best_target_point[0];
    t->tsnap.snap_source[0] = best_source_point[0];
    t->tsnap.direction |= DIR_GLOBAL_X;
  }

  if (best_dist[1] <= thr) {
    t->tsnap.snap_target[1] = best_target_point[1];
    t->tsnap.snap_source[1] = best_source_point[1];
    t->tsnap.direction |= DIR_GLOBAL_Y;
  }

  return (best_dist[0] <= thr || best_dist[1] <= thr);
}

bool transform_snap_sequencer_calc(TransInfo *t)
{
  const TransSeqSnapData *snap_data = t->tsnap.seq_context;
  if (snap_data == nullptr) {
    return false;
  }

  if (ELEM(t->data_type, &TransConvertType_Sequencer, &TransConvertType_SequencerRetiming)) {
    return transform_snap_sequencer_calc_timeline(t, snap_data);
  }
  else {
    return transform_snap_sequencer_calc_preview(t, snap_data);
  }
}

void transform_snap_sequencer_apply_seqslide(TransInfo *t, float *vec)
{
  *vec = t->tsnap.snap_target[0] - t->tsnap.snap_source[0];
}

void transform_snap_sequencer_image_apply_translate(TransInfo *t, float vec[2])
{
  /* Apply snap along x and y axes independently. */
  if (t->tsnap.direction & DIR_GLOBAL_X) {
    vec[0] = t->tsnap.snap_target[0] - t->tsnap.snap_source[0];
  }

  if (t->tsnap.direction & DIR_GLOBAL_Y) {
    vec[1] = t->tsnap.snap_target[1] - t->tsnap.snap_source[1];
  }
}

static int transform_snap_sequencer_to_closest_strip_ex(TransInfo *t,
                                                        const int frame_1,
                                                        const int frame_2)
{
  Scene *scene = t->scene;
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  blender::VectorSet<Sequence *> empty_col;
  blender::VectorSet<Sequence *> snap_targets = query_snap_targets_timeline(
      scene, empty_col, false);

  BLI_assert(frame_1 <= frame_2);
  snap_data->source_snap_points.reinitialize(2);
  snap_data->source_snap_points[0][0] = frame_1;
  snap_data->source_snap_points[1][0] = frame_2;

  short snap_mode = t->tsnap.mode;

  /* Build arrays of snap target frames. */
  seq_snap_target_points_build_timeline(scene, snap_mode, snap_data, snap_targets);

  t->tsnap.seq_context = snap_data;
  bool snap_success = transform_snap_sequencer_calc(t);
  transform_snap_sequencer_data_free(snap_data);
  t->tsnap.seq_context = nullptr;

  float snap_offset = 0;
  if (snap_success) {
    t->tsnap.status |= (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
    transform_snap_sequencer_apply_seqslide(t, &snap_offset);
  }
  else {
    t->tsnap.status &= ~(SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }

  return snap_offset;
}

bool ED_transform_snap_sequencer_to_closest_strip_calc(Scene *scene,
                                                       ARegion *region,
                                                       const int frame_1,
                                                       const int frame_2,
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
