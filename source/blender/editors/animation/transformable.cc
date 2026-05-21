/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include "BLI_math_rotation.h"
#include "BLI_string.h"

#include "DNA_object_types.h"

#include "ANIM_rna.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_anim_transformable.hh"

namespace blender::ed {

/**
 * Returns true if the given property index matches the axis flag.
 * Always returns true if no flag is set.
 */
static bool is_axis_mutable(const int index, const AxisMutable axis_flag)
{
  /* AxisMutable happens to be set up in such a way that X, Y and Z correspond to bits 0, 1
   * and 2. The AXIS_MUTABLE_ALL case has all these bits set. */
  return axis_flag & (1 << index);
}

static TransformFloats copy_pointers_to_values(const Span<float *> value)
{
  TransformFloats copy(value.size());
  for (const int i : value.index_range()) {
    copy[i] = *(value[i]);
  }
  return copy;
}

static void copy_span_into_mutable_span(const Span<float> value,
                                        MutableSpan<float> target,
                                        const AxisMutable axis_flag)
{
  BLI_assert(target.size() == value.size());
  for (const int i : value.index_range()) {
    if (!is_axis_mutable(i, axis_flag)) {
      continue;
    }
    target[i] = value[i];
  }
}

/**
 * Blend all values to a single target value. At factor 0, the given `values` are not modified.
 */
static void blend_linear(MutableSpan<float> values,
                         const float target,
                         const float factor,
                         const AxisMutable axis_flag)
{
  for (const int i : values.index_range()) {
    if (!is_axis_mutable(i, axis_flag)) {
      continue;
    }
    values[i] += factor * (target - values[i]);
  }
}

/**
 * Blend the given `values` towards `target`. The indices are matched up and the Span lengths are
 * expected to match.
 */
static void blend_linear(MutableSpan<float> values,
                         const Span<float> target,
                         const float factor,
                         const AxisMutable axis_flag)
{
  BLI_assert(values.size() == target.size());
  for (const int i : values.index_range()) {
    if (!is_axis_mutable(i, axis_flag)) {
      continue;
    }
    values[i] += factor * (target[i] - values[i]);
  }
}

Array<float> property_interpolated(const Span<float> a, const Span<float> b, const float factor)
{
  BLI_assert(a.size() == b.size());
  Array<float> interpolated(a.size());
  for (const int i : a.index_range()) {
    interpolated[i] = interpf(b[i], a[i], factor);
  }
  return interpolated;
}

/* Since there can be more than one representation of rotation data, they are stored in an array.
 * The enum is the index into that array. */
enum RotationModeIndices : uint8_t {
  ROT_IDX_QUATERNION,
  ROT_IDX_AXIS_ANGLE,
  ROT_IDX_EULER,
  /* Not a rotation mode, always keep last. */
  ROT_IDX_MAX_ENUM,
};

Rotation Rotation::converted_to_mode(const eRotationModes mode) const
{
  if (mode == this->mode) {
    return *this;
  }

  float4 quat;
  switch (this->mode) {
    case ROT_MODE_QUAT:
      copy_qt_qt(quat, this->values.data());
      break;

    case ROT_MODE_AXISANGLE:
      axis_angle_to_quat(quat, &this->values[1], this->values[0]);
      break;

    default:
      BLI_assert(this->mode <= ROT_MODE_ZYX);
      eulO_to_quat(quat, this->values.data(), this->mode);
      break;
  }

  Rotation converted;
  converted.mode = mode;
  switch (mode) {
    case ROT_MODE_QUAT:
      converted.values.reinitialize(4);
      copy_qt_qt(converted.values.data(), quat);
      break;

    case ROT_MODE_AXISANGLE:
      converted.values.reinitialize(4);
      quat_to_axis_angle(&converted.values[1], &converted.values[0], quat);
      break;

    default:
      /* TODO (christoph): pass in a reference rotation for the conversion to euler. */
      BLI_assert(mode <= ROT_MODE_ZYX);
      converted.values.reinitialize(3);
      quat_to_eulO(converted.values.data(), mode, quat);
      break;
  }
  return converted;
}

Rotation identity_rotation(const eRotationModes mode)
{
  switch (mode) {
    case ROT_MODE_QUAT:
      return {{1, 0, 0, 0}, mode};
    case ROT_MODE_AXISANGLE:
      return {{0, 0, 1, 0}, mode};
    default:
      BLI_assert(mode <= ROT_MODE_ZYX);
      return {{0, 0, 0}, mode};
  }
}

static void interpolate_axis_angle(const float a_angle,
                                   const float3 &a_axis,
                                   const float b_angle,
                                   const float3 &b_axis,
                                   const float factor,
                                   float *r_angle,
                                   float r_axis[3])
{
  float4 a_quat, b_quat;
  axis_angle_to_quat(a_quat, a_axis, a_angle);
  axis_angle_to_quat(b_quat, b_axis, b_angle);
  float4 interpolated_quat;
  interp_qt_qtqt(interpolated_quat, a_quat, b_quat, factor);
  quat_to_axis_angle(r_axis, r_angle, interpolated_quat);
}

Rotation rotation_interpolated(const Rotation &a, const Rotation &b, const float factor)
{
  /* Only different from `b` if the rotation mode does not match `a`. */
  const Rotation b_aligned = b.converted_to_mode(a.mode);
  Rotation interpolated;
  interpolated.mode = a.mode;
  interpolated.values.reinitialize(a.values.size());
  switch (a.mode) {
    case ROT_MODE_QUAT:
      interp_qt_qtqt(interpolated.values.data(), a.values.data(), b_aligned.values.data(), factor);
      break;

    case ROT_MODE_AXISANGLE: {
      interpolate_axis_angle(a.values[0],
                             &a.values[1],
                             b_aligned.values[0],
                             &b_aligned.values[1],
                             factor,
                             &interpolated.values[0],
                             &interpolated.values[1]);
      break;
    }

    default:
      /* Should axis angle use a different interpolation mode? */
      for (const int i : interpolated.values.index_range()) {
        interpolated.values[i] = interpf(b_aligned.values[i], a.values[i], factor);
      }
      break;
  }

  return interpolated;
}

static std::string get_pose_bone_rna_path(const bPoseChannel &pose_bone)
{
  char name_esc[sizeof(pose_bone.name) * 2];
  BLI_str_escape(name_esc, pose_bone.name, sizeof(name_esc));
  return fmt::format("pose.bones[\"{}\"]", name_esc);
}

static void build_rotations_array(
    Array<TransformFloatPtrs> &rotations, float *euler, float *quat, float *axis, float *angle)
{
  rotations.reinitialize(ROT_IDX_MAX_ENUM);
  rotations[ROT_IDX_EULER] = TransformFloatPtrs(3);
  for (const int i : IndexRange(3)) {
    rotations[ROT_IDX_EULER][i] = &euler[i];
  }

  rotations[ROT_IDX_QUATERNION] = TransformFloatPtrs(4);
  for (const int i : IndexRange(4)) {
    rotations[ROT_IDX_QUATERNION][i] = &quat[i];
  }

  rotations[ROT_IDX_AXIS_ANGLE] = TransformFloatPtrs(4);
  for (const int i : IndexRange(3)) {
    rotations[ROT_IDX_AXIS_ANGLE][i + 1] = &axis[i];
  }
  rotations[ROT_IDX_AXIS_ANGLE][0] = angle;
}

AnimTransformable::AnimTransformable(Object &owner_id, bPoseChannel &pchan)
    : type_(AnimTransformable::Type::POSE_BONE),
      owner_id_(&owner_id.id),
      data_(&pchan),
      location_({pchan.loc, 3}),
      rotation_mode_(&pchan.rotmode),
      scale_({pchan.scale, 3})
{
  build_rotations_array(rotations_, pchan.eul, pchan.quat, pchan.rotAxis, &pchan.rotAngle);
  rna_path_from_id_ = get_pose_bone_rna_path(pchan);
}

template<> bPoseChannel *AnimTransformable::data<bPoseChannel *>() const
{
  BLI_assert(type_ == Type::POSE_BONE);
  return static_cast<bPoseChannel *>(data_);
}

StringRefNull AnimTransformable::rna_path() const
{
  return rna_path_from_id_;
}

std::string AnimTransformable::rna_path_to_property(const PropertyType prop_type) const
{
  /* Note that this assumes the property name for the underlying struct. If we add support for a
   * struct where this doesn't match, the property names have to be moved to the constructor. */
  StringRefNull property_name;
  switch (prop_type) {
    case PropertyType::LOCATION:
      property_name = "location";
      break;
    case PropertyType::ROTATION:
      property_name = animrig::get_rotation_mode_path(*rotation_mode_);
      break;
    case PropertyType::SCALE:
      property_name = "scale";
      break;
  }
  if (rna_path_from_id_.empty()) {
    return std::string(property_name);
  }
  return fmt::format("{}.{}", rna_path_from_id_, property_name);
}

TransformFloats AnimTransformable::get_property(const PropertyType prop_type) const
{
  switch (prop_type) {
    case PropertyType::LOCATION:
      return location_.as_span();

    case PropertyType::ROTATION: {
      const TransformFloatPtrs *rotation_array = get_rotation_array_from_mode(*rotation_mode_);
      return copy_pointers_to_values(*rotation_array);
    }
    case PropertyType::SCALE:
      return scale_.as_span();
  }

  BLI_assert_unreachable();
  return {};
}

void AnimTransformable::set_property(const PropertyType prop_type,
                                     const Span<float> values,
                                     const AxisMutable axis_flag)
{
  switch (prop_type) {
    case PropertyType::LOCATION:
      copy_span_into_mutable_span(values, location_, axis_flag);
      break;

    case PropertyType::ROTATION: {
      const TransformFloatPtrs *rotation_array = get_rotation_array_from_mode(*rotation_mode_);
      if (rotation_array->size() > values.size()) {
        /* Trying to set a rotation with different mode. Use `set_rotation` instead. */
        BLI_assert_unreachable();
        return;
      }
      /* Axis flags don't work with quaternion rotations. */
      BLI_assert((axis_flag == AXIS_MUTABLE_ALL) || (*rotation_mode_ != ROT_MODE_QUAT));
      for (const int i : rotation_array->index_range()) {
        if (!is_axis_mutable(i, axis_flag)) {
          continue;
        }
        *(*rotation_array)[i] = values[i];
      }
      break;
    }
    case PropertyType::SCALE:
      copy_span_into_mutable_span(values, scale_, axis_flag);
      break;
  }
}

void AnimTransformable::blend_property_to(const PropertyType prop_type,
                                          const Span<float> target,
                                          const float factor,
                                          const AxisMutable axis_flag)
{
  switch (prop_type) {
    case PropertyType::LOCATION:
      blend_linear(location_, target, factor, axis_flag);
      break;

    case PropertyType::ROTATION: {
      const TransformFloatPtrs *rotation_array = get_rotation_array_from_mode(*rotation_mode_);
      if (rotation_array->size() != target.size()) {
        /* This doesn't catch all invalid cases. Differing euler rotation order or quaternion/axis
         * angle will still have the same array size but blending will create bogus data. */
        BLI_assert_msg(false, "Cannot do blending with differing rotation modes");
        return;
      }
      Rotation rotation;
      /* Assuming the rotation mode. See docstring of function. */
      rotation.mode = *rotation_mode_;
      rotation.values = target;
      blend_rotation_to(rotation, factor, axis_flag);
      break;
    }
    case PropertyType::SCALE:
      blend_linear(scale_, target, factor, axis_flag);
      break;
  }
}

void AnimTransformable::blend_property_to(const PropertyType prop_type,
                                          const float target,
                                          const float factor,
                                          const AxisMutable axis_flag)
{
  switch (prop_type) {
    case PropertyType::LOCATION:
      blend_linear(location_, target, factor, axis_flag);
      break;

    case PropertyType::ROTATION: {
      BLI_assert(*rotation_mode_ != ROT_MODE_QUAT);
      const TransformFloatPtrs *rotation_array = get_rotation_array_from_mode(*rotation_mode_);
      Rotation rotation;
      /* Assuming the rotation mode. See docstring of function. */
      rotation.mode = *rotation_mode_;
      rotation.values.reinitialize(rotation_array->size());
      rotation.values.fill(target);
      blend_rotation_to(rotation, factor, axis_flag);
      break;
    }
    case PropertyType::SCALE:
      blend_linear(scale_, target, factor, axis_flag);
      break;
  }
}

const TransformFloatPtrs *AnimTransformable::get_rotation_array_from_mode(
    const eRotationModes mode) const
{
  const TransformFloatPtrs *rotations_array = nullptr;
  switch (mode) {
    case ROT_MODE_QUAT:
      rotations_array = &rotations_[ROT_IDX_QUATERNION];
      break;
    case ROT_MODE_AXISANGLE:
      rotations_array = &rotations_[ROT_IDX_AXIS_ANGLE];
      break;
    default:
      BLI_assert(mode <= ROT_MODE_ZYX);
      rotations_array = &rotations_[ROT_IDX_EULER];
      break;
  }
  return rotations_array;
}

Rotation AnimTransformable::get_rotation() const
{
  Rotation rotation;
  rotation.mode = *rotation_mode_;
  const TransformFloatPtrs *rotations_array = get_rotation_array_from_mode(rotation.mode);
  BLI_assert(rotations_array != nullptr);
  rotation.values = copy_pointers_to_values(*rotations_array);
  return rotation;
}

void AnimTransformable::set_rotation(const Rotation &rotation)
{
  const TransformFloatPtrs *rotations_array = get_rotation_array_from_mode(*rotation_mode_);
  BLI_assert(rotations_array != nullptr);
  if (rotation.mode == *rotation_mode_) {
    /* Easy case, can just copy the values. */
    for (const int i : rotations_array->index_range()) {
      *(*rotations_array)[i] = rotation.values[i];
    }
    return;
  }

  Rotation rot_in_correct_mode = rotation.converted_to_mode(*rotation_mode_);
  for (const int i : rotations_array->index_range()) {
    *(*rotations_array)[i] = rot_in_correct_mode.values[i];
  }
}

eRotationModes AnimTransformable::get_rotation_mode() const
{
  return *rotation_mode_;
}

void AnimTransformable::blend_rotation_to(const Rotation &target,
                                          const float factor,
                                          const AxisMutable axis_flag)
{
  /* If `target` matches the `current_mode`, the function will return `target` unmodified. */
  Rotation compatible_rotation = target.converted_to_mode(*rotation_mode_);

  const TransformFloatPtrs *rotations_array = get_rotation_array_from_mode(*rotation_mode_);
  BLI_assert(rotations_array != nullptr);

  TransformFloats result;
  switch (*rotation_mode_) {
    case ROT_MODE_QUAT: {
      float4 current_quat;
      for (const int i : IndexRange(4)) {
        current_quat[i] = *((*rotations_array)[i]);
      }
      normalize_qt(current_quat);
      normalize_qt(compatible_rotation.values.data());
      result.reinitialize(4);
      /* We are not using the axis flag here. Not sure how that would work with quaternions. */
      interp_qt_qtqt(result.data(), current_quat, compatible_rotation.values.data(), factor);
      break;
    }
    case ROT_MODE_AXISANGLE: {
      result.reinitialize(4);
      for (const int i : IndexRange(4)) {
        result[i] = *((*rotations_array)[i]);
      }
      interpolate_axis_angle(result[0],
                             &result[1],
                             compatible_rotation.values[0],
                             &compatible_rotation.values[1],
                             factor,
                             &result[0],
                             &result[1]);
      break;
    }
    default: {
      BLI_assert(*rotation_mode_ <= ROT_MODE_ZYX);
      result.reinitialize(3);
      for (const int i : IndexRange(3)) {
        result[i] = *((*rotations_array)[i]);
      }
      blend_linear(result, compatible_rotation.values, factor, axis_flag);
      break;
    }
  }

  BLI_assert(result.size() == rotations_array->size());
  for (const int i : result.index_range()) {
    *(*rotations_array)[i] = result[i];
  }
}

}  // namespace blender::ed
