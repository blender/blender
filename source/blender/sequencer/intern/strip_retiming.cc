/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_sound.h"

#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "sequencer.hh"
#include "strip_time.hh"

using blender::MutableSpan;

MutableSpan<SeqRetimingKey> SEQ_retiming_keys_get(const Strip *strip)
{
  blender::MutableSpan<SeqRetimingKey> handles(strip->retiming_keys, strip->retiming_keys_num);
  return handles;
}

bool SEQ_retiming_is_last_key(const Strip *strip, const SeqRetimingKey *key)
{
  return SEQ_retiming_key_index_get(strip, key) == strip->retiming_keys_num - 1;
}

SeqRetimingKey *SEQ_retiming_last_key_get(const Strip *strip)
{
  return strip->retiming_keys + strip->retiming_keys_num - 1;
}

int SEQ_retiming_key_index_get(const Strip *strip, const SeqRetimingKey *key)
{
  return key - strip->retiming_keys;
}

static int content_frame_index_get(const Scene *scene,
                                   const Strip *strip,
                                   const int timeline_frame)
{
  const int sound_offset = SEQ_time_get_rounded_sound_offset(scene, strip);
  return (timeline_frame - SEQ_time_start_frame_get(strip) - sound_offset) *
         SEQ_time_media_playback_rate_factor_get(scene, strip);
}

SeqRetimingKey *SEQ_retiming_key_get_by_timeline_frame(const Scene *scene,
                                                       const Strip *strip,
                                                       const int timeline_frame)
{
  for (auto &key : SEQ_retiming_keys_get(strip)) {
    const int key_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, &key);
    if (key_timeline_frame == timeline_frame) {
      return &key;
    }
  }

  return nullptr;
}

SeqRetimingKey *SEQ_retiming_find_segment_start_key(const Strip *strip, float frame_index)
{
  SeqRetimingKey *start_key = nullptr;
  for (auto &key : SEQ_retiming_keys_get(strip)) {
    if (SEQ_retiming_is_last_key(strip, &key)) {
      break;
    }
    if (key.strip_frame_index > frame_index) {
      break;
    }

    start_key = &key;
  }

  return start_key;
}

int SEQ_retiming_keys_count(const Strip *strip)
{
  return strip->retiming_keys_num;
}

void SEQ_retiming_data_ensure(Strip *strip)
{
  if (!SEQ_retiming_is_allowed(strip)) {
    return;
  }

  if (SEQ_retiming_is_active(strip)) {
    return;
  }

  strip->retiming_keys = (SeqRetimingKey *)MEM_calloc_arrayN(2, sizeof(SeqRetimingKey), __func__);
  SeqRetimingKey *key = strip->retiming_keys + 1;
  key->strip_frame_index = strip->len - 1;
  key->retiming_factor = 1.0f;
  strip->retiming_keys_num = 2;
}

void SEQ_retiming_data_clear(Strip *strip)
{
  if (strip->retiming_keys != nullptr) {
    MEM_freeN(strip->retiming_keys);
    strip->retiming_keys = nullptr;
    strip->retiming_keys_num = 0;
  }
  strip->flag &= ~SEQ_SHOW_RETIMING;
}

static void retiming_key_overlap(Scene *scene, Strip *strip)
{
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  blender::VectorSet<Strip *> strips;
  blender::VectorSet<Strip *> dependant;
  dependant.add(strip);
  SEQ_iterator_set_expand(scene, seqbase, dependant, SEQ_query_strip_effect_chain);
  strips.add_multiple(dependant);
  dependant.remove(strip);
  SEQ_transform_handle_overlap(scene, seqbase, strips, dependant, true);
}

void SEQ_retiming_reset(Scene *scene, Strip *strip)
{
  if (!SEQ_retiming_is_allowed(strip)) {
    return;
  }

  SEQ_retiming_data_clear(strip);

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));

  retiming_key_overlap(scene, strip);
}

bool SEQ_retiming_is_active(const Strip *strip)
{
  return strip->retiming_keys_num > 1;
}

bool SEQ_retiming_data_is_editable(const Strip *strip)
{
  return strip->flag & SEQ_SHOW_RETIMING;
}

bool SEQ_retiming_is_allowed(const Strip *strip)
{
  if (strip->len < 2) {
    return false;
  }

  return ELEM(strip->type,
              STRIP_TYPE_SOUND_RAM,
              STRIP_TYPE_IMAGE,
              STRIP_TYPE_META,
              STRIP_TYPE_SCENE,
              STRIP_TYPE_MOVIE,
              STRIP_TYPE_MOVIECLIP,
              STRIP_TYPE_MASK);
}

static double strip_retiming_segment_length_get(const SeqRetimingKey *start_key)
{
  const SeqRetimingKey *end_key = start_key + 1;
  return end_key->strip_frame_index - start_key->strip_frame_index;
}

