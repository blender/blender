/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_angle_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_basis_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Quaternion
 * \{ */

namespace detail {

/* Forward declaration for casting operators. */
template<typename T> struct EulerXYZ;

/**
 * A `blender::math::Quaternion<T>` represents either an orientation or a rotation.
 *
 * Mainly used for rigging and armature deformations as they have nice mathematical properties
 * (eg: smooth shortest path interpolation). A `blender::math::Quaternion<T>` is cheaper to combine
 * than `MatBase<T, 3, 3>`. However, transforming points is slower. Consider converting to a
 * rotation matrix if you are rotating many points.
 *
 * See this for more information:
 * https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation#Performance_comparisons
 */
template<typename T = float> struct Quaternion {
  T w, x, y, z;

  Quaternion() = default;

  Quaternion(const T &new_w, const T &new_x, const T &new_y, const T &new_z)
      : w(new_w), x(new_x), y(new_y), z(new_z){};

  /**
   * Creates a quaternion from an vector without reordering the components.
   * \note Component order must follow the scalar constructor (w, x, y, z).
   */
  explicit Quaternion(const VecBase<T, 4> &vec) : Quaternion(UNPACK4(vec)){};

  /**
   * Creates a quaternion from real (w) and imaginary parts (x, y, z).
   */
  Quaternion(const T &real, const VecBase<T, 3> &imaginary)
      : Quaternion(real, UNPACK3(imaginary)){};

  /** Static functions. */

  static Quaternion identity()
  {
    return {1, 0, 0, 0};
  }

  /** This is just for convenience. Does not represent a rotation as it is degenerate. */
  static Quaternion zero()
  {
    return {0, 0, 0, 0};
  }

  /**
   * Create a quaternion from an exponential map representation.
   * An exponential map is basically the rotation axis multiplied by the rotation angle.
   */
  static Quaternion expmap(const VecBase<T, 3> &expmap);

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
  AngleRadian<T> twist_angle(const Axis axis) const;

  /**
   * Returns the twist part of this quaternion for the \a axis direction.
   * The twist is the isolated rotation in the plane whose \a axis is normal to.
   */
  Quaternion twist(const Axis axis) const;

  /**
   * Returns the swing part of this quaternion for the basis \a axis direction.
   * The swing is the original quaternion minus the twist around \a axis.
   * So we have the following identity : `q = q.swing(axis) * q.twist(axis)`
   */
  Quaternion swing(const Axis axis) const;

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
  Quaternion wrapped_around(const Quaternion &reference) const;

  /** Operators. */

  friend Quaternion operator*(const Quaternion &a, const Quaternion &b)
  {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
            a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x};
  }

  Quaternion &operator*=(const Quaternion &b)
  {
    *this = *this * b;
    return *this;
  }

  /* Scalar product. */
  friend Quaternion operator*(const Quaternion &a, const T &b)
  {
    return {a.w * b, a.x * b, a.y * b, a.z * b};
  }

  /* Negate the quaternion. */
  friend Quaternion operator-(const Quaternion &a)
  {
    return {-a.w, -a.x, -a.y, -a.z};
  }

  friend bool operator==(const Quaternion &a, const Quaternion &b)
  {
    return (a.w == b.w) && (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
  }

  friend std::ostream &operator<<(std::ostream &stream, const Quaternion &rot)
  {
    return stream << "Quaternion" << static_cast<VecBase<T, 4>>(rot);
  }
};

}  // namespace detail

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dual-Quaternion
 * \{ */

namespace detail {

/**
 * A `blender::math::DualQuaternion<T>` implements dual-quaternion skinning with scale aware
 * transformation. It allows volume preserving deformation for skinning.
 *
 * The type is implemented so that multiple weighted `blender::math::DualQuaternion<T>`
 * can be aggregated into a final rotation. Calling `normalize(dual_quat)` is mandatory before
 * trying to transform points with it.
 */
template<typename T = float> struct DualQuaternion {
  /** Non-dual part. */
  Quaternion<T> quat;
  /** Dual part. */
  Quaternion<T> trans;

  /**
   * Scaling is saved separately to handle cases of non orthonormal axes, non uniform scale and
   * flipped axes.
   */
  /* TODO(fclem): Can this be replaced by a Mat3x3 ?
   * It currently holds some translation in some cases. Is this wanted?
   * This would save some flops all along the way. */
  MatBase<T, 4, 4> scale;
  /** The weight of #DualQuaternion.scale. Set to 0 if uniformly scaled to skip `scale` sum. */
  T scale_weight;
  /**
   * The weight of this dual-quaternion. Used for and summation & normalizing.
   * A weight of 0 means the quaternion is not valid.
   */
  T quat_weight;

  DualQuaternion() = delete;

  /**
   * Dual quaternion without scaling.
   */
  DualQuaternion(const Quaternion<T> &non_dual, const Quaternion<T> &dual);

  /**
   * Dual quaternion with scaling.
   */
  DualQuaternion(const Quaternion<T> &non_dual,
                 const Quaternion<T> &dual,
                 const MatBase<T, 4, 4> &scale_mat);

  /** Static functions. */

  static DualQuaternion identity()
  {
    return DualQuaternion(Quaternion<T>::identity(), Quaternion<T>::zero());
  }

  /** Methods. */

  /** Operators. */

  /** Apply a scalar weight to a dual quaternion. */
  DualQuaternion &operator*=(const T &t);

  /** Add two weighted dual-quaternions rotations. */
  DualQuaternion &operator+=(const DualQuaternion &b);

  /** Apply a scalar weight to a dual quaternion. */
  friend DualQuaternion operator*(const DualQuaternion &a, const T &t)
  {
    DualQuaternion dq = a;
    dq *= t;
    return dq;
  }

  /** Apply a scalar weight to a dual quaternion. */
  friend DualQuaternion operator*(const T &t, const DualQuaternion &a)
  {
    DualQuaternion dq = a;
    dq *= t;
    return dq;
  }

  /** Add two weighted dual-quaternions rotations. */
  friend DualQuaternion operator+(const DualQuaternion &a, const DualQuaternion &b)
  {
    DualQuaternion dq = a;
    dq += b;
    return dq;
  }

  friend bool operator==(const DualQuaternion &a, const DualQuaternion &b)
  {
    return (a.quat == b.quat) && (a.trans == b.trans) && (a.quat_weight == b.quat_weight) &&
           (a.scale_weight == b.scale_weight) && (a.scale == b.scale);
  }

  friend std::ostream &operator<<(std::ostream &stream, const DualQuaternion &rot)
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

}  // namespace detail

/**
 * Returns true if the #DualQuaternion has not been mixed with other #DualQuaternion and needs no
 * normalization.
 */
template<typename T> [[nodiscard]] inline bool is_normalized(const detail::DualQuaternion<T> &dq)
{
  return dq.quat_weight == T(1);
}

/** \} */

template<typename U> struct AssertUnitEpsilon<detail::Quaternion<U>> {
  static constexpr U value = AssertUnitEpsilon<U>::value * 10;
};

/**
 * Intermediate Types.
 *
 * Some functions need to have higher precision than standard floats for some operations.
 */
template<typename T> struct TypeTraits {
  using DoublePrecision = T;
};
template<> struct TypeTraits<float> {
  using DoublePrecision = double;
};

using Quaternion = math::detail::Quaternion<float>;
using DualQuaternion = math::detail::DualQuaternion<float>;

}  // namespace blender::math

/** \} */
