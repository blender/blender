/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 *
 * \brief Defines an abstraction around various structs to modify their transform properties via a
 * unified API for the purpose of animation.
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_span.hh"

#include "DNA_action_types.h"

#include "RNA_types.hh"

namespace blender {

struct bPoseChannel;
struct ID;

namespace ed {

/**
 * Used to limit the modification of properties to certain axes.
 */
enum AxisMutable : int8_t {
  AXIS_MUTABLE_X = 1 << 0,
  AXIS_MUTABLE_Y = 1 << 1,
  AXIS_MUTABLE_Z = 1 << 2,
  AXIS_MUTABLE_ALL = AXIS_MUTABLE_X | AXIS_MUTABLE_Y | AXIS_MUTABLE_Z,
  /* There is currently no support for a W axis. This was already the case when porting this enum
   * from the pose slide code. */
};
ENUM_OPERATORS(AxisMutable);

/* By using Array<float, 4> we enforce a stack allocation limit of 4. Since we use at most 4
 * elements per property, we ensure that the values are always on the stack
 * for better performance. */
using TransformFloats = Array<float, 4>;
using TransformFloatPtrs = Array<float *, 4>;

/**
 * Describes a rotation in a specific mode and can be used to convert into other modes.
 */
struct Rotation {
  /* The array size differs depending on the rotation mode. */
  TransformFloats values;
  eRotationModes mode;

  /**
   * Returns a copy of the rotation in the given mode.
   *
   * \param reference_euler: Only used when converting to a euler rotation. The given Rotation *has
   * to be* of type euler too.
   */
  Rotation converted_to_mode(eRotationModes mode, const Rotation *reference_euler = nullptr) const;
};

/**
 * Provides a common interface to transform values for multiple structs.
 * In a way this is similar to RNA, however RNA has the issue that the properties don't have
 * consistent naming making it impossible to work with them in a generic way.
 */
class AnimTransformable {
 public:
  enum class Type : int8_t {
    POSE_BONE,
    OBJECT,
  };

  enum class PropertyType : int8_t { LOCATION, ROTATION, SCALE };

 private:
  Type type_;
  ID *owner_id_;
  /* The struct wrapped by the AnimTransformable. For possible types see `Type`. */
  void *data_;

  /* This is the path from the owner ID to the struct that the AnimTransformable represents. Has to
   * be created in the constructor. For structs that are an ID this is an empty string. */
  std::string rna_path_from_id_;
  std::string fcurve_group_name_;

  /* We are assuming here that the ground truth of transforms is store in separate loc rot scale
   * and not in a matrix, thus skew is not supported. */
  MutableSpan<float> location_;
  /* Rotation can be expressed in different modes, which are stored in separate arrays. We have to
   * use an Array of float* because the angle of axis-angle is a separate float property. This is
   * in contrast to e.g. `location_` which is always a float array so it can be referenced with a
   * `MutableSpan`. For the order of elements, see `RotationModeIndices` in `transformable.cc`. */
  Array<TransformFloatPtrs> rotations_;
  /* Points to an enum with the current rotation mode. See `eRotationModes`. */
  eRotationModes *rotation_mode_;
  MutableSpan<float> scale_;

  /**
   * Returns the correct array based on the given mode. Asserts that the array is set for the
   * current transformable.
   */
  const TransformFloatPtrs *get_rotation_array_from_mode(eRotationModes mode) const;

 public:
  /* There has to be a constructor for every struct supported. */
  /* Constructor for pose bones. */
  AnimTransformable(Object &owner_id, bPoseChannel &pchan);
  /* Constructor for Objects. */
  explicit AnimTransformable(Object &object);

  Type type() const
  {
    return type_;
  }

  ID *owner_id() const
  {
    return owner_id_;
  }

  StringRefNull fcurve_group_name() const
  {
    return fcurve_group_name_;
  }

  template<typename T> T data() const;