/* Segment step can be a very small number, which is multiplied later. Therefore double is used to
 * avoid loss of precision. */
static double strip_retiming_segment_step_get(const SeqRetimingKey *start_key)
{
  const SeqRetimingKey *end_key = start_key + 1;
  const double segment_length = strip_retiming_segment_length_get(start_key);
  const double segment_fac_diff = end_key->retiming_factor - start_key->retiming_factor;
  return segment_fac_diff / segment_length;
}

static void strip_retiming_segment_as_line_segment(const SeqRetimingKey *start_key,
                                                   double r_v1[2],
                                                   double r_v2[2])
{
  const SeqRetimingKey *end_key = start_key + 1;
  r_v1[0] = start_key->strip_frame_index;
  r_v1[1] = start_key->retiming_factor;
  r_v2[0] = end_key->strip_frame_index;
  r_v2[1] = end_key->retiming_factor;
}

static void strip_retiming_line_segments_tangent_circle(const SeqRetimingKey *start_key,
                                                        double r_center[2],
                                                        double *radius)
{
  blender::double2 s1_1, s1_2, s2_1, s2_2, p1_2;

  /* Get 2 segments. */
  strip_retiming_segment_as_line_segment(start_key - 1, s1_1, s1_2);
  strip_retiming_segment_as_line_segment(start_key + 1, s2_1, s2_2);
  /* Backup first segment end point - needed to calculate arc radius. */
  copy_v2_v2_db(p1_2, s1_2);
  /* Convert segments to vectors. */
  blender::double2 v1, v2;
  sub_v2_v2v2_db(v1, s1_1, s1_2);
  sub_v2_v2v2_db(v2, s2_1, s2_2);
  /* Rotate segments by 90 degrees around seg. 1 end and seg. 2 start point. */
  std::swap(v1[0], v1[1]);
  std::swap(v2[0], v2[1]);
  v1[0] *= -1;
  v2[0] *= -1;
  copy_v2_v2_db(s1_1, s1_2);
  s1_2 += v1;
  copy_v2_v2_db(s2_2, s2_1);
  s2_2 += v2;
  /* Get center and radius of arc segment between 2 linear segments. */
  double lambda, mu;
  isect_seg_seg_v2_lambda_mu_db(s1_1, s1_2, s2_1, s2_2, &lambda, &mu);
  r_center[0] = s1_1[0] + lambda * (s1_2[0] - s1_1[0]);
  r_center[1] = s1_1[1] + lambda * (s1_2[1] - s1_1[1]);
  *radius = len_v2v2_db(p1_2, r_center);
}

bool SEQ_retiming_key_is_transition_type(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_SPEED_TRANSITION_IN) != 0 || (key->flag & SEQ_SPEED_TRANSITION_OUT) != 0;
}

bool SEQ_retiming_key_is_transition_start(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_SPEED_TRANSITION_IN) != 0;
}

SeqRetimingKey *SEQ_retiming_transition_start_get(SeqRetimingKey *key)
{
  if (key->flag & SEQ_SPEED_TRANSITION_OUT) {
    return key - 1;
  }
  if (key->flag & SEQ_SPEED_TRANSITION_IN) {
    return key;
  }
  return nullptr;
}

bool SEQ_retiming_key_is_freeze_frame(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_FREEZE_FRAME_IN) != 0 || (key->flag & SEQ_FREEZE_FRAME_OUT) != 0;
}

/* Check colinearity of 2 segments allowing for some imprecision.
 * `isect_seg_seg_v2_lambda_mu_db()` return value does not work well in this case. */

static bool strip_retiming_transition_is_linear(const Strip *strip, const SeqRetimingKey *key)
{
  const float prev_speed = SEQ_retiming_key_speed_get(strip, key - 1);
  const float next_speed = SEQ_retiming_key_speed_get(strip, key + 2);

  return abs(prev_speed - next_speed) < 0.01f;
}

static float strip_retiming_evaluate_arc_segment(const SeqRetimingKey *key,
                                                 const float frame_index)
{
  double c[2], r;
  strip_retiming_line_segments_tangent_circle(key, c, &r);
  const int side = c[1] > key->retiming_factor ? -1 : 1;
  const float y = c[1] + side * sqrt(pow(r, 2) - pow((frame_index - c[0]), 2));
  return y;
}

