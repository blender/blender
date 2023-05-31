/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_fcurve.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "DNA_anim_types.h"
#include "DNA_sound_types.h"

#include "IMB_imbuf.h"

#include "RNA_prototypes.h"

#include "SEQ_channels.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_retiming.h"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

#include "sequencer.h"
#include "strip_time.h"
#include "utils.h"

using blender::MutableSpan;

MutableSpan<SeqRetimingHandle> SEQ_retiming_handles_get(const Sequence *seq)
{
  blender::MutableSpan<SeqRetimingHandle> handles(seq->retiming_handles, seq->retiming_handle_num);
  return handles;
}

struct SeqRetimingHandle *SEQ_retiming_last_handle_get(const struct Sequence *seq)
{
  return seq->retiming_handles + seq->retiming_handle_num - 1;
}

int SEQ_retiming_handle_index_get(const Sequence *seq, const SeqRetimingHandle *handle)
{
  return handle - seq->retiming_handles;
}

static bool seq_retiming_is_last_handle(const Sequence *seq, const SeqRetimingHandle *handle)
{
  return SEQ_retiming_handle_index_get(seq, handle) == seq->retiming_handle_num - 1;
}

const SeqRetimingHandle *SEQ_retiming_find_segment_start_handle(const Sequence *seq,
                                                                const int frame_index)
{
  const SeqRetimingHandle *start_handle = nullptr;
  for (auto const &handle : SEQ_retiming_handles_get(seq)) {
    if (seq_retiming_is_last_handle(seq, &handle)) {
      break;
    }
    if (handle.strip_frame_index > frame_index) {
      break;
    }

    start_handle = &handle;
  }

  return start_handle;
}

int SEQ_retiming_handles_count(const Sequence *seq)
{
  return seq->retiming_handle_num;
}

void SEQ_retiming_data_ensure(Sequence *seq)
{
  if (!SEQ_retiming_is_allowed(seq)) {
    return;
  }

  if (seq->retiming_handles != nullptr) {
    return;
  }

  seq->retiming_handles = (SeqRetimingHandle *)MEM_calloc_arrayN(
      2, sizeof(SeqRetimingHandle), __func__);
  SeqRetimingHandle *handle = seq->retiming_handles + 1;
  handle->strip_frame_index = seq->len;
  handle->retiming_factor = 1.0f;
  seq->retiming_handle_num = 2;
}

void SEQ_retiming_data_clear(Sequence *seq)
{
  seq->retiming_handles = nullptr;
  seq->retiming_handle_num = 0;
}

bool SEQ_retiming_is_active(const Sequence *seq)
{
  return seq->retiming_handle_num > 1;
}

bool SEQ_retiming_is_allowed(const Sequence *seq)
{
  return ELEM(seq->type,
              SEQ_TYPE_SOUND_RAM,
              SEQ_TYPE_IMAGE,
              SEQ_TYPE_META,
              SEQ_TYPE_SCENE,
              SEQ_TYPE_MOVIE,
              SEQ_TYPE_MOVIECLIP,
              SEQ_TYPE_MASK);
}

static int seq_retiming_segment_length_get(const SeqRetimingHandle *start_handle)
{
  const SeqRetimingHandle *end_handle = start_handle + 1;
  return end_handle->strip_frame_index - start_handle->strip_frame_index;
}

static float seq_retiming_segment_step_get(const SeqRetimingHandle *start_handle)
{
  const SeqRetimingHandle *end_handle = start_handle + 1;
  const int segment_length = seq_retiming_segment_length_get(start_handle);
  const float segment_fac_diff = end_handle->retiming_factor - start_handle->retiming_factor;
  return segment_fac_diff / segment_length;
}

static void seq_retiming_segment_as_line_segment(const SeqRetimingHandle *start_handle,
                                                 double r_v1[2],
                                                 double r_v2[2])
{
  const SeqRetimingHandle *end_handle = start_handle + 1;
  r_v1[0] = start_handle->strip_frame_index;
  r_v1[1] = start_handle->retiming_factor;
  r_v2[0] = end_handle->strip_frame_index;
  r_v2[1] = end_handle->retiming_factor;
}

