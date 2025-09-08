/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cstddef>
#include <cstdlib>

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "ED_transform.hh"

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

namespace blender::ed::transform {

struct TransSeqSnapData {
  Vector<float2> source_snap_points;
  Vector<float2> target_snap_points;

  MEM_CXX_CLASS_ALLOC_FUNCS("TransSeqSnapData")
};

/* -------------------------------------------------------------------- */
/** \name Snap sources
 * \{ */

static VectorSet<Strip *> query_snap_sources_timeline(
    const Scene *scene, Map<SeqRetimingKey *, Strip *> &retiming_selection)
{
  VectorSet<Strip *> snap_sources;

  ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));
  snap_sources = seq::query_selected_strips(seqbase);

  /* Add strips owned by retiming keys to exclude these from targets */
  for (Strip *strip : retiming_selection.values()) {
    snap_sources.add(strip);
  }

  return snap_sources;
}

static VectorSet<Strip *> query_snap_sources_preview(const Scene *scene)
{
  VectorSet<Strip *> snap_sources;

  Editing *ed = seq::editing_get(scene);
  ListBase *channels = seq::channels_displayed_get(ed);

  snap_sources = seq::query_rendered_strips(
      scene, channels, ed->current_strips(), scene->r.cfra, 0);
  snap_sources.remove_if([&](Strip *strip) { return (strip->flag & SELECT) == 0; });

  return snap_sources;
}

static int cmp_fn(const void *a, const void *b)
{
  return round_fl_to_int((*(float2 *)a)[0] - (*(float2 *)b)[0]);
}

static void points_build_sources_timeline_strips(const Scene *scene,
                                                 TransSeqSnapData *snap_data,
                                                 const Span<Strip *> snap_sources)
{
  for (Strip *strip : snap_sources) {
    int left = 0, right = 0;
    if (strip->flag & SEQ_LEFTSEL && !(strip->flag & SEQ_RIGHTSEL)) {
      left = right = seq::time_left_handle_frame_get(scene, strip);
    }
    else if (strip->flag & SEQ_RIGHTSEL && !(strip->flag & SEQ_LEFTSEL)) {
      left = right = seq::time_right_handle_frame_get(scene, strip);
    }
    else {
      left = seq::time_left_handle_frame_get(scene, strip);
      right = seq::time_right_handle_frame_get(scene, strip);
    }

    /* Set only the x-positions when snapping in the timeline. */
    snap_data->source_snap_points.append(float2(left));
    snap_data->source_snap_points.append(float2(right));
  }

  qsort(snap_data->source_snap_points.data(),
        snap_data->source_snap_points.size(),
        sizeof(float2),
        cmp_fn);
}

static void points_build_sources_timeline_retiming(
    const Scene *scene,
    TransSeqSnapData *snap_data,
    const Map<SeqRetimingKey *, Strip *> &retiming_selection)
{
  for (auto item : retiming_selection.items()) {
    const int key_frame = seq::retiming_key_timeline_frame_get(scene, item.value, item.key);
    snap_data->source_snap_points.append(float2(key_frame));
  }

  qsort(snap_data->source_snap_points.data(),
        snap_data->source_snap_points.size(),
        sizeof(float2),
        cmp_fn);
}

static void points_build_sources_preview_image(const Scene *scene,
                                               TransSeqSnapData *snap_data,
                                               const Span<Strip *> snap_sources)
{
  for (Strip *strip : snap_sources) {
    const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);

    for (int i = 0; i < 4; i++) {
      snap_data->source_snap_points.append(strip_image_quad[i]);
    }

    /* Add origins last */
    const float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
    snap_data->source_snap_points.append(image_origin);
  }
}

