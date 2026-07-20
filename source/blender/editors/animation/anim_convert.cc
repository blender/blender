/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLI_math_rotation_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_set.hh"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_rna.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

#include "ED_anim_api.hh"
#include "ED_anim_transformable.hh"

namespace blender {

void SortedFCurveBuffer::insert_fcurve(FCurve &fcurve)
{
  int insert_index = 0;
  for (FCurve *existing_fcurve : fcurves_) {
    BLI_assert_msg(fcurve.array_index != existing_fcurve->array_index,
                   "An FCurve with that index was already inserted");
    if (existing_fcurve->array_index > fcurve.array_index) {
      break;
    }
    insert_index++;
  }
  fcurves_.insert(insert_index, &fcurve);
}

void SortedFCurveBuffer::clear()
{
  fcurves_.clear();
}

Span<FCurve *> SortedFCurveBuffer::fcurves() const
{
  return fcurves_;
}

FCurve *SortedFCurveBuffer::get_fcurve_by_array_index(const int array_index) const
{
  for (FCurve *fcurve : fcurves_) {
    if (fcurve->array_index == array_index) {
      return fcurve;
    }
  }
  return nullptr;
}

/* Returns the ranges in which a rotation mode is active. Each entry denotes the starting point of
 * the range and it ends with the next entry. If the FCurve has any keys, there will be at least
 * one entry with the starting mode. */
static Vector<std::pair<float, eRotationModes>> get_rotation_mode_ranges(const FCurve &fcurve)
{
  BLI_assert(StringRefNull(fcurve.rna_path).endswith("rotation_mode"));
  if (!fcurve.bezt) {
    return {};
  }
  Vector<std::pair<float, eRotationModes>> changes;
  for (const int i : IndexRange(fcurve.totvert)) {
    const BezTriple &key = fcurve.bezt[i];
    const eRotationModes key_rotation_mode = eRotationModes(key.vec[1][1]);
    if (changes.is_empty()) {
      changes.append({0, key_rotation_mode});
      continue;
    }
    if (changes.last().second == key_rotation_mode) {
      continue;
    }
    changes.append({key.vec[1][0], key_rotation_mode});
  }
  return changes;
}

/**
 * Iterates all frames with keys on the given FCurves in the given range. Every frame is visited in
 * ascending order only once.
 */
class KeyframeIterator {
  Vector<const FCurve *> fcurves_;
  Array<int> key_indices_;
  Bounds<float> range_;

  float get_next_frame() const
  {
    float next_frame = FLT_MAX;
    for (const int i : fcurves_.index_range()) {
      const FCurve *fcurve = fcurves_[i];
      if (key_indices_[i] > fcurve->totvert - 1) {
        continue;
      }
      const float key_frame = fcurve->bezt[key_indices_[i]].vec[1][0];
      if (key_frame >= range_.max) {
        continue;
      }
      if (key_frame < next_frame) {
        next_frame = key_frame;
      }
    }
    return next_frame;
  }

 public:
  /**
   * \param fcurves is allowed to have nullptr entries.
   * \param range is interpreted as inclusive/exclusive.
   */
  KeyframeIterator(Span<const FCurve *> fcurves, const Bounds<float> range) : range_(range)
  {
    for (const int i : fcurves.index_range()) {
      if (!fcurves[i] || !fcurves[i]->bezt) {
        continue;
      }
      fcurves_.append(fcurves[i]);
    }

    key_indices_.reinitialize(fcurves_.size());
    bool frame_has_key;
    for (const int i : fcurves_.index_range()) {
      const FCurve *fcurve = fcurves_[i];
      const int index = BKE_fcurve_bezt_binarysearch_index(
          fcurve->bezt, range.min, fcurve->totvert, &frame_has_key);
      key_indices_[i] = index;
    }
  }

