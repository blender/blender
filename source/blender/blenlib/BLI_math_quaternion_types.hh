/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <ostream>

#include "BLI_math_angle_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_basis_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Quaternion
 * \{ */

/**
 * A `blender::math::QuaternionBase<T>` represents either an orientation or a rotation.
 *
 * Mainly used for rigging and armature deformations as they have nice mathematical properties
 * (eg: smooth shortest path interpolation). A `blender::math::QuaternionBase<T>` is cheaper to
 * combine than `MatBase<T, 3, 3>`. However, transforming points is slower. Consider converting to
 * a rotation matrix if you are rotating many points.
 *
 * See this for more information:
 * https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation#Performance_comparisons
 */
template<typename T> struct QuaternionBase {
  T w, x, y, z;

  QuaternionBase() = default;

  QuaternionBase(const T &new_w, const T &new_x, const T &new_y, const T &new_z)
      : w(new_w), x(new_x), y(new_y), z(new_z) {};

  /**
   * Creates a quaternion from an vector without reordering the components.
   * \note Component order must follow the scalar constructor (w, x, y, z).
   */
  explicit QuaternionBase(const VecBase<T, 4> &vec) : QuaternionBase(UNPACK4(vec)) {};

  /**
   * Creates a quaternion from real (w) and imaginary parts (x, y, z).
   */
  QuaternionBase(const T &real, const VecBase<T, 3> &imaginary)
      : QuaternionBase(real, UNPACK3(imaginary)) {};

  /** Static functions. */

  static QuaternionBase identity()
  {
    return {1, 0, 0, 0};
  }

  /** This is just for convenience. Does not represent a rotation as it is degenerate. */
  static QuaternionBase zero()
  {
    return {0, 0, 0, 0};
  }

  /**
   * Create a quaternion from an exponential map representation.
   * An exponential map is basically the rotation axis multiplied by the rotation angle.
   */
  static QuaternionBase expmap(const VecBase<T, 3> &expmap);

  /** Conversions. */

  explicit operator VecBase<T, 4>() const
  {
    return {this->w, this->x, this->y, this->z};
  }

  /**
   * Create an exponential map representation of this quaternion.
   * An exponential map is basically the rotation axis multiplied by the rotation angle.
   */
  VecBase<T, 3> expmap() const;

  /**
   * Returns the full twist angle for a given \a axis direction.
   * The twist is the isolated rotation in the plane whose \a axis is normal to.
   */
  AngleRadianBase<T> twist_angle(const Axis axis) const;

  /**
   * Returns the twist part of this quaternion for the \a axis direction.
   * The twist is the isolated rotation in the plane whose \a axis is normal to.
   */
  QuaternionBase twist(const Axis axis) const;

  /**
   * Returns the swing part of this quaternion for the basis \a axis direction.
   * The swing is the original quaternion minus the twist around \a axis.
   * So we have the following identity : `q = q.swing(axis) * q.twist(axis)`
   */
  QuaternionBase swing(const Axis axis) const;

  /**
   * Returns the imaginary part of this quaternion (x, y, z).
   */
  const VecBase<T, 3> &imaginary_part() const
  {
    return *reinterpret_cast<const VecBase<T, 3> *>(&x);
  }
  VecBase<T, 3> &imaginary_part()
  {
    return *reinterpret_cast<VecBase<T, 3> *>(&x);
  }

  /** Methods. */

  /**
   * Return this quaternions orientation but wrapped around \a reference.
   *
   * This means the interpolation between the returned value and \a reference will always take the
   * shortest path. The angle between them will not be more than pi.
   *
   * \note This quaternion is expected to be a unit quaternion.
   * \note Works even if \a reference is *not* a unit quaternion.
   */
  QuaternionBase wrapped_around(const QuaternionBase &reference) const;

  /** Operators. */

  friend QuaternionBase operator*(const QuaternionBase &a, const QuaternionBase &b)
  {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
            a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x};
  }

  QuaternionBase &operator*=(const QuaternionBase &b) &
  {
    *this = *this * b;
    return *this;
  }

  /* Scalar product. */
  friend QuaternionBase operator*(const QuaternionBase &a, const T &b)
  {
    return {a.w * b, a.x * b, a.y * b, a.z * b};
  }

  /* Negate the quaternion. */
  friend QuaternionBase operator-(const QuaternionBase &a)
  {
    return {-a.w, -a.x, -a.y, -a.z};
  }

  BLI_STRUCT_EQUALITY_OPERATORS_4(QuaternionBase, w, x, y, z)

  uint64_t hash() const
  {
    return VecBase<T, 4>(*this).hash();
  }

  friend std::ostream &operator<<(std::ostream &stream, const QuaternionBase &rot)
  {
    return stream << "Quaternion" << static_cast<VecBase<T, 4>>(rot);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dual-Quaternion
 * \{ */

/**
 * A `blender::math::DualQuaternionBase<T>` implements dual-quaternion skinning with scale aware
 * transformation. It allows volume preserving deformation for skinning.
 *
 * The type is implemented so that multiple weighted `blender::math::DualQuaternionBase<T>`
 * can be aggregated into a final rotation. Calling `normalize(dual_quat)` is mandatory before
 * trying to transform points with it.
 */
template<typename T> struct DualQuaternionBase {
  /** Non-dual part. */
  QuaternionBase<T> quat;
  /** Dual part. */
  QuaternionBase<T> trans;

  /**
   * Scaling is saved separately to handle cases of non orthonormal axes, non uniform scale and
   * flipped axes.
   */
  /* TODO(fclem): Can this be replaced by a Mat3x3 ?
   * It currently holds some translation in some cases. Is this wanted?
   * This would save some flops all along the way. */
  MatBase<T, 4, 4> scale;
  /** The weight of #DualQuaternionBase.scale. Set to 0 if uniformly scaled to skip `scale` sum. */
  T scale_weight;
  /**
   * The weight of this dual-quaternion. Used for and summation & normalizing.
   * A weight of 0 means the quaternion is not valid.
   */
  T quat_weight;

  DualQuaternionBase() = delete;

  /**
   * Dual quaternion without scaling.
   */
  DualQuaternionBase(const QuaternionBase<T> &non_dual, const QuaternionBase<T> &dual);

  /**
   * Dual quaternion with scaling.
   */
  DualQuaternionBase(const QuaternionBase<T> &non_dual,
                     const QuaternionBase<T> &dual,
                     const MatBase<T, 4, 4> &scale_mat);

  /** Static functions. */

  static DualQuaternionBase identity()
  {
    return DualQuaternionBase(QuaternionBase<T>::identity(), QuaternionBase<T>::zero());
  }

  /** Methods. */

  /** Operators. */

  /** Apply a scalar weight to a dual quaternion. */
  DualQuaternionBase &operator*=(const T &t) &;

  /** Add two weighted dual-quaternions rotations. */
  DualQuaternionBase &operator+=(const DualQuaternionBase &b) &;

  /** Apply a scalar weight to a dual quaternion. */
  friend DualQuaternionBase operator*(const DualQuaternionBase &a, const T &t)
  {
    DualQuaternionBase dq = a;
    dq *= t;
    return dq;
  }

  /** Apply a scalar weight to a dual quaternion. */
  friend DualQuaternionBase operator*(const T &t, const DualQuaternionBase &a)
  {
    DualQuaternionBase dq = a;
    dq *= t;
    return dq;
  }

  /** Add two weighted dual-quaternions rotations. */
  friend DualQuaternionBase operator+(const DualQuaternionBase &a, const DualQuaternionBase &b)
  {
    DualQuaternionBase dq = a;
    dq += b;
    return dq;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_5(
      DualQuaternionBase, quat, trans, quat_weight, scale_weight, scale)

  friend std::ostream &operator<<(std::ostream &stream, const DualQuaternionBase &rot)
  {
    stream << "DualQuaternion(\n";
    stream << "  .quat  = " << rot.quat << "\n";
    stream << "  .trans = " << rot.trans << "\n";
    if (rot.scale_weight != T(0)) {
      stream << "  .scale = " << rot.scale;
      stream << "  .scale_weight = " << rot.scale_weight << "\n";
    }
    stream << "  .quat_weight = " << rot.quat_weight << "\n)\n";
    return stream;
  }
};

/**
 * Returns true if the #DualQuaternion has not been mixed with other #DualQuaternion and needs no
 * normalization.
 */
template<typename T> [[nodiscard]] inline bool is_normalized(const DualQuaternionBase<T> &dq)
{
  return dq.quat_weight == T(1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assertions
 * \{ */

template<typename U> struct AssertUnitEpsilon<QuaternionBase<U>> {
  static constexpr U value = AssertUnitEpsilon<U>::value * 10;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Intermediate Types
 *
 * Some functions need to have higher precision than standard floats for some operations.
 * \{ */

template<typename T> struct TypeTraits {
  using DoublePrecision = T;
};
template<> struct TypeTraits<float> {
  using DoublePrecision = double;
};

using Quaternion = QuaternionBase<float>;
using DualQuaternion = DualQuaternionBase<float>;

/** \} */

}  // namespace blender::math