static void points_build_sources_preview_origin(const Scene *scene,
                                                TransSeqSnapData *snap_data,
                                                const Span<Strip *> snap_sources)
{

  const size_t point_count_source = snap_sources.size();

  if (point_count_source == 0) {
    return;
  }

  snap_data->source_snap_points.reinitialize(point_count_source);
  int i = 0;
  for (Strip *strip : snap_sources) {
    /* Add origins last */
    float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
    snap_data->source_snap_points[i][0] = image_origin[0];
    snap_data->source_snap_points[i][1] = image_origin[1];
    i++;
  }

  BLI_assert(i <= snap_data->source_snap_points.size());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap targets
 * \{ */

/* Add effect strips directly or indirectly connected to `strip_reference` to `collection`. */
static void query_strip_effects_fn(const Scene *scene,
                                   Strip *strip_reference,
                                   ListBase *seqbase,
                                   VectorSet<Strip *> &strips)
{
  if (strips.contains(strip_reference)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }
  strips.add(strip_reference);

  /* Find all strips connected to `strip_reference`. */
  LISTBASE_FOREACH (Strip *, strip_test, seqbase) {
    if (seq::relation_is_effect_of_strip(strip_test, strip_reference)) {
      query_strip_effects_fn(scene, strip_test, seqbase, strips);
    }
  }
}

static VectorSet<Strip *> query_snap_targets_timeline(Scene *scene,
                                                      const Span<Strip *> snap_sources,
                                                      const bool exclude_selected)
{
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  const short snap_flag = seq::tool_settings_snap_flag_get(scene);

  /* Effects will always change position with strip to which they are connected and they don't
   * have to be selected. Remove such strips from `snap_targets` collection. */
  VectorSet effects_of_snap_sources = snap_sources;
  seq::iterator_set_expand(scene, seqbase, effects_of_snap_sources, query_strip_effects_fn);
  effects_of_snap_sources.remove_if([&](Strip *strip) {
    return strip->is_effect() && seq::effect_get_num_inputs(strip->type) == 0;
  });

  VectorSet<Strip *> snap_targets;
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (exclude_selected && strip->flag & SELECT) {
      continue; /* Selected are being transformed if there is no drag and drop. */
    }
    if (seq::render_is_muted(channels, strip) && (snap_flag & SEQ_SNAP_IGNORE_MUTED)) {
      continue;
    }
    if (strip->type == STRIP_TYPE_SOUND_RAM && (snap_flag & SEQ_SNAP_IGNORE_SOUND)) {
      continue;
    }
    if (effects_of_snap_sources.contains(strip)) {
      continue;
    }

    snap_targets.add(strip);
  }

  return snap_targets;
}

static VectorSet<Strip *> query_snap_targets_preview(const TransInfo *t)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;

  VectorSet<Strip *> snap_targets;

  /* We don't need to calculate strip snap targets if the option is unselected. */
  if ((snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) == 0) {
    return snap_targets;
  }

  Editing *ed = seq::editing_get(scene);
  ListBase *channels = seq::channels_displayed_get(ed);

  snap_targets = seq::query_rendered_strips(
      scene, channels, ed->current_strips(), scene->r.cfra, 0);

  /* Selected strips are only valid targets when snapping the cursor or origin. */
  if ((t->data_type == &TransConvertType_SequencerImage) && (t->flag & T_ORIGIN) == 0) {
    snap_targets.remove_if([&](Strip *strip) { return (strip->flag & SELECT) != 0; });
  }

  return snap_targets;
}

static Map<SeqRetimingKey *, Strip *> visible_retiming_keys_get(const Scene *scene,
                                                                Span<Strip *> snap_strip_targets)
{
  Map<SeqRetimingKey *, Strip *> visible_keys;

  for (Strip *strip : snap_strip_targets) {
    for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
      const int key_frame = seq::retiming_key_timeline_frame_get(scene, strip, &key);
      if (seq::time_strip_intersects_frame(scene, strip, key_frame)) {
        visible_keys.add(&key, strip);
      }
    }
  }

  return visible_keys;
}