  /* Returns the rna path from the ID to the struct represented by this transformable. If the
   * struct is an ID this is an empty string. */
  StringRefNull rna_path() const;
  /**
   * Returns a string to the given property type.
   */
  std::string rna_path_to_property(PropertyType prop_type) const;
  std::string rna_path_to_rotation(const eRotationModes rotation_mode) const;
  std::string rna_path_to_rotation_mode() const;
  /**
   * Generic function that returns an rna path to the transformable for the property with the given
   * name. Note that the resulting string doesn't need to be a valid and existing RNA path. It is
   * up to the caller to pass the correct string for that.
   */
  std::string rna_path_to_property(const StringRef property_name) const;

  /**
   * Returns a copy of the rotation in the mode the transformable is currently in.
   */
  Rotation get_rotation() const;
  /**
   * Returns a copy of the rotation for the given mode. This is *not* the current rotation
   * converted to the given mode, but the values of the underlying rotation properties for the
   * given mode. For example, this can return the axis-angle rotation property values, even when
   * the transformable is in quaternion mode.
   */
  Rotation get_rotation_for_mode(eRotationModes mode) const;
  /**
   * Sets the rotation for the mode the transformable is currently in. If that doesn't match with
   * the given rotation, the `rotation` is converted.
   */
  void set_rotation(const Rotation &rotation);
  /**
   * Returns the current rotation mode of the transformable.
   */
  eRotationModes get_rotation_mode() const;
  /**
   * Only sets the rotation mode, does not touch the rotation properties or their animation.
   */
  void set_rotation_mode(eRotationModes mode);

  /**
   * Blends the rotation to the given `target`. If the rotation mode of the transformable and that
   * of the `target` does not match, the `target` is converted. This uses the correct interpolation
   * math depending on the rotation mode (LERP for euler, SLERP for quaternion). At `factor` 0, the
   * current rotation remains unchanged.
   */
  void blend_rotation_to(const Rotation &target, float factor, AxisMutable axis_flag);

  /**
   * Returns a copy of the property values for the given property type.
   * While this will return the values of the current rotation mode for PropertyType::ROTATION, it
   * is best to use the explicit function for it so a `Rotation` struct is returned which has more
   * features for dealing with different rotation modes.
   */
  TransformFloats get_property(PropertyType prop_type) const;
  /**
   * Generic way to set the given transform property. It is asserted that the value count matches
   * the current rotation mode. Use `set_rotation` to automatically convert to the correct mode.
   */
  void set_property(PropertyType prop_type, Span<float> values, AxisMutable axis_flag);
  /**
   * Do a linear blend of the property values towards the given `target`. It is asserted that the
   * given span size equals the property size. When setting the rotation property, it is the
   * responsibility of the caller to ensure that the values are in the correct mode. It is assumed
   * the `target` is in the same rotation mode as the transformable.
   *
   * \note This will not work correctly for quaternion rotations. Use `blend_rotation_to` instead.
   */
  void blend_property_to(PropertyType prop_type,
                         Span<float> target,
                         float factor,
                         AxisMutable axis_flag);
  /**
   * Overloaded function that blends all values of the given property type to the same float.
   */
  void blend_property_to(PropertyType prop_type,
                         float target,
                         float factor,
                         AxisMutable axis_flag);
};

/**
 * Returns a rotation representing "no rotation" for the given mode.
 */
Rotation identity_rotation(eRotationModes mode);
/**
 * Returns a new rotation, interpolated between `a` and `b` based on `factor`. If `factor` is 0
 * the result is `a`.
 * If the rotation mode on `a` and `b` does not match, then `b` is converted into the mode of `a`
 * first. The returned rotation is always in the rotation mode of `a`.
 * Quaternion rotations use spherical interpolation, all other modes use linear.
 */
Rotation rotation_interpolated(const Rotation &a, const Rotation &b, float factor);

/**
 * Interpolate the values linearly based on `factor` and returns a new Array. Asserts that both
 * spans are the same length. With the factor at `0` the values will match `a`.
 */
Array<float> property_interpolated(Span<float> a, Span<float> b, float factor);

}  // namespace ed
}  // namespace blender