static void seq_retiming_line_segments_tangent_circle(const SeqRetimingHandle *start_handle,
                                                      double r_center[2],
                                                      double *radius)
{
  double s1_1[2], s1_2[2], s2_1[2], s2_2[2], p1_2[2];

  /* Get 2 segments. */
  seq_retiming_segment_as_line_segment(start_handle - 1, s1_1, s1_2);
  seq_retiming_segment_as_line_segment(start_handle + 1, s2_1, s2_2);
  /* Backup first segment end point - needed to calculate arc radius. */
  copy_v2_v2_db(p1_2, s1_2);
  /* Convert segments to vectors. */
  double v1[2], v2[2];
  sub_v2_v2v2_db(v1, s1_1, s1_2);
  sub_v2_v2v2_db(v2, s2_1, s2_2);
  /* Rotate segments by 90 degrees around seg. 1 end and seg. 2 start point. */
  SWAP(double, v1[0], v1[1]);
  SWAP(double, v2[0], v2[1]);
  v1[0] *= -1;
  v2[0] *= -1;
  copy_v2_v2_db(s1_1, s1_2);
  add_v2_v2_db(s1_2, v1);
  copy_v2_v2_db(s2_2, s2_1);
  add_v2_v2_db(s2_2, v2);
  /* Get center and radius of arc segment between 2 linear segments. */
  double lambda, mu;
  isect_seg_seg_v2_lambda_mu_db(s1_1, s1_2, s2_1, s2_2, &lambda, &mu);
  r_center[0] = s1_1[0] + lambda * (s1_2[0] - s1_1[0]);
  r_center[1] = s1_1[1] + lambda * (s1_2[1] - s1_1[1]);
  *radius = len_v2v2_db(p1_2, r_center);
}

bool SEQ_retiming_handle_is_transition_type(const SeqRetimingHandle *handle)
{
  return (handle->flag & SPEED_TRANSITION) != 0;
}

/* Check colinearity of 2 segments allowing for some imprecision.
 * `isect_seg_seg_v2_lambda_mu_db()` return value does not work well in this case. */

static bool seq_retiming_transition_is_linear(const Sequence *seq, const SeqRetimingHandle *handle)
{
  const float prev_speed = SEQ_retiming_handle_speed_get(seq, handle - 1);
  const float next_speed = SEQ_retiming_handle_speed_get(seq, handle + 1);

  return abs(prev_speed - next_speed) < 0.01f;
}

static float seq_retiming_evaluate_arc_segment(const SeqRetimingHandle *handle,
                                               const float frame_index)
{
  double c[2], r;
  seq_retiming_line_segments_tangent_circle(handle, c, &r);
  const int side = c[1] > handle->retiming_factor ? -1 : 1;
  const float y = c[1] + side * sqrt(pow(r, 2) - pow((frame_index - c[0]), 2));
  return y;
}

float seq_retiming_evaluate(const Sequence *seq, const float frame_index)
{
  const SeqRetimingHandle *start_handle = SEQ_retiming_find_segment_start_handle(seq, frame_index);

  const int start_handle_index = start_handle - seq->retiming_handles;
  BLI_assert(start_handle_index < seq->retiming_handle_num);

  const float segment_frame_index = frame_index - start_handle->strip_frame_index;

  if (!SEQ_retiming_handle_is_transition_type(start_handle)) {
    const float segment_step = seq_retiming_segment_step_get(start_handle);
    return start_handle->retiming_factor + segment_step * segment_frame_index;
  }

  if (seq_retiming_transition_is_linear(seq, start_handle)) {
    const float segment_step = seq_retiming_segment_step_get(start_handle - 1);
    return start_handle->retiming_factor + segment_step * segment_frame_index;
  }

  /* Sanity check for transition type. */
  BLI_assert(start_handle_index > 0);
  BLI_assert(start_handle_index < seq->retiming_handle_num - 1);
  UNUSED_VARS_NDEBUG(start_handle_index);

  return seq_retiming_evaluate_arc_segment(start_handle, frame_index);
}