  bool can_advance() const
  {
    for (const int i : fcurves_.index_range()) {
      const FCurve *fcurve = fcurves_[i];
      if (key_indices_[i] > fcurve->totvert - 1) {
        /* No more keys for that FCurve. */
        continue;
      }
      const float key_frame = fcurve->bezt[key_indices_[i]].vec[1][0];
      if (key_frame < range_.max) {
        return true;
      }
    }
    return false;
  }

  void advance()
  {
    float next_frame = get_next_frame();

    if (next_frame == FLT_MAX) {
      /* No more keys to step in the range. */
      return;
    }

    for (const int i : fcurves_.index_range()) {
      const FCurve *fcurve = fcurves_[i];
      if (key_indices_[i] > fcurve->totvert - 1) {
        continue;
      }
      const float key_frame = fcurve->bezt[key_indices_[i]].vec[1][0];
      if (key_frame <= next_frame) {
        /* Advance all FCurves that have a key at that frame. */
        key_indices_[i]++;
      }
    }
  }

  /**
   * Returns the frame of the current iteration step. If the iterator has completed it will always
   * return 0.
   */
  float get_frame() const
  {
    float next_frame = get_next_frame();

    if (next_frame == FLT_MAX) {
      /* No more keys to step in the range. */
      return 0;
    }
    return next_frame;
  }

  /**
   * Returns the keyframe settings of the current step. The first FCurve with a key at the current
   * frame is used for this.
   */
  animrig::KeyframeSettings get_keyframe_settings() const
  {
    /* Always return some reasonable defaults. */
    animrig::KeyframeSettings settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO, BEZT_IPO_BEZ};
    BezTriple *key = nullptr;
    for (const int i : fcurves_.index_range()) {
      const FCurve *fcurve = fcurves_[i];
      if (key_indices_[i] > fcurve->totvert - 1) {
        continue;
      }
      if (key && fcurve->bezt[key_indices_[i]].vec[1][0] >= key->vec[1][0]) {
        continue;
      }
      key = &fcurve->bezt[key_indices_[i]];
    }
    if (key) {
      settings.handle = eBezTriple_Handle(key->h1);
      settings.interpolation = eBezTriple_Interpolation(key->ipo);
      settings.keyframe_type = BEZKEYTYPE(key);
    }
    return settings;
  }
};

/**
 * For all keyframes in the `evaluation_buffer` in the given range, insert values into
 * `insertion_buffer` that represent the same rotation but in the given rotation mode.
 *
 * \param evaluation_buffer It is assumed that those FCurves match the rotation mode `from_mode`.
 * They will be read for keyframe values. It is allowed to have nullptr FCurves in here.
 * \param insertion_buffer The FCurves relating to `to_mode`. Keyframes for the converted rotation
 * mode will be inserted here. None of the FCurves shall be a nullptr.
 * \param range Start and end frames to limit the range in which to convert and insert rotation
 * keys. Interpreted inclusive at the start and exclusive at the end and in FCurve time.
 * \param ensure_range_start_key if set to true a key will be set at `range.min` regardless of a
 * key existing on that frame in the evaluation buffer.
 */