static void points_build_targets_timeline(const Scene *scene,
                                          const short snap_mode,
                                          TransSeqSnapData *snap_data,
                                          const Span<Strip *> strip_targets)
{
  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->target_snap_points.append(float2(scene->r.cfra));
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
      snap_data->target_snap_points.append(float2(marker->frame));
    }
  }

  if (snap_mode & SEQ_SNAP_TO_FRAME_RANGE) {
    snap_data->target_snap_points.append(float2(PSFRA));
    snap_data->target_snap_points.append(float2(PEFRA + 1));
  }

  for (Strip *strip : strip_targets) {
    snap_data->target_snap_points.append(float2(seq::time_left_handle_frame_get(scene, strip)));
    snap_data->target_snap_points.append(float2(seq::time_right_handle_frame_get(scene, strip)));

    if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD) {
      int content_start = seq::time_start_frame_get(strip);
      int content_end = seq::time_content_end_frame_get(scene, strip);

      /* Effects and single image strips produce incorrect content length. Skip these strips. */
      if (strip->is_effect() || strip->len == 1) {
        content_start = seq::time_left_handle_frame_get(scene, strip);
        content_end = seq::time_right_handle_frame_get(scene, strip);
      }

      CLAMP(content_start,
            seq::time_left_handle_frame_get(scene, strip),
            seq::time_right_handle_frame_get(scene, strip));
      CLAMP(content_end,
            seq::time_left_handle_frame_get(scene, strip),
            seq::time_right_handle_frame_get(scene, strip));

      snap_data->target_snap_points.append(float2(content_start));
      snap_data->target_snap_points.append(float2(content_end));
    }
  }

  Map retiming_key_targets = visible_retiming_keys_get(scene, strip_targets);
  if (snap_mode & SEQ_SNAP_TO_RETIMING) {
    for (auto item : retiming_key_targets.items()) {
      const int key_frame = seq::retiming_key_timeline_frame_get(scene, item.value, item.key);
      snap_data->target_snap_points.append(float2(key_frame));
    }
  }

  qsort(snap_data->target_snap_points.data(),
        snap_data->target_snap_points.size(),
        sizeof(float2),
        cmp_fn);
}

static void points_build_targets_preview_general(const View2D *v2d,
                                                 const short snap_mode,
                                                 TransSeqSnapData *snap_data)
{
  if (snap_mode & SEQ_SNAP_TO_PREVIEW_BORDERS) {
    snap_data->target_snap_points.append(float2(v2d->tot.xmin, v2d->tot.ymin));
    snap_data->target_snap_points.append(float2(v2d->tot.xmax, v2d->tot.ymax));
    snap_data->target_snap_points.append(float2(v2d->tot.xmin, v2d->tot.ymax));
    snap_data->target_snap_points.append(float2(v2d->tot.xmax, v2d->tot.ymin));
  }

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_CENTER) {
    snap_data->target_snap_points.append(float2(0));
  }
}

static void points_build_targets_preview_image(const Scene *scene,
                                               const View2D *v2d,
                                               const short snap_mode,
                                               TransSeqSnapData *snap_data,
                                               const Span<Strip *> snap_targets)
{
  points_build_targets_preview_general(v2d, snap_mode, snap_data);

  if (snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) {
    for (Strip *strip : snap_targets) {
      const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);

      for (int i = 0; i < 4; i++) {
        snap_data->target_snap_points.append(strip_image_quad[i]);
      }

      const float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
      snap_data->target_snap_points.append(image_origin);
    }
  }
}

static void points_build_3x3_grid(const Scene *scene, TransSeqSnapData *snap_data, Strip *strip)
{
  const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);
  /* Corners. */
  for (int i = 0; i < 4; i++) {
    snap_data->target_snap_points.append(strip_image_quad[i]);
  }

  /* Middle top, bottom and center of the image. */
  const float2 tm = blender::math::interpolate(strip_image_quad[0], strip_image_quad[3], 0.5f);
  const float2 bm = blender::math::interpolate(strip_image_quad[1], strip_image_quad[2], 0.5f);
  const float2 mm = blender::math::interpolate(bm, tm, 0.5f);
  snap_data->target_snap_points.append(tm);
  snap_data->target_snap_points.append(mm);
  snap_data->target_snap_points.append(bm);
  /* Left and right. */
  snap_data->target_snap_points.append(
      blender::math::interpolate(strip_image_quad[2], strip_image_quad[3], 0.5f));
  snap_data->target_snap_points.append(
      blender::math::interpolate(strip_image_quad[0], strip_image_quad[1], 0.5f));
}