float strip_retiming_evaluate(const Strip *strip, const float frame_index)
{
  const SeqRetimingKey *start_key = SEQ_retiming_find_segment_start_key(strip, frame_index);

  const int start_key_index = start_key - strip->retiming_keys;
  BLI_assert(start_key_index < strip->retiming_keys_num);

  const float segment_frame_index = frame_index - start_key->strip_frame_index;

  if (!SEQ_retiming_key_is_transition_start(start_key)) {
    const double segment_step = strip_retiming_segment_step_get(start_key);
    return std::min(1.0f, start_key->retiming_factor + float(segment_step * segment_frame_index));
  }

  if (strip_retiming_transition_is_linear(strip, start_key)) {
    const double segment_step = strip_retiming_segment_step_get(start_key - 1);
    return std::min(1.0f, start_key->retiming_factor + float(segment_step * segment_frame_index));
  }

  /* Sanity check for transition type. */
  BLI_assert(start_key_index > 0);
  BLI_assert(start_key_index < strip->retiming_keys_num - 1);
  UNUSED_VARS_NDEBUG(start_key_index);

  return std::min(1.0f, strip_retiming_evaluate_arc_segment(start_key, frame_index));
}

static SeqRetimingKey *strip_retiming_add_key(Strip *strip, float frame_index)
{
  if (!SEQ_retiming_is_allowed(strip)) {
    return nullptr;
  }
  /* Clamp timeline frame to strip content range. */
  if (frame_index <= 0) {
    return &strip->retiming_keys[0];
  }
  if (frame_index >= SEQ_retiming_last_key_get(strip)->strip_frame_index) {
    return SEQ_retiming_last_key_get(strip); /* This is expected for strips with no offsets. */
  }

  SeqRetimingKey *start_key = SEQ_retiming_find_segment_start_key(strip, frame_index);

  if (start_key->strip_frame_index == frame_index) {
    return start_key; /* Retiming key already exists. */
  }

  if ((start_key->flag & SEQ_SPEED_TRANSITION_IN) != 0 ||
      (start_key->flag & SEQ_FREEZE_FRAME_IN) != 0)
  {
    return nullptr;
  }

  float value = strip_retiming_evaluate(strip, frame_index);

  SeqRetimingKey *keys = strip->retiming_keys;
  size_t keys_count = SEQ_retiming_keys_count(strip);
  const int new_key_index = start_key - keys + 1;
  BLI_assert(new_key_index >= 0);
  BLI_assert(new_key_index < keys_count);

  SeqRetimingKey *new_keys = (SeqRetimingKey *)MEM_callocN(
      (keys_count + 1) * sizeof(SeqRetimingKey), __func__);
  if (new_key_index > 0) {
    memcpy(new_keys, keys, new_key_index * sizeof(SeqRetimingKey));
  }
  if (new_key_index < keys_count) {
    memcpy(new_keys + new_key_index + 1,
           keys + new_key_index,
           (keys_count - new_key_index) * sizeof(SeqRetimingKey));
  }
  MEM_freeN(keys);
  strip->retiming_keys = new_keys;
  strip->retiming_keys_num++;

  SeqRetimingKey *added_key = (new_keys + new_key_index);
  added_key->strip_frame_index = frame_index;
  added_key->retiming_factor = value;

  return added_key;
}

SeqRetimingKey *SEQ_retiming_add_key(const Scene *scene, Strip *strip, const int timeline_frame)
{
  return strip_retiming_add_key(strip, content_frame_index_get(scene, strip, timeline_frame));
}

void SEQ_retiming_transition_key_frame_set(const Scene *scene,
                                           const Strip *strip,
                                           SeqRetimingKey *key,
                                           const int timeline_frame)
{
  SeqRetimingKey *key_start = SEQ_retiming_transition_start_get(key);
  SeqRetimingKey *key_end = key_start + 1;
  const float start_frame_index = key_start->strip_frame_index;
  const float midpoint = key_start->original_strip_frame_index;
  const float new_frame_index = content_frame_index_get(scene, strip, timeline_frame);
  float new_midpoint_offset = new_frame_index - midpoint;
  const double prev_segment_step = strip_retiming_segment_step_get(key_start - 1);
  const double next_segment_step = strip_retiming_segment_step_get(key_end);

  /* Prevent keys crossing eachother. */
  SeqRetimingKey *prev_segment_end = key_start - 1, *next_segment_start = key_end + 1;
  const float offset_max_left = midpoint - prev_segment_end->strip_frame_index - 1;
  const float offset_max_right = next_segment_start->strip_frame_index - midpoint - 1;
  new_midpoint_offset = fabs(new_midpoint_offset);
  new_midpoint_offset = min_fff(new_midpoint_offset, offset_max_left, offset_max_right);
  new_midpoint_offset = max_ff(new_midpoint_offset, 1);

  key_start->strip_frame_index = midpoint - new_midpoint_offset;
  key_end->strip_frame_index = midpoint + new_midpoint_offset;

  const float offset = key_start->strip_frame_index - start_frame_index;
  key_start->retiming_factor += offset * prev_segment_step;
  key_end->retiming_factor -= offset * next_segment_step;
}