static void convert_rotation_mode_range(const Span<const FCurve *> evaluation_buffer,
                                        const Span<FCurve *> insertion_buffer,
                                        const eRotationModes from_mode,
                                        const eRotationModes to_mode,
                                        const Bounds<float> range,
                                        const ed::AnimTransformable &transformable,
                                        const bool ensure_range_start_key)
{
  /* Filling the array with the current values to have good base values in case not every array
   * index is keyed. */
  ed::Rotation rotation_values = transformable.get_rotation_for_mode(from_mode);

  KeyframeIterator key_iterator = KeyframeIterator(evaluation_buffer, range);
  /* Generate the current rotation values respecting missing FCurves. */
  for (const FCurve *fcurve : evaluation_buffer) {
    if (!fcurve) {
      continue;
    }
    rotation_values.values[fcurve->array_index] = evaluate_fcurve(fcurve, range.min);
  }

  /* Storing the previous rotation for euler angles larger than 180 degrees. */
  ed::Rotation previous_conversion = rotation_values.converted_to_mode(to_mode);
  const animrig::KeyframeSettings settings = key_iterator.get_keyframe_settings();

  if (ensure_range_start_key && key_iterator.get_frame() > range.min) {
    /* This case can happen if the rotation mode is keyed, but not any of the rotation channels.
     * In that case the below loop would not insert a key into the range start which would result
     * in a visual jump after the conversion. */
    for (const int i : insertion_buffer.index_range()) {
      FCurve *fcurve = insertion_buffer[i];
      BLI_assert_msg(fcurve, "For insertion all FCurves are expected to be created before");
      insert_vert_fcurve(
          fcurve, {range.min, previous_conversion.values[i]}, settings, INSERTKEY_FAST);
    }
  }

  while (key_iterator.can_advance()) {
    const float frame = key_iterator.get_frame();
    const animrig::KeyframeSettings settings = key_iterator.get_keyframe_settings();
    key_iterator.advance();
    /* Generate the current rotation values respecting missing FCurves. */
    for (const FCurve *fcurve : evaluation_buffer) {
      if (!fcurve) {
        continue;
      }
      rotation_values.values[fcurve->array_index] = evaluate_fcurve(fcurve, frame);
    }
    ed::Rotation converted_rotation = rotation_values.converted_to_mode(to_mode,
                                                                        &previous_conversion);
    for (const int i : insertion_buffer.index_range()) {
      FCurve *fcurve = insertion_buffer[i];
      BLI_assert_msg(fcurve, "For insertion all FCurves are expected to be created before");
      insert_vert_fcurve(fcurve, {frame, converted_rotation.values[i]}, settings, INSERTKEY_FAST);
    }
    previous_conversion = std::move(converted_rotation);
  }
}

static void remove_rotation_fcurves(const ed::AnimTransformable &transformable,
                                    RNAFCurveMap &fcu_map,
                                    animrig::Channelbag &channelbag,
                                    const eRotationModes rotation_mode)
{
  std::string rna_path = transformable.rna_path_to_rotation(rotation_mode);
  SortedFCurveBuffer *rotation_fcurves = fcu_map.lookup_ptr(rna_path);
  if (!rotation_fcurves) {
    return;
  }
  /* Remove the FCurves that target the now no longer used rotation modes. */
  for (FCurve *fcurve : rotation_fcurves->fcurves()) {
    channelbag.fcurve_remove(*fcurve);
  }
  rotation_fcurves->clear();
}

