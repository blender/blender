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
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

struct TransSeqSnapData {
  Vector<float2> sources;
  Vector<float2> targets;

  MEM_CXX_CLASS_ALLOC_FUNCS("TransSeqSnapData")
};

/* In timeline snap data, a channel of 0 indicates "this snap type always applies to all channels",
 * e.g. for snapping to frame ranges or markers, rather than individual strips.  */
static constexpr int all_channels = 0;

/* -------------------------------------------------------------------- */
/** \name Snap Utilities
 * \{ */

static float snap_distance_view_threshold_get(const TransInfo *t)
{
  const int snap_distance = seq::tool_settings_snap_distance_get(t->scene);
  const View2D *v2d = &t->region->v2d;
  return ui::view2d_region_to_view_x(v2d, snap_distance) - ui::view2d_region_to_view_x(v2d, 0);
}

static int snap_distance_frame_threshold_get(const TransInfo *t)
{
  return round_fl_to_int(snap_distance_view_threshold_get(t));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Sources
 * \{ */

static VectorSet<Strip *> query_strip_sources_timeline(const Scene *scene)
{
  const Editing *ed = seq::editing_get(scene);
  ListBaseT<Strip> *seqbase = seq::active_seqbase_get(seq::editing_get(scene));

  VectorSet<Strip *> strip_sources = seq::query_selected_strips(seqbase);

  const Map retiming_selection = seq::retiming_selection_get(ed);
  /* Strips owned by retiming keys are technically not selected,
   * but adding them to strip sources ensures they will be properly excluded from targets. */
  for (Strip *strip : retiming_selection.values()) {
    strip_sources.add(strip);
  }

  return strip_sources;
}

static VectorSet<Strip *> query_strip_sources_preview(const Scene *scene)
{
  VectorSet<Strip *> strip_sources;

  Editing *ed = seq::editing_get(scene);
  ListBaseT<SeqTimelineChannel> *channels = seq::channels_displayed_get(ed);

  strip_sources = seq::query_rendered_strips(
      scene, channels, ed->current_strips(), scene->r.cfra, 0);
  strip_sources.remove_if([&](Strip *strip) { return (strip->flag & SEQ_SELECT) == 0; });

  return strip_sources;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Targets
 * \{ */

/* Add effect strips directly or indirectly connected to `strip_reference` to `collection`. */
static void query_strip_effects_fn(Strip *strip_reference,
                                   ListBaseT<Strip> *seqbase,
                                   VectorSet<Strip *> &strips)
{
  if (strips.contains(strip_reference)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }
  strips.add(strip_reference);

  /* Find all strips connected to `strip_reference`. */
  for (Strip &strip_test : *seqbase) {
    if (seq::relation_is_effect_of_strip(&strip_test, strip_reference)) {
      query_strip_effects_fn(&strip_test, seqbase, strips);
    }
  }
}

static VectorSet<Strip *> query_strip_targets_timeline(Scene *scene,
                                                       const Span<Strip *> strip_sources,
                                                       const bool drag_and_drop)
{
  Editing *ed = seq::editing_ensure(scene);
  ListBaseT<Strip> *seqbase = seq::active_seqbase_get(ed);
  ListBaseT<SeqTimelineChannel> *channels = seq::channels_displayed_get(ed);
  const short snap_flag = seq::tool_settings_snap_flag_get(scene);

  /* Effects will always change position with strip to which they are connected and they don't
   * have to be selected. Remove such strips from `snap_targets` collection. */
  VectorSet effects_of_strip_sources = strip_sources;
  seq::iterator_set_expand(seqbase, effects_of_strip_sources, query_strip_effects_fn);
  effects_of_strip_sources.remove_if(
      [&](Strip *strip) { return strip->is_effect() && !strip->is_effect_with_inputs(); });

  VectorSet<Strip *> strip_targets;
  for (Strip &strip : *seqbase) {
    if (!drag_and_drop && strip.flag & SEQ_SELECT) {
      continue; /* Selected strips are being transformed, they shouldn't be a target. */
    }
    if (seq::render_is_muted(channels, &strip) && (snap_flag & SEQ_SNAP_IGNORE_MUTED)) {
      continue;
    }
    if (strip.type == STRIP_TYPE_SOUND && (snap_flag & SEQ_SNAP_IGNORE_SOUND)) {
      continue;
    }
    if (effects_of_strip_sources.contains(&strip)) {
      continue;
    }

    strip_targets.add(&strip);
  }

  return strip_targets;
}

static VectorSet<Strip *> query_strip_targets_preview(const TransInfo *t)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;

  VectorSet<Strip *> strip_targets;

  /* We don't need to calculate strip snap targets if the option is unselected. */
  if ((snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) == 0) {
    return strip_targets;
  }

  Editing *ed = seq::editing_get(scene);
  ListBaseT<SeqTimelineChannel> *channels = seq::channels_displayed_get(ed);

  strip_targets = seq::query_rendered_strips(
      scene, channels, ed->current_strips(), scene->r.cfra, 0);

  /* Selected strips are only valid targets when snapping the cursor or origin. */
  if ((t->data_type == &TransConvertType_SequencerImage) && (t->flag & T_ORIGIN) == 0) {
    strip_targets.remove_if([&](Strip *strip) { return (strip->flag & SEQ_SELECT) != 0; });
  }

  return strip_targets;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sources
 * \{ */

static int cmp_fn(const void *a, const void *b)
{
  return round_fl_to_int((*static_cast<float2 *>(const_cast<void *>(a)))[0] -
                         (*static_cast<float2 *>(const_cast<void *>(b)))[0]);
}

static void build_sources_timeline(const Scene *scene,
                                   TransSeqSnapData *snap_data,
                                   const Span<Strip *> strip_sources)
{
  for (Strip *strip : strip_sources) {
    const int channel = strip->channel;
    const int left = strip->left_handle();
    const int right = strip->right_handle(scene);

    if (strip->flag & SEQ_LEFTSEL && !(strip->flag & SEQ_RIGHTSEL)) {
      snap_data->sources.append(float2(left, channel));
    }
    else if (strip->flag & SEQ_RIGHTSEL && !(strip->flag & SEQ_LEFTSEL)) {
      snap_data->sources.append(float2(right, channel));
    }
    else {
      snap_data->sources.append(float2(left, channel));
      snap_data->sources.append(float2(right, channel));
    }
  }

  qsort(snap_data->sources.data(), snap_data->sources.size(), sizeof(float2), cmp_fn);
}

static void build_sources_timeline_retiming(const Scene *scene, TransSeqSnapData *snap_data)
{
  const Editing *ed = seq::editing_get(scene);
  const Map retiming_selection = seq::retiming_selection_get(ed);

  for (auto item : retiming_selection.items()) {
    const int channel = item.value->channel;
    const int key_frame = seq::retiming_key_frame_get(scene, item.value, item.key);
    snap_data->sources.append(float2(key_frame, channel));
  }

  qsort(snap_data->sources.data(), snap_data->sources.size(), sizeof(float2), cmp_fn);
}

static void build_sources_preview(const Scene *scene,
                                  TransSeqSnapData *snap_data,
                                  const Span<Strip *> strip_sources)
{
  for (Strip *strip : strip_sources) {
    const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);
    for (const float2 &point : strip_image_quad) {
      snap_data->sources.append(point);
    }

    /* Add origins last */
    const float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
    snap_data->sources.append(image_origin);
  }
}

static void build_sources_preview_origin(const Scene *scene,
                                         TransSeqSnapData *snap_data,
                                         const Span<Strip *> strip_sources)
{
  const size_t point_count_source = strip_sources.size();

  if (point_count_source == 0) {
    return;
  }

  for (Strip *strip : strip_sources) {
    float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
    snap_data->sources.append(image_origin);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Targets
 * \{ */

static void build_targets_timeline(const Scene *scene,
                                   const short snap_mode,
                                   TransSeqSnapData *snap_data,
                                   const Span<Strip *> strip_targets)
{
  if (snap_mode & SEQ_SNAP_TO_CURRENT_FRAME) {
    snap_data->targets.append(float2(scene->r.cfra, all_channels));
  }

  if (snap_mode & SEQ_SNAP_TO_MARKERS) {
    for (TimeMarker &marker : scene->markers) {
      snap_data->targets.append(float2(marker.frame, all_channels));
    }
  }

  if (snap_mode & SEQ_SNAP_TO_FRAME_RANGE) {
    snap_data->targets.append(float2(PSFRA, all_channels));
    snap_data->targets.append(float2(PEFRA + 1, all_channels));
    /* Also snap to meta-strip display range if we are in a meta-strip. */
    MetaStack *ms = seq::meta_stack_active_get(seq::editing_get(scene));
    if (ms != nullptr) {
      snap_data->targets.append(float2(ms->disp_range[0], all_channels));
      snap_data->targets.append(float2(ms->disp_range[1], all_channels));
    }
  }

  for (Strip *strip : strip_targets) {
    const int channel = strip->channel;
    /* Snap to left and right handles by default. */
    snap_data->targets.append(float2(strip->left_handle(), channel));
    snap_data->targets.append(float2(strip->right_handle(scene), channel));

    /* Effects and strips with a single static image do not have holds. Skip these strips. */
    if (snap_mode & SEQ_SNAP_TO_STRIP_HOLD && !strip->is_effect() &&
        !seq::transform_single_image_check(strip))
    {
      if (strip->content_start() > strip->left_handle()) {
        snap_data->targets.append(float2(strip->content_start(), channel));
      }
      if (strip->content_end(scene) < strip->right_handle(scene)) {
        snap_data->targets.append(float2(strip->content_end(scene), channel));
      }
    }

    if (snap_mode & SEQ_SNAP_TO_RETIMING) {
      for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
        const int key_frame = seq::retiming_key_frame_get(scene, strip, &key);
        /* Only add visible keyframes. */
        if (strip->intersects_frame(scene, key_frame)) {
          snap_data->targets.append(float2(key_frame, channel));
        }
      }
    }
  }

  qsort(snap_data->targets.data(), snap_data->targets.size(), sizeof(float2), cmp_fn);
}

static void build_targets_preview(const Scene *scene,
                                  const View2D *v2d,
                                  const short snap_mode,
                                  TransSeqSnapData *snap_data,
                                  const Span<Strip *> snap_targets)
{
  if (snap_mode & SEQ_SNAP_TO_PREVIEW_BORDERS) {
    snap_data->targets.append(float2(v2d->tot.xmin, v2d->tot.ymin));
    snap_data->targets.append(float2(v2d->tot.xmax, v2d->tot.ymax));
    snap_data->targets.append(float2(v2d->tot.xmin, v2d->tot.ymax));
    snap_data->targets.append(float2(v2d->tot.xmax, v2d->tot.ymin));
  }

  if (snap_mode & SEQ_SNAP_TO_PREVIEW_CENTER) {
    snap_data->targets.append(float2(0, 0));
  }

  if (snap_mode & SEQ_SNAP_TO_STRIPS_PREVIEW) {
    for (Strip *strip : snap_targets) {
      const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);
      for (const float2 &point : strip_image_quad) {
        snap_data->targets.append(point);
      }

      const float2 image_origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
      snap_data->targets.append(image_origin);
    }
  }
}

static void build_3x3_grid_points(const Scene *scene, TransSeqSnapData *snap_data, Strip *strip)
{
  const Array<float2> strip_image_quad = seq::image_transform_final_quad_get(scene, strip);

  /* Corners. */
  for (const float2 &point : strip_image_quad) {
    snap_data->targets.append(point);
  }

  /* Middle top, bottom and center of the image. */
  const float2 tm = math::interpolate(strip_image_quad[0], strip_image_quad[3], 0.5f);
  const float2 bm = math::interpolate(strip_image_quad[1], strip_image_quad[2], 0.5f);
  const float2 mm = math::interpolate(bm, tm, 0.5f);
  snap_data->targets.append(tm);
  snap_data->targets.append(mm);
  snap_data->targets.append(bm);

  /* Left and right. */
  snap_data->targets.append(math::interpolate(strip_image_quad[2], strip_image_quad[3], 0.5f));
  snap_data->targets.append(math::interpolate(strip_image_quad[0], strip_image_quad[1], 0.5f));
}

static void build_targets_preview_origin(const Scene *scene,
                                         TransSeqSnapData *snap_data,
                                         const Span<Strip *> snap_sources,
                                         const Span<Strip *> snap_targets)
{
  for (Strip *strip : snap_sources) {
    build_3x3_grid_points(scene, snap_data, strip);
  }

  for (Strip *strip : snap_targets) {
    build_3x3_grid_points(scene, snap_data, strip);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Build Snap Data
 * \{ */

static void build_snap_data_timeline(const TransInfo *t, TransSeqSnapData *snap_data)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;

  VectorSet<Strip *> strip_sources = query_strip_sources_timeline(scene);
  VectorSet<Strip *> strip_targets = query_strip_targets_timeline(scene, strip_sources, false);

  /* Build arrays of snap points. */
  if (t->data_type == &TransConvertType_Sequencer) {
    build_sources_timeline(scene, snap_data, strip_sources);
  }
  else { /* &TransConvertType_SequencerRetiming */
    build_sources_timeline_retiming(scene, snap_data);
  }
  build_targets_timeline(scene, snap_mode, snap_data, strip_targets);
}

static void build_snap_data_preview(const TransInfo *t, TransSeqSnapData *snap_data)
{
  Scene *scene = t->scene;
  short snap_mode = t->tsnap.mode;
  View2D *v2d = &t->region->v2d;
  SpaceSeq *sseq = static_cast<SpaceSeq *>(t->area->spacedata.first);

  VectorSet<Strip *> strip_sources = query_strip_sources_preview(scene);
  VectorSet<Strip *> strip_targets = query_strip_targets_preview(t);

  /* Build arrays of snap points. */
  if (t->data_type == &TransConvertType_SequencerImage) {
    if (t->flag & T_ORIGIN) {
      build_sources_preview_origin(scene, snap_data, strip_sources);
      build_targets_preview_origin(scene, snap_data, strip_sources, strip_targets);
    }
    else {
      build_sources_preview(scene, snap_data, strip_sources);
      build_targets_preview(scene, v2d, snap_mode, snap_data, strip_targets);
    }
  }
  else if (t->data_type == &TransConvertType_CursorSequencer) {
    float2 cursor_view = float2(sseq->cursor) * float2(t->aspect);
    snap_data->sources.append(cursor_view);
    build_targets_preview(scene, v2d, snap_mode, snap_data, strip_targets);
  }
}

TransSeqSnapData *snap_sequencer_data_build(const TransInfo *t)
{
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  if (ELEM(t->data_type, &TransConvertType_Sequencer, &TransConvertType_SequencerRetiming)) {
    build_snap_data_timeline(t, snap_data);
  }
  else {
    build_snap_data_preview(t, snap_data);
  }

  if (snap_data->sources.is_empty() || snap_data->targets.is_empty()) {
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
/** \name Snap Calculation
 * \{ */

static bool snap_calc_timeline(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Prevent snapping when constrained to Y axis. */
  if (t->con.mode & CON_APPLY && t->con.mode & CON_AXIS1) {
    return false;
  }

  const short snap_flag = seq::tool_settings_snap_flag_get(t->scene);
  const bool ignore_other_channels = !(snap_flag & SEQ_SNAP_TO_ALL_CHANNEL_STRIPS);

  int best_dist = MAXFRAME;
  float2 best_target(0.0f);
  float2 best_source(0.0f);

  for (const float2 source : snap_data->sources) {
    for (const float2 target : snap_data->targets) {
      if (ignore_other_channels && target[1] != all_channels &&
          (source[1] + round_fl_to_int(t->values[1])) != target[1])
      {
        continue;
      }

      int source_frame = source[0];
      int target_frame = target[0];
      int dist = abs(target_frame - (source_frame + round_fl_to_int(t->values[0])));
      if (dist > best_dist) {
        continue;
      }

      best_dist = dist;
      best_target = target;
      best_source = source;
    }
  }

  if (best_dist > snap_distance_frame_threshold_get(t)) {
    return false;
  }

  float2 best_offset(float(best_target[0] - best_source[0]), 0.0f);
  if (transform_convert_sequencer_clamp(t, best_offset)) {
    return false;
  }

  copy_v2_v2(t->tsnap.snap_target, best_target);
  copy_v2_v2(t->tsnap.snap_source, best_source);
  return true;
}

static bool snap_calc_preview_origin(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Store best snap candidates in x and y directions separately. */
  float best_dist(std::numeric_limits<float>::max());
  float2 best_target(0.0f);
  float2 best_source(0.0f);

  for (const float2 source : snap_data->sources) {
    for (const float2 target : snap_data->targets) {
      /* First update snaps in x direction, then y direction. */
      const float2 point = source + t->values;
      const float dist = math::distance(target, point);
      if (dist > best_dist) {
        continue;
      }

      best_dist = dist;
      best_target = target;
      best_source = source;
    }
  }

  if (best_dist <= snap_distance_view_threshold_get(t)) {
    copy_v2_v2(t->tsnap.snap_target, best_target);
    copy_v2_v2(t->tsnap.snap_source, best_source);
    t->tsnap.direction |= DIR_GLOBAL_X | DIR_GLOBAL_Y;
    return true;
  }
  return false;
}

static bool snap_calc_preview_image(TransInfo *t, const TransSeqSnapData *snap_data)
{
  /* Store best snap candidates in x and y directions separately. */
  float2 best_dist(std::numeric_limits<float>::max());
  float2 best_target(0.0f);
  float2 best_source(0.0f);

  for (const float2 source : snap_data->sources) {
    for (const float2 target : snap_data->targets) {
      /* First update snaps in x direction, then y direction. */
      for (int i = 0; i < 2; i++) {
        int dist = abs(target[i] - (source[i] + t->values[i]));
        if (dist > best_dist[i]) {
          continue;
        }

        best_dist[i] = dist;
        best_target[i] = target[i];
        best_source[i] = source[i];
      }
    }
  }

  t->tsnap.direction &= ~(DIR_GLOBAL_X | DIR_GLOBAL_Y);
  float thr = snap_distance_view_threshold_get(t);

  if (best_dist[0] <= thr) {
    t->tsnap.snap_target[0] = best_target[0];
    t->tsnap.snap_source[0] = best_source[0];
    t->tsnap.direction |= DIR_GLOBAL_X;
  }

  if (best_dist[1] <= thr) {
    t->tsnap.snap_target[1] = best_target[1];
    t->tsnap.snap_source[1] = best_source[1];
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drag and Drop Snapping
 * \{ */

static int snap_sequencer_calc_drag_drop_impl(TransInfo *t,
                                              const int left_frame,
                                              const int right_frame,
                                              const int channel)
{
  Scene *scene = t->scene;
  TransSeqSnapData *snap_data = MEM_new<TransSeqSnapData>(__func__);

  VectorSet<Strip *> empty_col;
  VectorSet<Strip *> strip_targets = query_strip_targets_timeline(scene, empty_col, false);

  BLI_assert(left_frame <= right_frame);

  snap_data->sources.append(float2(left_frame, channel));
  snap_data->sources.append(float2(right_frame, channel));

  /* Build arrays of snap target frames. */
  const short snap_mode = t->tsnap.mode;
  build_targets_timeline(scene, snap_mode, snap_data, strip_targets);

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

bool snap_sequencer_calc_drag_drop(Scene *scene,
                                   ARegion *region,
                                   const int left_frame,
                                   const int right_frame,
                                   const int channel,
                                   int *r_snap_distance,
                                   float2 *r_snap_point)
{
  TransInfo t = {nullptr};
  t.scene = scene;
  t.region = region;
  t.values[0] = 0;
  t.data_type = &TransConvertType_Sequencer;

  t.tsnap.mode = eSnapMode(seq::tool_settings_snap_mode_get(scene));
  t.tsnap.flag = eSnapFlag(seq::tool_settings_snap_flag_get(scene));
  *r_snap_distance = snap_sequencer_calc_drag_drop_impl(&t, left_frame, right_frame, channel);
  copy_v2_v2(*r_snap_point, t.tsnap.snap_target);
  return validSnap(&t);
}

void snap_sequencer_draw_drag_drop(Scene *scene, ARegion *region, const float2 snap_point)
{
  /* Reuse the snapping drawing code from the transform system. */
  TransInfo t = {nullptr};
  t.scene = scene;
  t.region = region;
  t.mode = TFM_SEQ_SLIDE;
  t.modifiers = MOD_SNAP;
  t.spacetype = SPACE_SEQ;
  t.tsnap.flag = SCE_SNAP;
  t.tsnap.status = (SNAP_TARGET_FOUND | SNAP_SOURCE_FOUND);
  copy_v2_v2(t.tsnap.snap_target, snap_point);

  drawSnapping(&t);
}

/** \} */

}  // namespace blender::ed::transform