SeqRetimingHandle *SEQ_retiming_add_handle(const Scene *scene,
                                           Sequence *seq,
                                           const int timeline_frame)
{
  float frame_index = (timeline_frame - SEQ_time_start_frame_get(seq)) *
                      seq_time_media_playback_rate_factor_get(scene, seq);
  float value = seq_retiming_evaluate(seq, frame_index);

  const SeqRetimingHandle *start_handle = SEQ_retiming_find_segment_start_handle(seq, frame_index);
  if (start_handle->strip_frame_index == frame_index) {
    return nullptr; /* Retiming handle already exists. */
  }

  if (SEQ_retiming_handle_is_transition_type(start_handle)) {
    return nullptr;
  }

  SeqRetimingHandle *handles = seq->retiming_handles;
  size_t handle_count = SEQ_retiming_handles_count(seq);
  const int new_handle_index = start_handle - handles + 1;
  BLI_assert(new_handle_index >= 0);
  BLI_assert(new_handle_index < handle_count);

  SeqRetimingHandle *new_handles = (SeqRetimingHandle *)MEM_callocN(
      (handle_count + 1) * sizeof(SeqRetimingHandle), __func__);
  if (new_handle_index > 0) {
    memcpy(new_handles, handles, new_handle_index * sizeof(SeqRetimingHandle));
  }
  if (new_handle_index < handle_count) {
    memcpy(new_handles + new_handle_index + 1,
           handles + new_handle_index,
           (handle_count - new_handle_index) * sizeof(SeqRetimingHandle));
  }
  MEM_freeN(handles);
  seq->retiming_handles = new_handles;
  seq->retiming_handle_num++;

  SeqRetimingHandle *added_handle = (new_handles + new_handle_index);
  added_handle->strip_frame_index = frame_index;
  added_handle->retiming_factor = value;

  return added_handle;
}

static void seq_retiming_offset_linear_handle(const Scene *scene,
                                              Sequence *seq,
                                              SeqRetimingHandle *handle,
                                              const int offset)
{
  MutableSpan handles = SEQ_retiming_handles_get(seq);

  for (SeqRetimingHandle *next_handle = handle; next_handle < handles.end(); next_handle++) {
    next_handle->strip_frame_index += offset * seq_time_media_playback_rate_factor_get(scene, seq);
  }

  /* Handle affected transitions: remove and re-create transition. This way transition won't change
   * length.
   * Alternative solution is to find where in arc segment the `y` value is closest to `handle`
   * retiming factor, then trim transition to that point. This would change transition length. */
  if (SEQ_retiming_handle_is_transition_type(handle - 2)) {
    SeqRetimingHandle *transition_handle = handle - 2;

    const int transition_offset = transition_handle->strip_frame_index -
                                  transition_handle->original_strip_frame_index;

    const int transition_handle_index = SEQ_retiming_handle_index_get(seq, transition_handle);

    SEQ_retiming_remove_handle(scene, seq, transition_handle);
    SeqRetimingHandle *orig_handle = seq->retiming_handles + transition_handle_index;
    SEQ_retiming_add_transition(scene, seq, orig_handle, -transition_offset);
  }

  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}

