/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

namespace blender::math {

namespace detail {

/**
 * Rotation Types
 *
 * It gives more semantic information allowing overloaded functions based on the rotation type.
 * It also prevent implicit cast from rotation to vector types.
 */

/* Forward declaration. */
template<typename T> struct AxisAngle;
template<typename T> struct Quaternion;

template<typename T> struct AngleRadian {
  T value;

  AngleRadian() = default;

  AngleRadian(const T &radian) : value(radian){};

  /** Static functions. */

  static AngleRadian identity()
  {
    return 0;
  }

  /** Conversions. */

  explicit operator T() const
  {
    return value;
  }

  /** Operators. */

  friend std::ostream &operator<<(std::ostream &stream, const AngleRadian &rot)
  {
    return stream << "AngleRadian(" << rot.value << ")";
  }
};

template<typename T> struct EulerXYZ {
  T x, y, z;

  EulerXYZ() = default;

  EulerXYZ(const T &x, const T &y, const T &z)
  {
    this->x = x;
    this->y = y;
    this->z = z;
  }

  EulerXYZ(const VecBase<T, 3> &vec) : EulerXYZ(UNPACK3(vec)){};

  /** Static functions. */

  static EulerXYZ identity()
  {
    return {0, 0, 0};
  }

  /** Conversions. */

  explicit operator VecBase<T, 3>() const
  {
    return {this->x, this->y, this->z};
  }

  explicit operator AxisAngle<T>() const;

  explicit operator Quaternion<T>() const;

  /** Operators. */

  friend std::ostream &operator<<(std::ostream &stream, const EulerXYZ &rot)
  {
    return stream << "EulerXYZ" << static_cast<VecBase<T, 3>>(rot);
  }
};

template<typename T = float> struct Quaternion {
  T x, y, z, w;

  Quaternion() = default;

  Quaternion(const T &x, const T &y, const T &z, const T &w)
  {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
  }

  Quaternion(const VecBase<T, 4> &vec) : Quaternion(UNPACK4(vec)){};

  /** Static functions. */

  static Quaternion identity()
  {
    return {1, 0, 0, 0};
  }

  /** Conversions. */

  explicit operator VecBase<T, 4>() const
  {
    return {this->x, this->y, this->z, this->w};
  }

  explicit operator EulerXYZ<T>() const;

  explicit operator AxisAngle<T>() const;

  /** Operators. */

  const T &operator[](int i) const
  {
    BLI_assert(i >= 0 && i < 4);
    return (&this->x)[i];
  }

  friend std::ostream &operator<<(std::ostream &stream, const Quaternion &rot)
  {
    return stream << "Quaternion" << static_cast<VecBase<T, 4>>(rot);
  }
};

template<typename T> struct AxisAngle {
  using vec3_type = VecBase<T, 3>;

 protected:
  vec3_type axis_ = {0, 1, 0};
  /** Store cosine and sine so rotation is cheaper and doesn't require atan2. */
  T angle_cos_ = 1;
  T angle_sin_ = 0;
  /**
   * Source angle for interpolation.
   * It might not be computed on creation, so the getter ensures it is updated.
   */
  T angle_ = 0;

  /**
   * A defaulted constructor would cause zero initialization instead of default initialization,
   * and not call the default member initializers.
   */
  explicit AxisAngle(){};

 public:
  /**
   * Create a rotation from an axis and an angle.
   * \note `axis` does not have to be normalized.
   * Use `AxisAngleNormalized` instead to skip normalization cost.
   */
  AxisAngle(const vec3_type &axis, T angle);

  /**
   * Create a rotation from 2 normalized vectors.
   * \note `from` and `to` must be normalized.
   */
  AxisAngle(const vec3_type &from, const vec3_type &to);

  /** Static functions. */

  static AxisAngle<T> identity()
  {
    return AxisAngle<T>();
  }

  /** Getters. */

  const vec3_type &axis() const
  {
    return axis_;
  }

  const T &angle() const
  {
    if (UNLIKELY(angle_ == T(0) && angle_cos_ != T(1))) {
      /* Angle wasn't computed by constructor. */
      const_cast<AxisAngle *>(this)->angle_ = math::atan2(angle_sin_, angle_cos_);
    }
    return angle_;
  }

  const T &angle_cos() const
  {
    return angle_cos_;
  }

  const T &angle_sin() const
  {
    return angle_sin_;
  }

  /** Conversions. */

  explicit operator Quaternion<T>() const;

  explicit operator EulerXYZ<T>() const;

  /** Operators. */

  friend bool operator==(const AxisAngle &a, const AxisAngle &b)
  {
    return (a.axis == b.axis) && (a.angle == b.angle);
  }

  friend bool operator!=(const AxisAngle &a, const AxisAngle &b)
  {
    return (a != b);
  }

  friend std::ostream &operator<<(std::ostream &stream, const AxisAngle &rot)
  {
    return stream << "AxisAngle(axis=" << rot.axis << ", angle=" << rot.angle << ")";
  }
};

/**
 * A version of AxisAngle that expects axis to be already normalized.
 * Implicitly cast back to AxisAngle.
 */
template<typename T> struct AxisAngleNormalized : public AxisAngle<T> {
  AxisAngleNormalized(const VecBase<T, 3> &axis, T angle);

  operator AxisAngle<T>() const
  {
    return *this;
  }
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

};  // namespace detail

template<typename U> struct AssertUnitEpsilon<detail::Quaternion<U>> {
  static constexpr U value = AssertUnitEpsilon<U>::value * 10;
};

/* Most common used types. */
using AngleRadian = math::detail::AngleRadian<float>;
using EulerXYZ = math::detail::EulerXYZ<float>;
using Quaternion = math::detail::Quaternion<float>;
using AxisAngle = math::detail::AxisAngle<float>;
using AxisAngleNormalized = math::detail::AxisAngleNormalized<float>;

}  // namespace blender::math

/** \} */