static bool convert_rotation_mode_channelbag(animrig::Channelbag &channelbag,
                                             RNAFCurveMap &fcu_map,
                                             const eRotationModes to_mode,
                                             const Span<std::pair<float, eRotationModes>> ranges,
                                             const ed::AnimTransformable &transformable)
{
  const int insertion_buffer_count = to_mode > ROT_MODE_QUAT ? 3 : 4;
  Array<FCurve *> insertion_buffer(insertion_buffer_count);

  for (const int i : insertion_buffer.index_range()) {
    /* Is needed to get correct FCurve colors. */
    PropertySubType prop_subtype = PROP_EULER;
    if (to_mode == ROT_MODE_QUAT) {
      prop_subtype = PROP_QUATERNION;
    }
    else if (to_mode == ROT_MODE_AXISANGLE) {
      prop_subtype = PROP_AXISANGLE;
    }
    FCurve *fcurve = animrig::create_fcurve_for_channel(
        {transformable.rna_path_to_rotation(to_mode),
         i,
         PROP_FLOAT,
         prop_subtype,
         transformable.fcurve_group_name()});
    insertion_buffer[i] = fcurve;
  }

  bool modified_keys = false;
  for (const int i : ranges.index_range()) {
    const std::pair<float, eRotationModes> &rotation_mode_range = ranges[i];
    const eRotationModes from_mode = rotation_mode_range.second;

    const std::string from_mode_rna_path = transformable.rna_path_to_rotation(from_mode);
    const SortedFCurveBuffer *rotation_fcurves = fcu_map.lookup_ptr(from_mode_rna_path);
    if (!rotation_fcurves) {
      continue;
    }

    const int evaluation_buffer_count = from_mode > ROT_MODE_QUAT ? 3 : 4;
    Array<FCurve *> evaluation_buffer(evaluation_buffer_count);
    for (const int buffer_index : evaluation_buffer.index_range()) {
      evaluation_buffer[buffer_index] = rotation_fcurves->get_fcurve_by_array_index(buffer_index);
    }

    Bounds<float> range(rotation_mode_range.first, FLT_MAX);
    if (i + 1 < ranges.size()) {
      range.max = ranges[i + 1].first;
    }

    convert_rotation_mode_range(
        evaluation_buffer, insertion_buffer, from_mode, to_mode, range, transformable, i > 0);

    modified_keys = true;
  }

  if (!modified_keys) {
    /* There were no rotation FCurves to read from. In that case don't insert the
     * `insertion_buffer` FCurves into the channelbag. */
    for (FCurve *fcurve : insertion_buffer) {
      BKE_fcurve_free(fcurve);
    }
    return false;
  }

  /* Remove all old rotation FCurves. */
  remove_rotation_fcurves(transformable, fcu_map, channelbag, ROT_MODE_QUAT);
  remove_rotation_fcurves(transformable, fcu_map, channelbag, ROT_MODE_EUL);
  remove_rotation_fcurves(transformable, fcu_map, channelbag, ROT_MODE_AXISANGLE);

  for (FCurve *fcurve : insertion_buffer) {
    channelbag.fcurve_append(*fcurve);
    bActionGroup &grp = channelbag.channel_group_ensure(transformable.fcurve_group_name());
    channelbag.fcurve_assign_to_channel_group(*fcurve, grp);
    BKE_fcurve_handles_recalc(*fcurve);
  }
  return true;
}

bool convert_rotation_keys(const ed::AnimTransformable &transformable,
                           ChannelbagFCurveMap &channelbag_fcurve_map,
                           const eRotationModes to_mode)
{
  bool modified_keys = false;

  for (const auto &item : channelbag_fcurve_map.items()) {
    animrig::Channelbag *channelbag = item.key;
    RNAFCurveMap &fcu_map = item.value;
    const std::string rotation_mode_path = transformable.rna_path_to_rotation_mode();
    Vector<std::pair<float, eRotationModes>> rotation_mode_ranges;
    FCurve *rotation_mode_fcurve = nullptr;
    if (const SortedFCurveBuffer *rotation_mode_buffer = fcu_map.lookup_ptr(rotation_mode_path)) {
      BLI_assert(rotation_mode_buffer->fcurves().size() == 1);
      rotation_mode_fcurve = rotation_mode_buffer->fcurves()[0];
      rotation_mode_ranges = get_rotation_mode_ranges(*rotation_mode_fcurve);
    }
    else {
      /* Defaulting back to the struct value means that this can have unexpected results when
       * dealing with action layers. The rotation mode can still be animated by a higher layer but
       * that means we cannot know the correct rotation mode for the current layer. */
      rotation_mode_ranges = {{0, transformable.get_rotation_mode()}};
    }

    modified_keys |= convert_rotation_mode_channelbag(
        *channelbag, fcu_map, to_mode, rotation_mode_ranges, transformable);

    if (rotation_mode_fcurve && rotation_mode_fcurve->bezt) {
      for (const int i : IndexRange(rotation_mode_fcurve->totvert)) {
        rotation_mode_fcurve->bezt[i].vec[1][1] = to_mode;
      }
      BKE_fcurve_handles_recalc(*rotation_mode_fcurve);
    }
  }

  return modified_keys;
}

static bool is_rotation_mode_path(const StringRefNull rna_path)
{
  const int start_of_propname = rna_path.rfind(".") + 1;
  return rna_path.substr(start_of_propname, rna_path.size()) == "rotation_mode";
}