static void seq_retiming_offset_transition_handle(const Scene *scene,
                                                  const Sequence *seq,
                                                  SeqRetimingHandle *handle,
                                                  const int offset)
{
  SeqRetimingHandle *handle_start, *handle_end;
  int corrected_offset;

  if (SEQ_retiming_handle_is_transition_type(handle)) {
    handle_start = handle;
    handle_end = handle + 1;
    corrected_offset = offset;
  }
  else {
    handle_start = handle - 1;
    handle_end = handle;
    corrected_offset = -1 * offset;
  }

  /* Prevent transition handles crossing each other. */
  const float start_frame = SEQ_retiming_handle_timeline_frame_get(scene, seq, handle_start);
  const float end_frame = SEQ_retiming_handle_timeline_frame_get(scene, seq, handle_end);
  int xmax = ((start_frame + end_frame) / 2) - 1;
  int max_offset = xmax - start_frame;
  corrected_offset = min_ii(corrected_offset, max_offset);
  /* Prevent mirrored movement crossing any handle. */
  SeqRetimingHandle *prev_segment_end = handle_start - 1, *next_segment_start = handle_end + 1;
  const int offset_min_left = SEQ_retiming_handle_timeline_frame_get(
                                  scene, seq, prev_segment_end) +
                              1 - start_frame;
  const int offset_min_right =
      end_frame - SEQ_retiming_handle_timeline_frame_get(scene, seq, next_segment_start) - 1;
  corrected_offset = max_iii(corrected_offset, offset_min_left, offset_min_right);

  const float prev_segment_step = seq_retiming_segment_step_get(handle_start - 1);
  const float next_segment_step = seq_retiming_segment_step_get(handle_end);

  handle_start->strip_frame_index += corrected_offset;
  handle_start->retiming_factor += corrected_offset * prev_segment_step;
  handle_end->strip_frame_index -= corrected_offset;
  handle_end->retiming_factor -= corrected_offset * next_segment_step;
}

void SEQ_retiming_offset_handle(const Scene *scene,
                                Sequence *seq,
                                SeqRetimingHandle *handle,
                                const int offset)
{
  if (handle->strip_frame_index == 0) {
    return; /* First handle can not be moved. */
  }

  SeqRetimingHandle *prev_handle = handle - 1;
  SeqRetimingHandle *next_handle = handle + 1;

  /* Limit retiming handle movement. */
  int corrected_offset = offset;
  int handle_frame = SEQ_retiming_handle_timeline_frame_get(scene, seq, handle);
  int offset_min = SEQ_retiming_handle_timeline_frame_get(scene, seq, prev_handle) + 1 -
                   handle_frame;
  int offset_max;
  if (SEQ_retiming_handle_index_get(seq, handle) == seq->retiming_handle_num - 1) {
    offset_max = INT_MAX;
  }
  else {
    offset_max = SEQ_retiming_handle_timeline_frame_get(scene, seq, next_handle) - 1 -
                 handle_frame;
  }
  corrected_offset = max_ii(corrected_offset, offset_min);
  corrected_offset = min_ii(corrected_offset, offset_max);

  if (SEQ_retiming_handle_is_transition_type(handle) ||
      SEQ_retiming_handle_is_transition_type(prev_handle))
  {
    seq_retiming_offset_transition_handle(scene, seq, handle, corrected_offset);
  }
  else {
    seq_retiming_offset_linear_handle(scene, seq, handle, corrected_offset);
  }
}

static void seq_retiming_remove_handle_ex(Sequence *seq, SeqRetimingHandle *handle)
{
  SeqRetimingHandle *last_handle = SEQ_retiming_last_handle_get(seq);
  if (handle->strip_frame_index == 0 || handle == last_handle) {
    return; /* First and last handle can not be removed. */
  }

  size_t handle_count = SEQ_retiming_handles_count(seq);
  SeqRetimingHandle *handles = (SeqRetimingHandle *)MEM_callocN(
      (handle_count - 1) * sizeof(SeqRetimingHandle), __func__);

  const int handle_index = handle - seq->retiming_handles;
  memcpy(handles, seq->retiming_handles, (handle_index) * sizeof(SeqRetimingHandle));
  memcpy(handles + handle_index,
         seq->retiming_handles + handle_index + 1,
         (handle_count - handle_index - 1) * sizeof(SeqRetimingHandle));
  MEM_freeN(seq->retiming_handles);
  seq->retiming_handles = handles;
  seq->retiming_handle_num--;
}

