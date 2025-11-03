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
#include "DNA_sound_types.h"

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_sound.hh"

#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "sequencer.hh"
#include "strip_time.hh"

namespace blender::seq {

MutableSpan<SeqRetimingKey> retiming_keys_get(const Strip *strip)
{
  blender::MutableSpan<SeqRetimingKey> handles(strip->retiming_keys, strip->retiming_keys_num);
  return handles;
}

bool retiming_is_last_key(const Strip *strip, const SeqRetimingKey *key)
{
  return retiming_key_index_get(strip, key) == strip->retiming_keys_num - 1;
}

SeqRetimingKey *retiming_last_key_get(const Strip *strip)
{
  return strip->retiming_keys + strip->retiming_keys_num - 1;
}

int retiming_key_index_get(const Strip *strip, const SeqRetimingKey *key)
{
  return key - strip->retiming_keys;
}

static int content_frame_index_get(const Scene *scene,
                                   const Strip *strip,
                                   const int timeline_frame)
{
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const int sound_offset = time_get_rounded_sound_offset(strip, scene_fps);
  return (timeline_frame - time_start_frame_get(strip) - sound_offset) *
         time_media_playback_rate_factor_get(strip, scene_fps);
}

SeqRetimingKey *retiming_key_get_by_timeline_frame(const Scene *scene,
                                                   const Strip *strip,
                                                   const int timeline_frame)
{
  for (auto &key : retiming_keys_get(strip)) {
    const int key_timeline_frame = retiming_key_timeline_frame_get(scene, strip, &key);
    if (key_timeline_frame == timeline_frame) {
      return &key;
    }
  }

  return nullptr;
}

SeqRetimingKey *retiming_find_segment_start_key(const Strip *strip, float frame_index)
{
  SeqRetimingKey *start_key = nullptr;
  for (auto &key : retiming_keys_get(strip)) {
    if (retiming_is_last_key(strip, &key)) {
      break;
    }
    if (key.strip_frame_index > frame_index) {
      break;
    }

    start_key = &key;
  }

  return start_key;
}

int retiming_keys_count(const Strip *strip)
{
  return strip->retiming_keys_num;
}

void retiming_data_ensure(Strip *strip)
{
  if (!retiming_is_allowed(strip)) {
    return;
  }

  if (retiming_is_active(strip)) {
    return;
  }

  strip->retiming_keys = MEM_calloc_arrayN<SeqRetimingKey>(2, __func__);
  SeqRetimingKey *key = strip->retiming_keys + 1;
  key->strip_frame_index = strip->len - 1;
  key->retiming_factor = 1.0f;
  strip->retiming_keys_num = 2;
}

void retiming_data_clear(Strip *strip)
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
  ListBase *seqbase = active_seqbase_get(editing_get(scene));
  blender::VectorSet<Strip *> strips;
  blender::VectorSet<Strip *> dependant;
  dependant.add(strip);
  iterator_set_expand(scene, seqbase, dependant, query_strip_effect_chain);
  strips.add_multiple(dependant);
  dependant.remove(strip);
  transform_handle_overlap(scene, seqbase, strips, dependant, true);
}

void retiming_reset(Scene *scene, Strip *strip)
{
  if (!retiming_is_allowed(strip)) {
    return;
  }

  retiming_data_clear(strip);

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));

  retiming_key_overlap(scene, strip);
}

bool retiming_is_active(const Strip *strip)
{
  return strip->retiming_keys_num > 1;
}

bool retiming_data_is_editable(const Strip *strip)
{
  return strip->flag & SEQ_SHOW_RETIMING;
}

bool retiming_is_allowed(const Strip *strip)
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

static float strip_retiming_segment_step_get(const SeqRetimingKey *start_key)
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

bool retiming_key_is_transition_type(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_SPEED_TRANSITION_IN) != 0 || (key->flag & SEQ_SPEED_TRANSITION_OUT) != 0;
}

bool retiming_key_is_transition_start(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_SPEED_TRANSITION_IN) != 0;
}

SeqRetimingKey *retiming_transition_start_get(SeqRetimingKey *key)
{
  if (key->flag & SEQ_SPEED_TRANSITION_OUT) {
    return key - 1;
  }
  if (key->flag & SEQ_SPEED_TRANSITION_IN) {
    return key;
  }
  return nullptr;
}