static void strip_retiming_cleanup_freeze_frame(SeqRetimingKey *key)
{
  if ((key->flag & SEQ_FREEZE_FRAME_IN) != 0) {
    SeqRetimingKey *next_key = key + 1;
    key->flag &= ~SEQ_FREEZE_FRAME_IN;
    next_key->flag &= ~SEQ_FREEZE_FRAME_OUT;
  }
  if ((key->flag & SEQ_FREEZE_FRAME_OUT) != 0) {
    SeqRetimingKey *previous_key = key - 1;
    key->flag &= ~SEQ_FREEZE_FRAME_OUT;
    previous_key->flag &= ~SEQ_FREEZE_FRAME_IN;
  }
}

void SEQ_retiming_remove_multiple_keys(Strip *strip,
                                       blender::Vector<SeqRetimingKey *> &keys_to_remove)
{
  /* Transitions need special treatment, so separate these from `keys_to_remove`. */
  blender::Vector<SeqRetimingKey *> transitions;

  /* Cleanup freeze frames and extract transition keys. */
  for (SeqRetimingKey *key : keys_to_remove) {
    if (SEQ_retiming_key_is_freeze_frame(key)) {
      strip_retiming_cleanup_freeze_frame(key);
    }
    if ((key->flag & SEQ_SPEED_TRANSITION_IN) != 0) {
      transitions.append_non_duplicates(key);
      transitions.append_non_duplicates(key + 1);
    }
    if ((key->flag & SEQ_SPEED_TRANSITION_OUT) != 0) {
      transitions.append_non_duplicates(key);
      transitions.append_non_duplicates(key - 1);
    }
  }

  /* Sanitize keys to be removed. */
  keys_to_remove.remove_if([&](const SeqRetimingKey *key) {
    return key->strip_frame_index == 0 || SEQ_retiming_is_last_key(strip, key) ||
           SEQ_retiming_key_is_transition_type(key);
  });

  const size_t keys_count = SEQ_retiming_keys_count(strip);
  size_t new_keys_count = keys_count - keys_to_remove.size() - transitions.size() / 2;
  SeqRetimingKey *new_keys = (SeqRetimingKey *)MEM_callocN(new_keys_count * sizeof(SeqRetimingKey),
                                                           __func__);
  int keys_copied = 0;

  /* Copy keys to new array. */
  for (SeqRetimingKey &key : SEQ_retiming_keys_get(strip)) {
    /* Create key that was used to make transition in new array. */
    if (transitions.contains(&key) && SEQ_retiming_key_is_transition_start(&key)) {
      SeqRetimingKey *new_key = new_keys + keys_copied;
      new_key->strip_frame_index = key.original_strip_frame_index;
      new_key->retiming_factor = key.original_retiming_factor;
      keys_copied++;
      continue;
    }
    if (keys_to_remove.contains(&key) || transitions.contains(&key)) {
      continue;
    }
    memcpy(new_keys + keys_copied, &key, sizeof(SeqRetimingKey));
    keys_copied++;
  }

  MEM_freeN(strip->retiming_keys);
  strip->retiming_keys = new_keys;
  strip->retiming_keys_num = new_keys_count;
}

static void strip_retiming_remove_key_ex(Strip *strip, SeqRetimingKey *key)
{
  if (key->strip_frame_index == 0 || SEQ_retiming_is_last_key(strip, key)) {
    return; /* First and last key can not be removed. */
  }

  if (SEQ_retiming_key_is_freeze_frame(key)) {
    strip_retiming_cleanup_freeze_frame(key);
  }

  size_t keys_count = SEQ_retiming_keys_count(strip);
  SeqRetimingKey *keys = (SeqRetimingKey *)MEM_callocN((keys_count - 1) * sizeof(SeqRetimingKey),
                                                       __func__);

  const int key_index = key - strip->retiming_keys;
  memcpy(keys, strip->retiming_keys, (key_index) * sizeof(SeqRetimingKey));
  memcpy(keys + key_index,
         strip->retiming_keys + key_index + 1,
         (keys_count - key_index - 1) * sizeof(SeqRetimingKey));
  MEM_freeN(strip->retiming_keys);
  strip->retiming_keys = keys;
  strip->retiming_keys_num--;
}

/* This function removes transition segment and creates retiming key where it originally was. */
static SeqRetimingKey *strip_retiming_remove_transition(Strip *strip, SeqRetimingKey *key)
{
  SeqRetimingKey *transition_start = key;
  if ((key->flag & SEQ_SPEED_TRANSITION_OUT) != 0) {
    transition_start = key - 1;
  }

  const float orig_frame_index = transition_start->original_strip_frame_index;
  const float orig_retiming_factor = transition_start->original_retiming_factor;

  /* Remove both keys defining transition. */
  int key_index = SEQ_retiming_key_index_get(strip, transition_start);
  strip_retiming_remove_key_ex(strip, transition_start);
  strip_retiming_remove_key_ex(strip, strip->retiming_keys + key_index);

  /* Create original linear key. */
  SeqRetimingKey *orig_key = strip_retiming_add_key(strip, orig_frame_index);
  orig_key->retiming_factor = orig_retiming_factor;
  return orig_key;
}