/* This function removes transition segment and creates retiming handle where it originally was. */
static void seq_retiming_remove_transition(const Scene *scene,
                                           Sequence *seq,
                                           SeqRetimingHandle *handle)
{
  const int orig_frame_index = handle->original_strip_frame_index;
  const float orig_retiming_factor = handle->original_retiming_factor;

  /* Remove both handles defining transition. */
  int handle_index = SEQ_retiming_handle_index_get(seq, handle);
  seq_retiming_remove_handle_ex(seq, handle);
  seq_retiming_remove_handle_ex(seq, seq->retiming_handles + handle_index);

  /* Create original linear handle. */
  SeqRetimingHandle *orig_handle = SEQ_retiming_add_handle(
      scene, seq, SEQ_time_start_frame_get(seq) + orig_frame_index);
  orig_handle->retiming_factor = orig_retiming_factor;
}

void SEQ_retiming_remove_handle(const Scene *scene, Sequence *seq, SeqRetimingHandle *handle)
{
  if (SEQ_retiming_handle_is_transition_type(handle)) {
    seq_retiming_remove_transition(scene, seq, handle);
    return;
  }
  SeqRetimingHandle *previous_handle = handle - 1;
  if (SEQ_retiming_handle_is_transition_type(previous_handle)) {
    seq_retiming_remove_transition(scene, seq, previous_handle);
    return;
  }
  seq_retiming_remove_handle_ex(seq, handle);
}

SeqRetimingHandle *SEQ_retiming_add_transition(const Scene *scene,
                                               Sequence *seq,
                                               SeqRetimingHandle *handle,
                                               const int offset)
{
  if (SEQ_retiming_handle_is_transition_type(handle) ||
      SEQ_retiming_handle_is_transition_type(handle - 1))
  {
    return nullptr;
  }

  const int orig_handle_index = SEQ_retiming_handle_index_get(seq, handle);
  const int orig_timeline_frame = SEQ_retiming_handle_timeline_frame_get(scene, seq, handle);
  const int orig_frame_index = handle->strip_frame_index;
  const float orig_retiming_factor = handle->retiming_factor;
  SEQ_retiming_add_handle(scene, seq, orig_timeline_frame + offset);
  SeqRetimingHandle *transition_handle = SEQ_retiming_add_handle(
      scene, seq, orig_timeline_frame - offset);
  transition_handle->flag |= SPEED_TRANSITION;
  transition_handle->original_strip_frame_index = orig_frame_index;
  transition_handle->original_retiming_factor = orig_retiming_factor;
  seq_retiming_remove_handle_ex(seq, seq->retiming_handles + orig_handle_index + 1);
  return seq->retiming_handles + orig_handle_index;
}

float SEQ_retiming_handle_speed_get(const Sequence *seq, const SeqRetimingHandle *handle)
{
  if (handle->strip_frame_index == 0) {
    return 1.0f;
  }

  const SeqRetimingHandle *handle_prev = handle - 1;

  const int frame_index_max = seq->len - 1;
  const int frame_retimed_prev = round_fl_to_int(handle_prev->retiming_factor * frame_index_max);
  const int frame_index_prev = handle_prev->strip_frame_index;
  const int frame_retimed = round_fl_to_int(handle->retiming_factor * frame_index_max);
  const int frame_index = handle->strip_frame_index;

  const int fragment_length_retimed = frame_retimed - frame_retimed_prev;
  const int fragment_length_original = frame_index - frame_index_prev;

  const float speed = float(fragment_length_retimed) / float(fragment_length_original);
  return speed;
}

enum eRangeType {
  LINEAR = 0,
  TRANSITION = 1,
};

enum eIntersectType {
  FULL,
  PARTIAL_START,
  PARTIAL_END,
  INSIDE,
  NONE,
};

class RetimingRange {
 public:
  int start, end;
  float speed;
  blender::Vector<float> speed_table;

  eRangeType type;
  RetimingRange(const Sequence *seq, int start_frame, int end_frame, float speed, eRangeType type)
      : start(start_frame), end(end_frame), speed(speed), type(type)
  {
    if (type == TRANSITION) {
      speed = 1.0f;
      claculate_speed_table_from_seq(seq);
    }
  }