ChannelbagFCurveMap build_rotation_fcurve_map(animrig::Action &action,
                                              const animrig::slot_handle_t slot_handle)
{
  ChannelbagFCurveMap rotation_map;
  for (animrig::Channelbag *channelbag : channelbags_for_action_slot(action, slot_handle)) {
    RNAFCurveMap &curves = rotation_map.lookup_or_add(channelbag, {});
    for (FCurve *fcurve : channelbag->fcurves()) {
      StringRefNull rna_path(fcurve->rna_path);
      if (!animrig::is_rotation_path(rna_path) && !is_rotation_mode_path(rna_path)) {
        continue;
      }
      SortedFCurveBuffer &fcurve_buffer = curves.lookup_or_add(rna_path, {});
      fcurve_buffer.insert_fcurve(*fcurve);
    }
  }
  return rotation_map;
}

void bake_rotation_fcurves(const ChannelbagFCurveMap &channelbag_fcurve_map,
                           const ed::AnimTransformable &transformable)
{
  /* Need to bake on all potential FCurves to cover for an animated rotation mode. */
  const Array<eRotationModes> rotation_modes = {ROT_MODE_EUL, ROT_MODE_QUAT, ROT_MODE_AXISANGLE};
  for (const eRotationModes rotation_mode : rotation_modes) {
    std::string rotation_rna_path = transformable.rna_path_to_rotation(rotation_mode);

    for (const RNAFCurveMap &rna_fcurve_map : channelbag_fcurve_map.values()) {
      const SortedFCurveBuffer *fcurve_buffer = rna_fcurve_map.lookup_ptr(rotation_rna_path);
      if (!fcurve_buffer) {
        continue;
      }
      for (FCurve *fcurve : fcurve_buffer->fcurves()) {
        if (!fcurve || !fcurve->bezt) {
          continue;
        }
        const int2 range = {int(fcurve->bezt[0].vec[1][0]),
                            int(fcurve->bezt[fcurve->totvert - 1].vec[1][0])};
        animrig::bake_fcurve(fcurve, range, 1, animrig::BakeCurveRemove::ALL);
      }
    }
  }
}

void convert_to_rotation_mode(bContext &C,
                              ed::AnimTransformable &transformable,
                              const eRotationModes to_mode,
                              const bool bake)
{
  Main *bmain = CTX_data_main(&C);
  if (!BKE_id_is_editable(bmain, transformable.owner_id())) {
    return;
  }
  /* A map built per action to make it quicker to find the FCurves by RNA path. */
  Map<std::pair<animrig::Action *, animrig::slot_handle_t>, ChannelbagFCurveMap> data_map;

  bool converted_actions = false;
  animrig::foreach_action_slot_use(
      *transformable.owner_id(),
      [&](animrig::Action &action, const animrig::slot_handle_t slot_handle) {
        if (!BKE_id_is_editable(bmain, &action.id)) {
          return true;
        }
        if (!data_map.contains({&action, slot_handle})) {
          ChannelbagFCurveMap fcurve_map = build_rotation_fcurve_map(action, slot_handle);
          data_map.add({&action, slot_handle}, fcurve_map);
        }
        ChannelbagFCurveMap &channelbag_fcurve_map = data_map.lookup({&action, slot_handle});
        if (bake) {
          bake_rotation_fcurves(channelbag_fcurve_map, transformable);
        }
        converted_actions |= convert_rotation_keys(transformable, channelbag_fcurve_map, to_mode);
        DEG_id_tag_update(&action.id, ID_RECALC_ANIMATION);
        return true;
      });

  if (converted_actions) {
    transformable.set_rotation_mode(to_mode);
    ID *id = transformable.owner_id();
    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  }
  else {
    ed::Rotation rotation = transformable.get_rotation();
    transformable.set_rotation_mode(to_mode);
    transformable.set_rotation(rotation.converted_to_mode(to_mode));
  }
}

}  // namespace blender