void SEQ_retiming_remove_key(Strip *strip, SeqRetimingKey *key)
{

  if (SEQ_retiming_key_is_transition_type(key)) {
    strip_retiming_remove_transition(strip, key);
    return;
  }

  strip_retiming_remove_key_ex(strip, key);
}

static float strip_retiming_clamp_create_offset(const Scene *scene,
                                                const Strip *strip,
                                                SeqRetimingKey *key,
                                                int offset)
{
  SeqRetimingKey *prev_key = key - 1;
  SeqRetimingKey *next_key = key + 1;
  const int prev_dist = SEQ_retiming_key_timeline_frame_get(scene, strip, prev_key) -
                        SEQ_retiming_key_timeline_frame_get(scene, strip, key);
  const int next_dist = SEQ_retiming_key_timeline_frame_get(scene, strip, next_key) -
                        SEQ_retiming_key_timeline_frame_get(scene, strip, key);
  return std::clamp(offset, prev_dist + 1, next_dist - 1);
}

SeqRetimingKey *SEQ_retiming_add_freeze_frame(const Scene *scene,
                                              Strip *strip,
                                              SeqRetimingKey *key,
                                              const int offset)
{
  /* First offset old key, then add new key to original place with same fac
   * This is not great way to do things, but it's done in order to be able to freeze last key. */
  if (SEQ_retiming_key_is_transition_type(key)) {
    return nullptr;
  }

  int clamped_offset = strip_retiming_clamp_create_offset(
      scene, strip, key, offset * SEQ_time_media_playback_rate_factor_get(scene, strip));

  const int orig_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, key);
  const float orig_retiming_factor = key->retiming_factor;
  key->strip_frame_index += clamped_offset;
  key->flag |= SEQ_FREEZE_FRAME_OUT;

  SeqRetimingKey *new_key = SEQ_retiming_add_key(scene, strip, orig_timeline_frame);

  if (new_key == nullptr) {
    key->strip_frame_index -= clamped_offset;
    key->flag &= ~SEQ_FREEZE_FRAME_OUT;
    return nullptr;
  }

  new_key->retiming_factor = orig_retiming_factor;
  new_key->flag |= SEQ_FREEZE_FRAME_IN;

  /* Tag previous key as freeze frame key. This is only a convenient way to prevent creating
   * speed transitions. When freeze frame is deleted, this flag should be cleared. */
  return new_key + 1;
}

SeqRetimingKey *SEQ_retiming_add_transition(const Scene *scene,
                                            Strip *strip,
                                            SeqRetimingKey *key,
                                            float offset)
{
  BLI_assert(!SEQ_retiming_is_last_key(strip, key));
  BLI_assert(key->strip_frame_index != 0);

  SeqRetimingKey *prev_key = key - 1;
  if ((key->flag & SEQ_SPEED_TRANSITION_IN) != 0 ||
      (prev_key->flag & SEQ_SPEED_TRANSITION_IN) != 0)
  {
    return nullptr;
  }

  if ((key->flag & SEQ_FREEZE_FRAME_IN) != 0 || (prev_key->flag & SEQ_FREEZE_FRAME_IN) != 0) {
    return nullptr;
  }

  float clamped_offset = strip_retiming_clamp_create_offset(scene, strip, key, offset);

  const int orig_key_index = SEQ_retiming_key_index_get(strip, key);
  const float orig_frame_index = key->strip_frame_index;
  const float orig_retiming_factor = key->retiming_factor;

  SeqRetimingKey *transition_out = strip_retiming_add_key(strip,
                                                          orig_frame_index + clamped_offset);
  transition_out->flag |= SEQ_SPEED_TRANSITION_OUT;

  SeqRetimingKey *transition_in = strip_retiming_add_key(strip, orig_frame_index - clamped_offset);
  transition_in->flag |= SEQ_SPEED_TRANSITION_IN;
  transition_in->original_strip_frame_index = orig_frame_index;
  transition_in->original_retiming_factor = orig_retiming_factor;

  strip_retiming_remove_key_ex(strip, strip->retiming_keys + orig_key_index + 1);
  return strip->retiming_keys + orig_key_index + 1;
}

static float strip_retiming_clamp_transition_offset(const Scene *scene,
                                                    const Strip *strip,
                                                    SeqRetimingKey *start_key,
                                                    float offset)
{
  SeqRetimingKey *end_key = start_key + 1;
  SeqRetimingKey *prev_key = start_key - 1;
  SeqRetimingKey *next_key = start_key + 2;
  const float prev_max_offset = prev_key->strip_frame_index - start_key->strip_frame_index;
  const float next_max_offset = next_key->strip_frame_index - end_key->strip_frame_index;
  const float min_step = SEQ_time_media_playback_rate_factor_get(scene, strip);

  return std::clamp(offset, prev_max_offset + min_step, next_max_offset - min_step);
}