static void points_build_targets_preview_origin(const Scene *scene,
                                                TransSeqSnapData *snap_data,
                                                const Span<Strip *> snap_sources,
                                                const Span<Strip *> snap_targets)
{
  for (Strip *strip : snap_sources) {
    points_build_3x3_grid(scene, snap_data, strip);
  }

  for (Strip *strip : snap_targets) {
    points_build_3x3_grid(scene, snap_data, strip);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap utilities
 * \{ */

static float seq_snap_threshold_get_view_distance(const TransInfo *t)
{
  const int snap_distance = seq::tool_settings_snap_distance_get(t->scene);
  const View2D *v2d = &t->region->v2d;
  return UI_view2d_region_to_view_x(v2d, snap_distance) - UI_view2d_region_to_view_x(v2d, 0);
}

static int seq_snap_threshold_get_frame_distance(const TransInfo *t)
{
  return round_fl_to_int(seq_snap_threshold_get_view_distance(t));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap data
 * \{ */

static void snap_data_build_timeline(const TransInfo *t, TransSeqSnapData *snap_data)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;

  Map retiming_selection = seq::retiming_selection_get(seq::editing_get(scene));
  VectorSet<Strip *> snap_sources = query_snap_sources_timeline(scene, retiming_selection);
  VectorSet<Strip *> snap_targets = query_snap_targets_timeline(scene, snap_sources, true);

  /* Build arrays of snap points. */
  if (t->data_type == &TransConvertType_Sequencer) {
    points_build_sources_timeline_strips(scene, snap_data, snap_sources);
  }
  else { /* &TransConvertType_SequencerRetiming */
    points_build_sources_timeline_retiming(scene, snap_data, retiming_selection);
  }
  points_build_targets_timeline(scene, snap_mode, snap_data, snap_targets);
}

static void snap_data_build_preview(const TransInfo *t, TransSeqSnapData *snap_data)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;
  View2D *v2d = &t->region->v2d;
  SpaceSeq *sseq = static_cast<SpaceSeq *>(t->area->spacedata.first);

  VectorSet<Strip *> snap_sources = query_snap_sources_preview(scene);
  VectorSet<Strip *> snap_targets;

  snap_targets = query_snap_targets_preview(t);

  /* Build arrays of snap points. */
  if (t->data_type == &TransConvertType_SequencerImage) {
    if (t->flag & T_ORIGIN) {
      points_build_sources_preview_origin(scene, snap_data, snap_sources);
      points_build_targets_preview_origin(scene, snap_data, snap_sources, snap_targets);
    }
    else {
      points_build_sources_preview_image(scene, snap_data, snap_sources);
      points_build_targets_preview_image(scene, v2d, snap_mode, snap_data, snap_targets);
    }
  }
  else if (t->data_type == &TransConvertType_CursorSequencer) {
    float2 cursor_view = float2(sseq->cursor) * float2(t->aspect);
    snap_data->source_snap_points.append(cursor_view);
    points_build_targets_preview_image(scene, v2d, snap_mode, snap_data, snap_targets);
  }
}

TransSeqSnapData *snap_sequencer_data_alloc(const TransInfo *t)
{
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  if (ELEM(t->data_type, &TransConvertType_Sequencer, &TransConvertType_SequencerRetiming)) {
    snap_data_build_timeline(t, snap_data);
  }
  else {
    snap_data_build_preview(t, snap_data);
  }

  if (snap_data->source_snap_points.is_empty() || snap_data->target_snap_points.is_empty()) {
    MEM_delete(snap_data);
    return nullptr;
  }

  return snap_data;
}

void snap_sequencer_data_free(TransSeqSnapData *data)
{
  MEM_delete(data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap calculation
 * \{ */

static bool snap_calc_timeline(TransInfo *t, const TransSeqSnapData *snap_data)
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

  float2 best_offset(float(best_target_frame - best_source_frame), 0.0f);
  if (transform_convert_sequencer_clamp(t, best_offset)) {
    return false;
  }

  t->tsnap.snap_target[0] = best_target_frame;
  t->tsnap.snap_source[0] = best_source_frame;
  return true;
}

static bool snap_calc_preview_origin(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Store best snap candidates in x and y directions separately. */
  float best_dist(std::numeric_limits<float>::max());
  float2 best_target_point(0.0f);
  float2 best_source_point(0.0f);

  for (const float2 snap_source_point : snap_data->source_snap_points) {
    for (const float2 snap_target_point : snap_data->target_snap_points) {
      /* First update snaps in x direction, then y direction. */
      const float2 transformed_point(snap_source_point.x + t->values[0],
                                     snap_source_point.y + t->values[1]);
      const float dist = blender::math::distance(snap_target_point, transformed_point);
      if (dist > best_dist) {
        continue;
      }

      best_dist = dist;
      best_target_point = snap_target_point;
      best_source_point = snap_source_point;
    }
  }

  if (best_dist <= seq_snap_threshold_get_view_distance(t)) {
    copy_v2_v2(t->tsnap.snap_target, best_target_point);
    copy_v2_v2(t->tsnap.snap_source, best_source_point);
    t->tsnap.direction |= DIR_GLOBAL_X | DIR_GLOBAL_Y;
    return true;
  }
  return false;
}

static bool snap_calc_preview_image(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Store best snap candidates in x and y directions separately. */
  float2 best_dist(std::numeric_limits<float>::max());
  float2 best_target_point(0.0f);
  float2 best_source_point(0.0f);

  for (const float2 snap_source_point : snap_data->source_snap_points) {
    for (const float2 snap_target_point : snap_data->target_snap_points) {
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

bool snap_sequencer_calc(TransInfo *t)
{
  const TransSeqSnapData *snap_data = t->tsnap.seq_context;
  if (snap_data == nullptr) {
    return false;
  }

  if (ELEM(t->data_type, &TransConvertType_Sequencer, &TransConvertType_SequencerRetiming)) {
    return snap_calc_timeline(t, snap_data);
  }
  if (t->flag & T_ORIGIN) {
    return snap_calc_preview_origin(t, snap_data);
  }
  return snap_calc_preview_image(t, snap_data);
}

void snap_sequencer_apply_seqslide(TransInfo *t, float *vec)
{
  *vec = t->tsnap.snap_target[0] - t->tsnap.snap_source[0];
}

void snap_sequencer_image_apply_translate(TransInfo *t, float vec[2])
{
  /* Apply snap along x and y axes independently. */
  if (t->tsnap.direction & DIR_GLOBAL_X) {
    vec[0] = t->tsnap.snap_target[0] - t->tsnap.snap_source[0];
  }

  if (t->tsnap.direction & DIR_GLOBAL_Y) {
    vec[1] = t->tsnap.snap_target[1] - t->tsnap.snap_source[1];
  }
}

static int snap_sequencer_to_closest_strip_ex(TransInfo *t, const int frame_1, const int frame_2)
{
  Scene *scene = t->scene;
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  VectorSet<Strip *> empty_col;
  VectorSet<Strip *> snap_targets = query_snap_targets_timeline(scene, empty_col, false);

  BLI_assert(frame_1 <= frame_2);

  snap_data->source_snap_points[0][0] = frame_1;
  snap_data->source_snap_points[1][0] = frame_2;

  short snap_mode = t->tsnap.mode;

  /* Build arrays of snap target frames. */
  points_build_targets_timeline(scene, snap_mode, snap_data, snap_targets);

  t->tsnap.seq_context = snap_data;
  bool snap_success = snap_sequencer_calc(t);
  snap_sequencer_data_free(snap_data);
  t->tsnap.seq_context = nullptr;

  float snap_offset = 0;
  if (snap_success) {
    t->tsnap.status |= (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
    snap_sequencer_apply_seqslide(t, &snap_offset);
  }
  else {
    t->tsnap.status &= ~(SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  }

  return snap_offset;
}

bool snap_sequencer_to_closest_strip_calc(Scene *scene,
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

  t.tsnap.mode = eSnapMode(seq::tool_settings_snap_mode_get(scene));
  *r_snap_distance = snap_sequencer_to_closest_strip_ex(&t, frame_1, frame_2);
  *r_snap_frame = t.tsnap.snap_target[0];
  return validSnap(&t);
}

void sequencer_snap_point(ARegion *region, const float snap_point)
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

/** \} */

}  // namespace blender::ed::transform