  RetimingRange(int start_frame, int end_frame, float speed, eRangeType type)
      : start(start_frame), end(end_frame), speed(speed), type(type)
  {
  }

  RetimingRange duplicate()
  {
    RetimingRange new_range = RetimingRange(start, end, speed, type);
    for (int i = 0; i < speed_table.size(); i++) {
      new_range.speed_table.append(speed_table[i]);
    }
    return new_range;
  }

  /* Create new range representing overlap of 2 ranges.
   * Returns overlapping range. */
  RetimingRange operator*(const RetimingRange rhs_range)
  {
    RetimingRange new_range = RetimingRange(0, 0, 0, LINEAR);

    /* Offsets to merge speed tables. */
    int range_offset = 0, rhs_range_offset = 0;
    if (intersect_type(rhs_range) == FULL) {
      new_range.start = start;
      new_range.end = end;
      rhs_range_offset = start - rhs_range.start;
    }
    else if (intersect_type(rhs_range) == PARTIAL_START) {
      new_range.start = start;
      new_range.end = rhs_range.end;
      rhs_range_offset = start - rhs_range.start;
    }
    else if (intersect_type(rhs_range) == PARTIAL_END) {
      new_range.start = rhs_range.start;
      new_range.end = end;
      range_offset = rhs_range.start - start;
    }
    else if (intersect_type(rhs_range) == INSIDE) {
      new_range.start = rhs_range.start;
      new_range.end = rhs_range.end;
      range_offset = rhs_range.start - start;
    }

    if (type != TRANSITION && rhs_range.type != TRANSITION) {
      new_range.speed = speed * rhs_range.speed;
      return new_range;
    }

    /* One of ranges is transition type, so speed tables has to be copied. */
    new_range.type = TRANSITION;
    new_range.speed = 1.0f;
    const int new_range_len = new_range.end - new_range.start;

    if (type == TRANSITION && rhs_range.type == TRANSITION) {
      for (int i = 0; i < new_range_len; i++) {
        const float range_speed = speed_table[i + range_offset];
        const float rhs_range_speed = rhs_range.speed_table[i + rhs_range_offset];
        new_range.speed_table.append(range_speed * rhs_range_speed);
      }
    }
    else if (type == TRANSITION) {
      for (int i = 0; i < new_range_len; i++) {
        const float range_speed = speed_table[i + range_offset];
        new_range.speed_table.append(range_speed * rhs_range.speed);
      }
    }
    else if (rhs_range.type == TRANSITION) {
      for (int i = 0; i < new_range_len; i++) {
        const float rhs_range_speed = rhs_range.speed_table[i + rhs_range_offset];
        new_range.speed_table.append(speed * rhs_range_speed);
      }
    }

    return new_range;
  }

  void claculate_speed_table_from_seq(const Sequence *seq)
  {
    for (int frame = start; frame <= end; frame++) {
      /* We need number actual number of frames here. */
      const double normal_step = 1 / double(seq->len);

      /* Who needs calculus, when you can have slow code? */
      const double val_prev = seq_retiming_evaluate(seq, frame - 1);
      const double val = seq_retiming_evaluate(seq, frame);
      const double speed_at_frame = (val - val_prev) / normal_step;
      speed_table.append(speed_at_frame);
    }
  }

  const eIntersectType intersect_type(const RetimingRange &other) const
  {
    if (other.start <= start && other.end >= end) {
      return FULL;
    }
    if (other.start > start && other.start < end && other.end > start && other.end < end) {
      return INSIDE;
    }
    if (other.start > start && other.start < end) {
      return PARTIAL_END;
    }
    if (other.end > start && other.end < end) {
      return PARTIAL_START;
    }
    return NONE;
  }
};