static void strip_retiming_transition_offset(const Scene *scene,
                                             Strip *strip,
                                             SeqRetimingKey *key,
                                             const float offset)
{
  float clamped_offset = strip_retiming_clamp_transition_offset(scene, strip, key, offset);
  const float duration = (key->original_strip_frame_index - key->strip_frame_index) /
                         SEQ_time_media_playback_rate_factor_get(scene, strip);
  const bool was_selected = SEQ_retiming_selection_contains(SEQ_editing_get(scene), key);

  SeqRetimingKey *original_key = strip_retiming_remove_transition(strip, key);
  original_key->strip_frame_index += clamped_offset;

  SeqRetimingKey *transition_out = SEQ_retiming_add_transition(
      scene, strip, original_key, duration);

  if (was_selected) {
    SEQ_retiming_selection_append(transition_out);
    SEQ_retiming_selection_append(transition_out - 1);
  }
}

static int strip_retiming_clamp_timeline_frame(const Scene *scene,
                                               Strip *strip,
                                               SeqRetimingKey *key,
                                               const int timeline_frame)
{
  if ((key->flag & SEQ_SPEED_TRANSITION_IN) != 0) {
    return timeline_frame;
  }

  int prev_key_timeline_frame = -MAXFRAME;
  int next_key_timeline_frame = MAXFRAME;

  if (key->strip_frame_index > 0) {
    SeqRetimingKey *prev_key = key - 1;
    prev_key_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, prev_key);
  }

  if (!SEQ_retiming_is_last_key(strip, key)) {
    SeqRetimingKey *next_key = key + 1;
    next_key_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, next_key);
  }

  return std::clamp(timeline_frame, prev_key_timeline_frame + 1, next_key_timeline_frame - 1);
}

/* Remove and re-create transition. This way transition won't change length.
 * Alternative solution is to find where in arc segment the `y` value is closest to key
 * retiming factor, then trim transition to that point. This would change transition length. */

static void strip_retiming_fix_transition(const Scene *scene, Strip *strip, SeqRetimingKey *key)
{
  const int keys_num = strip->retiming_keys_num;

  const float transition_duration = (key->original_strip_frame_index - key->strip_frame_index) /
                                    SEQ_time_media_playback_rate_factor_get(scene, strip);
  SeqRetimingKey *orig_key = strip_retiming_remove_transition(strip, key);
  SEQ_retiming_add_transition(scene, strip, orig_key, transition_duration);
  BLI_assert(keys_num == strip->retiming_keys_num);
  UNUSED_VARS_NDEBUG(keys_num);
}

static void strip_retiming_fix_transitions(const Scene *scene, Strip *strip, SeqRetimingKey *key)
{
  /* Store value, since handles array will be reallocated. */
  const int key_index = SEQ_retiming_key_index_get(strip, key);

  if (key_index > 1) {
    SeqRetimingKey *prev_key = key - 2;
    if (SEQ_retiming_key_is_transition_start(prev_key)) {
      strip_retiming_fix_transition(scene, strip, prev_key);
    }
  }

  if (!SEQ_retiming_is_last_key(strip, key)) {
    SeqRetimingKey *next_key = &SEQ_retiming_keys_get(strip)[key_index + 1];
    if (SEQ_retiming_key_is_transition_start(next_key)) {
      strip_retiming_fix_transition(scene, strip, next_key);
    }
  }
}

static void strip_retiming_key_offset(const Scene *scene,
                                      Strip *strip,
                                      SeqRetimingKey *key,
                                      const float offset)
{
  if ((key->flag & SEQ_SPEED_TRANSITION_IN) != 0) {
    strip_retiming_transition_offset(scene, strip, key, offset);
  }
  else {
    key->strip_frame_index += offset;
    strip_retiming_fix_transitions(scene, strip, key);
  }
}

int SEQ_retiming_key_timeline_frame_get(const Scene *scene,
                                        const Strip *strip,
                                        const SeqRetimingKey *key)
{
  int sound_offset = SEQ_time_get_rounded_sound_offset(scene, strip);
  return round_fl_to_int(SEQ_time_start_frame_get(strip) + sound_offset +
                         key->strip_frame_index /
                             SEQ_time_media_playback_rate_factor_get(scene, strip));
}