bool retiming_key_is_freeze_frame(const SeqRetimingKey *key)
{
  return (key->flag & SEQ_FREEZE_FRAME_IN) != 0 || (key->flag & SEQ_FREEZE_FRAME_OUT) != 0;
}

/* Check colinearity of 2 segments allowing for some imprecision.
 * `isect_seg_seg_v2_lambda_mu_db()` return value does not work well in this case. */

static bool strip_retiming_transition_is_linear(const Strip *strip, const SeqRetimingKey *key)
{
  const float prev_speed = retiming_key_speed_get(strip, key);
  const float next_speed = retiming_key_speed_get(strip, key + 2);

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
  const SeqRetimingKey *start_key = retiming_find_segment_start_key(strip, frame_index);

  const int start_key_index = start_key - strip->retiming_keys;
  BLI_assert(start_key_index < strip->retiming_keys_num);

  const float segment_frame_index = frame_index - start_key->strip_frame_index;

  if (!retiming_key_is_transition_start(start_key)) {
    const float segment_step = strip_retiming_segment_step_get(start_key);
    return std::min(1.0f, start_key->retiming_factor + float(segment_step * segment_frame_index));
  }

  if (strip_retiming_transition_is_linear(strip, start_key)) {
    const float segment_step = strip_retiming_segment_step_get(start_key - 1);
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
  if (!retiming_is_allowed(strip)) {
    return nullptr;
  }
  /* Clamp timeline frame to strip content range. */
  if (frame_index <= 0) {
    return &strip->retiming_keys[0];
  }
  if (frame_index >= retiming_last_key_get(strip)->strip_frame_index) {
    return retiming_last_key_get(strip); /* This is expected for strips with no offsets. */
  }

  SeqRetimingKey *start_key = retiming_find_segment_start_key(strip, frame_index);

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
  const int keys_count = retiming_keys_count(strip);
  const int new_key_index = start_key - keys + 1;
  BLI_assert(new_key_index >= 0);
  BLI_assert(new_key_index < keys_count);

  SeqRetimingKey *new_keys = MEM_calloc_arrayN<SeqRetimingKey>(keys_count + 1, __func__);
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

SeqRetimingKey *retiming_add_key(const Scene *scene, Strip *strip, const int timeline_frame)
{
  return strip_retiming_add_key(strip, content_frame_index_get(scene, strip, timeline_frame));
}

void retiming_transition_key_frame_set(const Scene *scene,
                                       const Strip *strip,
                                       SeqRetimingKey *key,
                                       const int timeline_frame)
{
  SeqRetimingKey *key_start = retiming_transition_start_get(key);
  SeqRetimingKey *key_end = key_start + 1;
  const float start_frame_index = key_start->strip_frame_index;
  const float midpoint = key_start->original_strip_frame_index;
  const float new_frame_index = content_frame_index_get(scene, strip, timeline_frame);
  float new_midpoint_offset = new_frame_index - midpoint;
  const float prev_segment_step = strip_retiming_segment_step_get(key_start - 1);
  const float next_segment_step = strip_retiming_segment_step_get(key_end);

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

void retiming_remove_multiple_keys(Strip *strip, blender::Vector<SeqRetimingKey *> &keys_to_remove)
{
  /* Transitions need special treatment, so separate these from `keys_to_remove`. */
  blender::Vector<SeqRetimingKey *> transitions;

  /* Cleanup freeze frames and extract transition keys. */
  for (SeqRetimingKey *key : keys_to_remove) {
    if (retiming_key_is_freeze_frame(key)) {
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
    return key->strip_frame_index == 0 || retiming_is_last_key(strip, key) ||
           retiming_key_is_transition_type(key);
  });

  const size_t keys_count = retiming_keys_count(strip);
  size_t new_keys_count = keys_count - keys_to_remove.size() - transitions.size() / 2;
  SeqRetimingKey *new_keys = MEM_calloc_arrayN<SeqRetimingKey>(new_keys_count, __func__);
  int keys_copied = 0;

  /* Copy keys to new array. */
  for (SeqRetimingKey &key : retiming_keys_get(strip)) {
    /* Create key that was used to make transition in new array. */
    if (transitions.contains(&key) && retiming_key_is_transition_start(&key)) {
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
  if (key->strip_frame_index == 0 || retiming_is_last_key(strip, key)) {
    return; /* First and last key can not be removed. */
  }

  if (retiming_key_is_freeze_frame(key)) {
    strip_retiming_cleanup_freeze_frame(key);
  }

  size_t keys_count = retiming_keys_count(strip);
  SeqRetimingKey *keys = MEM_calloc_arrayN<SeqRetimingKey>(keys_count - 1, __func__);

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
  int key_index = retiming_key_index_get(strip, transition_start);
  strip_retiming_remove_key_ex(strip, transition_start);
  strip_retiming_remove_key_ex(strip, strip->retiming_keys + key_index);

  /* Create original linear key. */
  SeqRetimingKey *orig_key = strip_retiming_add_key(strip, orig_frame_index);
  orig_key->retiming_factor = orig_retiming_factor;
  return orig_key;
}

void retiming_remove_key(Strip *strip, SeqRetimingKey *key)
{

  if (retiming_key_is_transition_type(key)) {
    strip_retiming_remove_transition(strip, key);
    return;
  }

  strip_retiming_remove_key_ex(strip, key);
}

static Bounds<float> strip_retiming_clamp_bounds_get(const Scene *scene,
                                                     const Strip *strip,
                                                     SeqRetimingKey *key)
{
  Bounds<float> max_tml_frame_offset = {MINAFRAMEF, MAXFRAMEF};

  if (key->strip_frame_index != 0) {
    SeqRetimingKey *prev_key = key - 1;
    max_tml_frame_offset.min = retiming_key_timeline_frame_get(scene, strip, prev_key) -
                               retiming_key_timeline_frame_get(scene, strip, key);
  }
  if (!retiming_is_last_key(strip, key)) {
    SeqRetimingKey *next_key = key + 1;
    max_tml_frame_offset.max = retiming_key_timeline_frame_get(scene, strip, next_key) -
                               retiming_key_timeline_frame_get(scene, strip, key);
  }
  return max_tml_frame_offset;
}

/* Create pair of retiming keys separated by offset. */
static std::pair<SeqRetimingKey *, SeqRetimingKey *> freeze_key_pair_create(const Scene *scene,
                                                                            Strip *strip,
                                                                            SeqRetimingKey *key,
                                                                            const int offset)
{

  Bounds<float> max_offset = strip_retiming_clamp_bounds_get(scene, strip, key);
  const float tml_frame_offset = math::clamp(float(offset), max_offset.min, max_offset.max);
  const int orig_timeline_frame = retiming_key_timeline_frame_get(scene, strip, key);

  /* Offset last key first, then add a freeze start key before it, because it is not possible to
   * add keys after last one. */
  if (retiming_is_last_key(strip, key)) {
    const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
    const float frame_index_offset = tml_frame_offset *
                                     time_media_playback_rate_factor_get(strip, scene_fps);
    key->strip_frame_index += frame_index_offset;
    SeqRetimingKey *freeze_start = retiming_add_key(scene, strip, orig_timeline_frame);

    if (freeze_start == nullptr) {
      key->strip_frame_index -= frame_index_offset;
      return {nullptr, nullptr};
    }
    return {freeze_start, freeze_start + 1};
  }

  SeqRetimingKey *freeze_end = retiming_add_key(
      scene, strip, orig_timeline_frame + tml_frame_offset);

  if (freeze_end == nullptr) {
    return {nullptr, nullptr};
  }

  return {freeze_end - 1, freeze_end};
}

/* This function tags previous key as freeze frame key. This is only a convenient way to prevent
 * creating speed transitions. When freeze frame is deleted, this flag should be cleared. */
SeqRetimingKey *retiming_add_freeze_frame(const Scene *scene,
                                          Strip *strip,
                                          SeqRetimingKey *key,
                                          const int offset)
{
  if (retiming_key_is_transition_type(key)) {
    return nullptr;
  }

  const float orig_retiming_factor = key->retiming_factor;
  std::pair<SeqRetimingKey *, SeqRetimingKey *> freeze_keys = freeze_key_pair_create(
      scene, strip, key, offset);

  if (freeze_keys.first == nullptr) {
    return nullptr;
  }

  freeze_keys.first->flag |= SEQ_FREEZE_FRAME_IN;
  freeze_keys.second->flag |= SEQ_FREEZE_FRAME_OUT;
  freeze_keys.first->retiming_factor = orig_retiming_factor;
  freeze_keys.second->retiming_factor = orig_retiming_factor;
  return freeze_keys.second;
}

SeqRetimingKey *retiming_add_transition(const Scene *scene,
                                        Strip *strip,
                                        SeqRetimingKey *key,
                                        float offset)
{
  BLI_assert(!retiming_is_last_key(strip, key));
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

  Bounds<float> max_offset = strip_retiming_clamp_bounds_get(scene, strip, key);
  float clamped_offset = math::clamp(offset, max_offset.min, max_offset.max);

  const int orig_key_index = retiming_key_index_get(strip, key);
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
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float min_step = time_media_playback_rate_factor_get(strip, scene_fps);

  return std::clamp(offset, prev_max_offset + min_step, next_max_offset - min_step);
}

static void strip_retiming_transition_offset(const Scene *scene,
                                             Strip *strip,
                                             SeqRetimingKey *key,
                                             const float offset)
{
  float clamped_offset = strip_retiming_clamp_transition_offset(scene, strip, key, offset);
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float duration = (key->original_strip_frame_index - key->strip_frame_index) /
                         time_media_playback_rate_factor_get(strip, scene_fps);
  const bool was_selected = retiming_selection_contains(editing_get(scene), key);

  SeqRetimingKey *original_key = strip_retiming_remove_transition(strip, key);
  original_key->strip_frame_index += clamped_offset;

  SeqRetimingKey *transition_out = retiming_add_transition(scene, strip, original_key, duration);

  if (was_selected) {
    retiming_selection_append(transition_out);
    retiming_selection_append(transition_out - 1);
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
    prev_key_timeline_frame = retiming_key_timeline_frame_get(scene, strip, prev_key);
  }

  if (!retiming_is_last_key(strip, key)) {
    SeqRetimingKey *next_key = key + 1;
    next_key_timeline_frame = retiming_key_timeline_frame_get(scene, strip, next_key);
  }

  return std::clamp(timeline_frame, prev_key_timeline_frame + 1, next_key_timeline_frame - 1);
}

/* Remove and re-create transition. This way transition won't change length.
 * Alternative solution is to find where in arc segment the `y` value is closest to key
 * retiming factor, then trim transition to that point. This would change transition length. */

static void strip_retiming_fix_transition(const Scene *scene, Strip *strip, SeqRetimingKey *key)
{
  const int keys_num = strip->retiming_keys_num;
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float transition_duration = (key->original_strip_frame_index - key->strip_frame_index) /
                                    time_media_playback_rate_factor_get(strip, scene_fps);
  SeqRetimingKey *orig_key = strip_retiming_remove_transition(strip, key);
  retiming_add_transition(scene, strip, orig_key, transition_duration);
  BLI_assert(keys_num == strip->retiming_keys_num);
  UNUSED_VARS_NDEBUG(keys_num);
}

static void strip_retiming_fix_transitions(const Scene *scene, Strip *strip, SeqRetimingKey *key)
{
  /* Store value, since handles array will be reallocated. */
  const int key_index = retiming_key_index_get(strip, key);

  if (key_index > 1) {
    SeqRetimingKey *prev_key = key - 2;
    if (retiming_key_is_transition_start(prev_key)) {
      strip_retiming_fix_transition(scene, strip, prev_key);
    }
  }

  if (!retiming_is_last_key(strip, &retiming_keys_get(strip)[key_index])) {
    SeqRetimingKey *next_key = &retiming_keys_get(strip)[key_index + 1];
    if (retiming_key_is_transition_start(next_key)) {
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

int retiming_key_timeline_frame_get(const Scene *scene,
                                    const Strip *strip,
                                    const SeqRetimingKey *key)
{
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const int sound_offset = time_get_rounded_sound_offset(strip, scene_fps);
  return round_fl_to_int(time_start_frame_get(strip) + sound_offset +
                         key->strip_frame_index /
                             time_media_playback_rate_factor_get(strip, scene_fps));
}

void retiming_key_timeline_frame_set(const Scene *scene,
                                     Strip *strip,
                                     SeqRetimingKey *key,
                                     const int timeline_frame)
{
  if ((key->flag & SEQ_SPEED_TRANSITION_OUT) != 0) {
    return;
  }

  const int orig_timeline_frame = retiming_key_timeline_frame_get(scene, strip, key);
  const int clamped_timeline_frame = strip_retiming_clamp_timeline_frame(
      scene, strip, key, timeline_frame);
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float offset = (clamped_timeline_frame - orig_timeline_frame) *
                       time_media_playback_rate_factor_get(strip, scene_fps);

  const int key_count = retiming_keys_get(strip).size();
  const int key_index = retiming_key_index_get(strip, key);

  if (orig_timeline_frame == time_right_handle_frame_get(scene, strip)) {
    for (int i = key_index; i < key_count; i++) {
      SeqRetimingKey *key_iter = &retiming_keys_get(strip)[i];
      strip_retiming_key_offset(scene, strip, key_iter, offset);
    }
  }
  else if (orig_timeline_frame == time_left_handle_frame_get(scene, strip) ||
           key->strip_frame_index == 0)
  {
    strip->start += clamped_timeline_frame - orig_timeline_frame;
    for (int i = key_index + 1; i < key_count; i++) {
      SeqRetimingKey *key_iter = &retiming_keys_get(strip)[i];
      strip_retiming_key_offset(scene, strip, key_iter, -offset);
    }
  }
  else {
    strip_retiming_key_offset(scene, strip, key, offset);
  }

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));
}

float retiming_key_speed_get(const Strip *strip, const SeqRetimingKey *key)
{
  if (key->strip_frame_index == 0) {
    return 1.0f;
  }

  BLI_assert(retiming_key_index_get(strip, key) > 0);
  const SeqRetimingKey *key_prev = key - 1;
  const int frame_index_max = strip->len - 1;
  const float frame_index_start = round_fl_to_int(key_prev->retiming_factor * frame_index_max);
  const float frame_index_end = round_fl_to_int(key->retiming_factor * frame_index_max);
  const float segment_content_frame_count = frame_index_end - frame_index_start;
  const float segment_length = key->strip_frame_index - key_prev->strip_frame_index;
  const float speed = segment_content_frame_count / segment_length;
  return speed;
}

void retiming_key_speed_set(
    const Scene *scene, Strip *strip, SeqRetimingKey *key, const float speed, bool keep_retiming)
{
  if (key->strip_frame_index == 0) {
    return;
  }

  const SeqRetimingKey *key_prev = key - 1;

  const int frame_index_max = strip->len - 1;
  const float frame_index_prev = round_fl_to_int(key_prev->retiming_factor * frame_index_max);
  const float frame_index = round_fl_to_int(key->retiming_factor * frame_index_max);

  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float segment_timeline_duration = (frame_index - frame_index_prev) /
                                          time_media_playback_rate_factor_get(strip, scene_fps);
  const float new_timeline_duration = segment_timeline_duration / speed;

  const float orig_timeline_frame = retiming_key_timeline_frame_get(scene, strip, key);
  const float new_timeline_frame = std::round(
      retiming_key_timeline_frame_get(scene, strip, key_prev) + new_timeline_duration);

  retiming_key_timeline_frame_set(scene, strip, key, new_timeline_frame);

  if (keep_retiming) {
    const int key_index = retiming_key_index_get(strip, key);
    const int offset = new_timeline_frame - orig_timeline_frame;
    for (int i = key_index + 1; i < retiming_keys_count(strip); i++) {
      SeqRetimingKey *key_iter = &retiming_keys_get(strip)[i];
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

      const int frame_index = timeline_frame - time_start_frame_get(strip);
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
    for (const SeqRetimingKey &key : retiming_keys_get(strip)) {
      if (key.strip_frame_index == 0) {
        continue;
      }
      const SeqRetimingKey *key_prev = &key - 1;
      float speed = retiming_key_speed_get(strip, &key);
      int frame_start = time_start_frame_get(strip) + key_prev->strip_frame_index;
      int frame_end = time_start_frame_get(strip) + key.strip_frame_index;

      eRangeType type = retiming_key_is_transition_start(key_prev) ? TRANSITION : LINEAR;
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

  const Strip *meta_parent = lookup_meta_by_strip(scene->ed, strip);
  if (meta_parent == nullptr) {
    return strip_retiming_data;
  }

  RetimingRangeData meta_retiming_data = RetimingRangeData(meta_parent);
  strip_retiming_data *= meta_retiming_data;
  return strip_retiming_data;
}

void retiming_sound_animation_data_set(const Scene *scene, const Strip *strip)
{

  RetimingRangeData retiming_data = strip_retiming_range_data_get(scene, strip);

  /* No need to apply the time-stretch effect if all the retiming range speeds are 1, as the
   * effect itself is still expensive while the audio is playing and want to avoid having to use it
   * whenever we can. */
  bool correct_pitch = (strip->flag & SEQ_AUDIO_PITCH_CORRECTION) && strip->sound != nullptr &&
                       std::any_of(retiming_data.ranges.begin(),
                                   retiming_data.ranges.end(),
                                   [](const RetimingRange &range) {
                                     return range.type != TRANSITION && range.speed != 1.0;
                                   });
#if !defined(WITH_RUBBERBAND)
  correct_pitch = false;
#endif

  void *sound_handle = strip->sound ? strip->sound->playback_handle : nullptr;
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  if (correct_pitch) {
    sound_handle = BKE_sound_ensure_time_stretch_effect(
        sound_handle, strip->scene_sound, scene_fps);
    BKE_sound_set_scene_sound_pitch_constant_range(
        strip->scene_sound, 0, strip->start + strip->len, 1.0f);
  }

  /* Content cut off by `anim_startofs` is as if it does not exist for sequencer. But Audaspace
   * seeking relies on having animation buffer initialized for whole sequence. */
  if (strip->anim_startofs > 0) {
    const int strip_start = time_start_frame_get(strip);
    BKE_sound_set_scene_sound_pitch_constant_range(
        strip->scene_sound, strip_start - strip->anim_startofs, strip_start, 1.0f);
  }

  const int sound_offset = time_get_rounded_sound_offset(strip, scene_fps);

  for (int i = 0; i < retiming_data.ranges.size(); i++) {
    const RetimingRange &range = retiming_data.ranges[i];
    if (range.type == TRANSITION) {
      const int range_length = range.end - range.start;
      for (int i = 0; i <= range_length; i++) {
        const int frame = range.start + i;
        if (correct_pitch) {
          BKE_sound_set_scene_sound_time_stretch_at_frame(
              sound_handle, frame - strip->start, 1.0 / range.speed_table[i], true);
        }
        else {
          BKE_sound_set_scene_sound_pitch_at_frame(
              strip->scene_sound, frame + sound_offset, range.speed_table[i], true);
        }
      }
    }
    else {
      if (correct_pitch) {
        const float speed = range.speed == 0.0f ? 1.0f : 1.0f / range.speed;
        BKE_sound_set_scene_sound_time_stretch_constant_range(
            sound_handle, range.start - strip->start, range.end - strip->start, speed);
      }
      else {
        BKE_sound_set_scene_sound_pitch_constant_range(
            strip->scene_sound, range.start + sound_offset, range.end + sound_offset, range.speed);
      }
    }
  }

  if (correct_pitch) {
    BKE_sound_update_sequence_handle(strip->scene_sound, sound_handle);
  }
}

bool retiming_selection_clear(const Editing *ed)
{
  bool was_empty = true;

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    for (SeqRetimingKey &key : retiming_keys_get(strip)) {
      was_empty &= (key.flag & SEQ_KEY_SELECTED) == 0;
      key.flag &= ~SEQ_KEY_SELECTED;
    }
  }
  return !was_empty;
}

void retiming_selection_append(

    SeqRetimingKey *key)
{
  key->flag |= SEQ_KEY_SELECTED;
}

void retiming_selection_remove(SeqRetimingKey *key)
{
  key->flag &= ~SEQ_KEY_SELECTED;
}

void retiming_selection_copy(SeqRetimingKey *dst, const SeqRetimingKey *src)
{
  retiming_selection_remove(dst);
  dst->flag |= (src->flag & SEQ_KEY_SELECTED);
}

blender::Map<SeqRetimingKey *, Strip *> retiming_selection_get(const Editing *ed)
{
  blender::Map<SeqRetimingKey *, Strip *> selection;
  if (!ed) {
    return selection;
  }
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    for (SeqRetimingKey &key : retiming_keys_get(strip)) {
      if ((key.flag & SEQ_KEY_SELECTED) != 0) {
        selection.add(&key, strip);
      }
    }
  }
  return selection;
}

bool retiming_selection_contains(const Editing *ed, const SeqRetimingKey *key)
{
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    for (const SeqRetimingKey &key_iter : retiming_keys_get(strip)) {
      if ((key_iter.flag & SEQ_KEY_SELECTED) != 0 && &key_iter == key) {
        return true;
      }
    }
  }
  return false;
}

bool retiming_selection_has_whole_transition(const Editing *ed, SeqRetimingKey *key)
{
  SeqRetimingKey *key_start = retiming_transition_start_get(key);
  SeqRetimingKey *key_end = key_start + 1;
  bool has_start = false, has_end = false;

  blender::Map<SeqRetimingKey *, Strip *> selection = retiming_selection_get(ed);

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

}  // namespace blender::seq