class RetimingRangeData {
 public:
  blender::Vector<RetimingRange> ranges;
  RetimingRangeData(const Sequence *seq)
  {
    MutableSpan handles = SEQ_retiming_handles_get(seq);
    for (const SeqRetimingHandle &handle : handles) {
      if (handle.strip_frame_index == 0) {
        continue;
      }
      const SeqRetimingHandle *handle_prev = &handle - 1;
      float speed = SEQ_retiming_handle_speed_get(seq, &handle);
      int frame_start = SEQ_time_start_frame_get(seq) + handle_prev->strip_frame_index;
      int frame_end = SEQ_time_start_frame_get(seq) + handle.strip_frame_index;

      eRangeType type = SEQ_retiming_handle_is_transition_type(handle_prev) ? TRANSITION : LINEAR;
      RetimingRange range = RetimingRange(seq, frame_start, frame_end, speed, type);
      ranges.append(range);
    }
  }

  RetimingRangeData &operator*=(const RetimingRangeData rhs)
  {
    if (ranges.is_empty()) {
      for (const RetimingRange &rhs_range : rhs.ranges) {
        RetimingRange range = RetimingRange(
            rhs_range.start, rhs_range.end, rhs_range.speed, rhs_range.type);
        ranges.append(range);
      }
      return *this;
    }

    for (int i = 0; i < ranges.size(); i++) {
      RetimingRange &range = ranges[i];
      for (const RetimingRange &rhs_range : rhs.ranges) {
        if (range.intersect_type(rhs_range) == NONE) {
          continue;
        }
        else if (range.intersect_type(rhs_range) == FULL) {
          RetimingRange isect = range * rhs_range;
          ranges.remove(i);
          ranges.insert(i, isect);
        }
        if (range.intersect_type(rhs_range) == PARTIAL_START) {
          ranges.insert(i, range * rhs_range);
          ranges.insert(i, range * rhs_range);
          range.start = rhs_range.end + 1;
        }
        else if (range.intersect_type(rhs_range) == PARTIAL_END) {
          ranges.insert(i, range * rhs_range);
          range.end = rhs_range.start;
        }
        else if (range.intersect_type(rhs_range) == INSIDE) {
          RetimingRange left_range = range.duplicate();
          left_range.end = rhs_range.start;
          range.start = rhs_range.end + 1;

          ranges.insert(i, left_range);
          ranges.insert(i, range * rhs_range);
        }
      }
    }
    return *this;
  }
};

static RetimingRangeData seq_retiming_range_data_get(const Scene *scene, const Sequence *seq)
{
  RetimingRangeData strip_retiming_data = RetimingRangeData(seq);

  const Sequence *meta_parent = seq_sequence_lookup_meta_by_seq(scene, seq);
  if (meta_parent == nullptr) {
    return strip_retiming_data;
  }

  RetimingRangeData meta_retiming_data = RetimingRangeData(meta_parent);
  strip_retiming_data *= meta_retiming_data;
  return strip_retiming_data;
}

void SEQ_retiming_sound_animation_data_set(const Scene *scene, const Sequence *seq)
{
  RetimingRangeData retiming_data = seq_retiming_range_data_get(scene, seq);
  for (int i = 0; i < retiming_data.ranges.size(); i++) {
    RetimingRange range = retiming_data.ranges[i];
    if (range.type == TRANSITION) {

      const int range_length = range.end - range.start;
      for (int i = 0; i <= range_length; i++) {
        const int frame = range.start + i;
        BKE_sound_set_scene_sound_pitch_at_frame(
            seq->scene_sound, frame, range.speed_table[i], true);
      }
    }
    else {
      BKE_sound_set_scene_sound_pitch_constant_range(
          seq->scene_sound, range.start, range.end, range.speed);
    }
  }
}

float SEQ_retiming_handle_timeline_frame_get(const Scene *scene,
                                             const Sequence *seq,
                                             const SeqRetimingHandle *handle)
{
  return SEQ_time_start_frame_get(seq) +
         handle->strip_frame_index / seq_time_media_playback_rate_factor_get(scene, seq);
}