void SEQ_retiming_key_timeline_frame_set(const Scene *scene,
                                         Strip *strip,
                                         SeqRetimingKey *key,
                                         const int timeline_frame)
{
  if ((key->flag & SEQ_SPEED_TRANSITION_OUT) != 0) {
    return;
  }

  const int orig_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, key);
  const int clamped_timeline_frame = strip_retiming_clamp_timeline_frame(
      scene, strip, key, timeline_frame);
  const float offset = (clamped_timeline_frame - orig_timeline_frame) *
                       SEQ_time_media_playback_rate_factor_get(scene, strip);

  const int key_count = SEQ_retiming_keys_get(strip).size();
  const int key_index = SEQ_retiming_key_index_get(strip, key);

  if (orig_timeline_frame == SEQ_time_right_handle_frame_get(scene, strip)) {
    for (int i = key_index; i < key_count; i++) {
      SeqRetimingKey *key_iter = &SEQ_retiming_keys_get(strip)[i];
      strip_retiming_key_offset(scene, strip, key_iter, offset);
    }
  }
  else if (orig_timeline_frame == SEQ_time_left_handle_frame_get(scene, strip) ||
           key->strip_frame_index == 0)
  {
    strip->start += clamped_timeline_frame - orig_timeline_frame;
    for (int i = key_index + 1; i < key_count; i++) {
      SeqRetimingKey *key_iter = &SEQ_retiming_keys_get(strip)[i];
      strip_retiming_key_offset(scene, strip, key_iter, -offset);
    }
  }
  else {
    strip_retiming_key_offset(scene, strip, key, offset);
  }

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

float SEQ_retiming_key_speed_get(const Strip *strip, const SeqRetimingKey *key)
{
  if (key->strip_frame_index == 0) {
    return 1.0f;
  }

  const SeqRetimingKey *key_prev = key - 1;
  const int frame_index_max = strip->len - 1;
  const float frame_index_start = round_fl_to_int(key_prev->retiming_factor * frame_index_max);
  const float frame_index_end = round_fl_to_int(key->retiming_factor * frame_index_max);
  const float segment_content_frame_count = frame_index_end - frame_index_start;
  const float segment_length = key->strip_frame_index - key_prev->strip_frame_index;
  const float speed = segment_content_frame_count / segment_length;
  return speed;
}

void SEQ_retiming_key_speed_set(
    const Scene *scene, Strip *strip, SeqRetimingKey *key, const float speed, bool keep_retiming)
{
  if (key->strip_frame_index == 0) {
    return;
  }

  const SeqRetimingKey *key_prev = key - 1;

  const int frame_index_max = strip->len - 1;
  const float frame_index_prev = round_fl_to_int(key_prev->retiming_factor * frame_index_max);
  const float frame_index = round_fl_to_int(key->retiming_factor * frame_index_max);

  const float segment_timeline_duration = (frame_index - frame_index_prev) /
                                          SEQ_time_media_playback_rate_factor_get(scene, strip);
  const float new_timeline_duration = segment_timeline_duration / speed;

  const float orig_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, strip, key);
  const float new_timeline_frame = std::round(
      SEQ_retiming_key_timeline_frame_get(scene, strip, key_prev) + new_timeline_duration);

  SEQ_retiming_key_timeline_frame_set(scene, strip, key, new_timeline_frame);

  if (keep_retiming) {
    const int key_index = SEQ_retiming_key_index_get(strip, key);
    const int offset = new_timeline_frame - orig_timeline_frame;
    for (int i = key_index + 1; i < SEQ_retiming_keys_count(strip); i++) {
      SeqRetimingKey *key_iter = &SEQ_retiming_keys_get(strip)[i];
      strip_retiming_key_offset(scene, strip, key_iter, offset);
    }
  }
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
  RetimingRange(const Strip *strip, int start_frame, int end_frame, float speed, eRangeType type)
      : start(start_frame), end(end_frame), speed(speed), type(type)
  {
    if (type == TRANSITION) {
      this->speed = 1.0f;
      claculate_speed_table_from_seq(strip);
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
  RetimingRange operator*(const RetimingRange &rhs_range)
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

  void claculate_speed_table_from_seq(const Strip *strip)
  {
    for (int timeline_frame = start; timeline_frame <= end; timeline_frame++) {
      /* We need number actual number of frames here. */
      const double normal_step = 1 / double(strip->len - 1);

      const int frame_index = timeline_frame - SEQ_time_start_frame_get(strip);
      /* Who needs calculus, when you can have slow code? */
      const double val_prev = strip_retiming_evaluate(strip, frame_index - 1);
      const double val = strip_retiming_evaluate(strip, frame_index);
      const double speed_at_frame = (val - val_prev) / normal_step;
      speed_table.append(speed_at_frame);
    }
  }

  eIntersectType intersect_type(const RetimingRange &other) const
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
  RetimingRangeData(const Strip *strip)
  {
    for (const SeqRetimingKey &key : SEQ_retiming_keys_get(strip)) {
      if (key.strip_frame_index == 0) {
        continue;
      }
      const SeqRetimingKey *key_prev = &key - 1;
      float speed = SEQ_retiming_key_speed_get(strip, &key);
      int frame_start = SEQ_time_start_frame_get(strip) + key_prev->strip_frame_index;
      int frame_end = SEQ_time_start_frame_get(strip) + key.strip_frame_index;

      eRangeType type = SEQ_retiming_key_is_transition_start(key_prev) ? TRANSITION : LINEAR;
      RetimingRange range = RetimingRange(strip, frame_start, frame_end, speed, type);
      ranges.append(range);
    }
  }

  RetimingRangeData &operator*=(const RetimingRangeData &rhs)
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
        if (range.intersect_type(rhs_range) == FULL) {
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

static RetimingRangeData strip_retiming_range_data_get(const Scene *scene, const Strip *strip)
{
  RetimingRangeData strip_retiming_data = RetimingRangeData(strip);

  const Strip *meta_parent = SEQ_lookup_meta_by_strip(scene, strip);
  if (meta_parent == nullptr) {
    return strip_retiming_data;
  }

  RetimingRangeData meta_retiming_data = RetimingRangeData(meta_parent);
  strip_retiming_data *= meta_retiming_data;
  return strip_retiming_data;
}

void SEQ_retiming_sound_animation_data_set(const Scene *scene, const Strip *strip)
{
  /* Content cut off by `anim_startofs` is as if it does not exist for sequencer. But Audaspace
   * seeking relies on having animation buffer initialized for whole sequence. */
  if (strip->anim_startofs > 0) {
    const int strip_start = SEQ_time_start_frame_get(strip);
    BKE_sound_set_scene_sound_pitch_constant_range(
        strip->scene_sound, strip_start - strip->anim_startofs, strip_start, 1.0f);
  }

  int sound_offset = SEQ_time_get_rounded_sound_offset(scene, strip);

  RetimingRangeData retiming_data = strip_retiming_range_data_get(scene, strip);
  for (int i = 0; i < retiming_data.ranges.size(); i++) {
    RetimingRange range = retiming_data.ranges[i];
    if (range.type == TRANSITION) {

      const int range_length = range.end - range.start;
      for (int i = 0; i <= range_length; i++) {
        const int frame = range.start + i;
        BKE_sound_set_scene_sound_pitch_at_frame(
            strip->scene_sound, frame + sound_offset, range.speed_table[i], true);
      }
    }
    else {
      BKE_sound_set_scene_sound_pitch_constant_range(
          strip->scene_sound, range.start + sound_offset, range.end + sound_offset, range.speed);
    }
  }
}

bool SEQ_retiming_selection_clear(const Editing *ed)
{
  bool was_empty = true;

  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    for (SeqRetimingKey &key : SEQ_retiming_keys_get(strip)) {
      was_empty &= (key.flag & SEQ_KEY_SELECTED) == 0;
      key.flag &= ~SEQ_KEY_SELECTED;
    }
  }
  return !was_empty;
}

void SEQ_retiming_selection_append(

    SeqRetimingKey *key)
{
  key->flag |= SEQ_KEY_SELECTED;
}

void SEQ_retiming_selection_remove(SeqRetimingKey *key)
{
  key->flag &= ~SEQ_KEY_SELECTED;
}

void SEQ_retiming_selection_copy(SeqRetimingKey *dst, const SeqRetimingKey *src)
{
  SEQ_retiming_selection_remove(dst);
  dst->flag |= (src->flag & SEQ_KEY_SELECTED);
}

blender::Map<SeqRetimingKey *, Strip *> SEQ_retiming_selection_get(const Editing *ed)
{
  blender::Map<SeqRetimingKey *, Strip *> selection;
  if (!ed) {
    return selection;
  }
  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    for (SeqRetimingKey &key : SEQ_retiming_keys_get(strip)) {
      if ((key.flag & SEQ_KEY_SELECTED) != 0) {
        selection.add(&key, strip);
      }
    }
  }
  return selection;
}

bool SEQ_retiming_selection_contains(const Editing *ed, const SeqRetimingKey *key)
{
  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    for (const SeqRetimingKey &key_iter : SEQ_retiming_keys_get(strip)) {
      if ((key_iter.flag & SEQ_KEY_SELECTED) != 0 && &key_iter == key) {
        return true;
      }
    }
  }
  return false;
}

bool SEQ_retiming_selection_has_whole_transition(const Editing *ed, SeqRetimingKey *key)
{
  SeqRetimingKey *key_start = SEQ_retiming_transition_start_get(key);
  SeqRetimingKey *key_end = key_start + 1;
  bool has_start = false, has_end = false;

  blender::Map<SeqRetimingKey *, Strip *> selection = SEQ_retiming_selection_get(ed);

  for (auto item : selection.items()) {
    if (item.key == key_start) {
      has_start = true;
    }
    if (item.key == key_end) {
      has_end = true;
    }
    if (has_start && has_end) {
      return true;
    }
  }
  return false;
}
