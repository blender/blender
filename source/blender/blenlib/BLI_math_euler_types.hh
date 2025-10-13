/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Euler rotations are represented as a triple of angle representing a rotation around each basis
 * vector. The order in which the three rotations are applied changes the resulting orientation.
 *
 * A `blender::math::EulerXYZ` represent an Euler triple with fixed axis order (XYZ).
 * A `blender::math::Euler3` represents an Euler triple with arbitrary axis order.
 *
 * They are prone to gimbal lock and are not suited for many applications. However they are more
 * intuitive than other rotation types. Their main use is for converting user facing rotation
 * values to other rotation types.
 *
 * The rotation values can still be reinterpreted like this:
 * `Euler3(float3(my_euler3_zyx_rot), EulerOrder::XYZ)`
 * This will swap the X and Z rotation order and will likely not produce the same rotation matrix.
 *
 * If the goal is to convert (keep the same orientation) to `Euler3` then you have to do an
 * assignment.
 * eg: `Euler3 my_euler(EulerOrder::XYZ); my_euler = my_quaternion:`
 */

#include <ostream>

#include "BLI_math_angle_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_basis_types.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender::math {

/* WARNING: must match the #eRotationModes in `DNA_action_types.h`
 * order matters - types are saved to file. */
enum EulerOrder {
  XYZ = 1,
  XZY,
  YXZ,
  YZX,
  ZXY,
  ZYX,
};

std::ostream &operator<<(std::ostream &stream, EulerOrder order);

/* -------------------------------------------------------------------- */
/** \name EulerBase
 * \{ */

template<typename T> struct EulerBase {
  using AngleT = AngleRadianBase<T>;

 protected:
  /**
   * Container for the rotation values. They are always stored as XYZ order.
   * Rotation values are stored without parity flipping.
   */
  VecBase<AngleT, 3> xyz_;

  EulerBase() = default;

  EulerBase(const AngleT &x, const AngleT &y, const AngleT &z) : xyz_(x, y, z) {};

  EulerBase(const VecBase<AngleT, 3> &vec) : xyz_(vec) {};

  EulerBase(const VecBase<T, 3> &vec) : xyz_(vec.x, vec.y, vec.z) {};

 public:
  /** Static functions. */

  /** Conversions. */

  explicit operator VecBase<AngleT, 3>() const
  {
    return this->xyz_;
  }

  explicit operator VecBase<T, 3>() const
  {
    return {T(x()), T(y()), T(z())};
  }

  /** Methods. */

  VecBase<AngleT, 3> &xyz()
  {
    return this->xyz_;
  }
  const VecBase<AngleT, 3> &xyz() const
  {
    return this->xyz_;
  }

  const AngleT &x() const
  {
    return this->xyz_.x;
  }

  const AngleT &y() const
  {
    return this->xyz_.y;
  }

  const AngleT &z() const
  {
    return this->xyz_.z;
  }

  AngleT &x()
  {
    return this->xyz_.x;
  }

  AngleT &y()
  {
    return this->xyz_.y;
  }

  AngleT &z()
  {
    return this->xyz_.z;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name EulerXYZ
 * \{ */

template<typename T> struct EulerXYZBase : public EulerBase<T> {
  using AngleT = AngleRadianBase<T>;

 public:
  EulerXYZBase() = default;

  /**
   * Create an euler x,y,z rotation from a triple of radian angle.
   */
  template<typename AngleU> EulerXYZBase(const VecBase<AngleU, 3> &vec) : EulerBase<T>(vec){};

  EulerXYZBase(const AngleT &x, const AngleT &y, const AngleT &z) : EulerBase<T>(x, y, z) {};

  /**
   * Create a rotation from an basis axis and an angle.
   * This sets a single component of the euler triple, the others are left to 0.
   */
  EulerXYZBase(const Axis axis, const AngleT &angle)
  {
    *this = identity();
    this->xyz_[axis] = angle;
  }

  /** Static functions. */

  static EulerXYZBase identity()
  {
    return {AngleRadianBase<T>::identity(),
            AngleRadianBase<T>::identity(),
            AngleRadianBase<T>::identity()};
  }

  /** Methods. */

  /**
   * Return this euler orientation but with angles wrapped inside [-pi..pi] range.
   */
  EulerXYZBase wrapped() const;

  /**
   * Return this euler orientation but wrapped around \a reference.
   *
   * This means the interpolation between the returned value and \a reference will always take the
   * shortest path. The angle between them will not be more than pi.
   */
  EulerXYZBase wrapped_around(const EulerXYZBase &reference) const;

  /** Operators. */

  friend EulerXYZBase operator-(const EulerXYZBase &a)
  {
    return {-a.xyz_.x, -a.xyz_.y, -a.xyz_.z};
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(EulerXYZBase, xyz_)

  friend std::ostream &operator<<(std::ostream &stream, const EulerXYZBase &rot)
  {
    return stream << "EulerXYZ" << static_cast<VecBase<T, 3>>(rot);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler3
 * \{ */

template<typename T> struct Euler3Base : public EulerBase<T> {
  using AngleT = AngleRadianBase<T>;

 private:
  /** Axes order from applying the rotation. */
  EulerOrder order_;

  /**
   * Swizzle structure allowing to rotation ordered assignment.
   */
  class Swizzle {
   private:
    Euler3Base &eul_;

   public:
    explicit Swizzle(Euler3Base &eul) : eul_(eul) {};

    Euler3Base &operator=(const VecBase<AngleT, 3> &angles)
    {
      eul_.xyz_.x = angles[eul_.i_index()];
      eul_.xyz_.y = angles[eul_.j_index()];
      eul_.xyz_.z = angles[eul_.k_index()];
      return eul_;
    }

    operator VecBase<AngleT, 3>() const
    {
      return {eul_.i(), eul_.j(), eul_.k()};
    }

    explicit operator VecBase<T, 3>() const
    {
      return {T(eul_.i()), T(eul_.j()), T(eul_.k())};
    }
  };

 public:
  Euler3Base() = delete;

  /**
   * Create an euler rotation with \a order rotation ordering
   * from a triple of radian angles in XYZ order.
   * eg: If \a order is `EulerOrder::ZXY` then `angles.z` will be the angle of the first rotation.
   */
  template<typename AngleU>
  Euler3Base(const VecBase<AngleU, 3> &angles_xyz, EulerOrder order)
      : EulerBase<T>(angles_xyz), order_(order){};

  Euler3Base(const AngleT &x, const AngleT &y, const AngleT &z, EulerOrder order)
      : EulerBase<T>(x, y, z), order_(order) {};

  /**
   * Create a rotation around a single euler axis and an angle.
   */
  Euler3Base(const Axis axis, AngleT angle, EulerOrder order) : EulerBase<T>(), order_(order)
  {
    this->xyz_[axis] = angle;
  }

  /**
   * Defines rotation order but not the rotation values.
   * Used for conversion from other rotation types.
   */
  Euler3Base(EulerOrder order) : order_(order) {};

  /** Methods. */

  const EulerOrder &order() const
  {
    return order_;
  }

  /**
   * Returns the rotations angle in rotation order.
   * eg: if rotation `order` is `YZX` then `i` is the `Y` rotation.
   */
  Swizzle ijk()
  {
    return Swizzle{*this};
  }
  VecBase<AngleT, 3> ijk() const
  {
    return {i(), j(), k()};
  }

  /**
   * Returns the rotations angle in rotation order.
   * eg: if rotation `order` is `YZX` then `i` is the `Y` rotation.
   */
  const AngleT &i() const
  {
    return this->xyz_[i_index()];
  }
  const AngleT &j() const
  {
    return this->xyz_[j_index()];
  }
  const AngleT &k() const
  {
    return this->xyz_[k_index()];
  }
  AngleT &i()
  {
    return this->xyz_[i_index()];
  }
  AngleT &j()
  {
    return this->xyz_[j_index()];
  }
  AngleT &k()
  {
    return this->xyz_[k_index()];
  }

  /**
   * Return this euler orientation but wrapped around \a reference.
   *
   * This means the interpolation between the returned value and \a reference will always take the
   * shortest path. The angle between them will not be more than pi.
   */
  Euler3Base wrapped_around(const Euler3Base &reference) const
  {
    return {VecBase<AngleT, 3>(EulerXYZBase<T>(this->xyz_).wrapped_around(reference.xyz_)),
            order_};
  }

  /** Operators. */

  friend Euler3Base operator-(const Euler3Base &a)
  {
    return {-a.xyz_, a.order_};
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(Euler3Base, xyz_, order_)

  friend std::ostream &operator<<(std::ostream &stream, const Euler3Base &rot)
  {
    return stream << "Euler3_" << rot.order_ << rot.xyz_;
  }

  /* Utilities for conversions and functions operating on Euler3.
   * This should be private in theory. */

  /**
   * Parity of axis permutation.
   * It is considered even if axes are not shuffled (X followed by Y which in turn followed by Z).
   * Return `true` if odd (shuffled) and `false` if even (non-shuffled).
   */
  bool parity() const
  {
    switch (order_) {
      default:
        BLI_assert_unreachable();
        return false;
      case XYZ:
      case ZXY:
      case YZX:
        return false;
      case XZY:
      case YXZ:
      case ZYX:
        return true;
    }
  }

  /**
   * Source Axis of the 1st axis rotation.
   */
  int i_index() const
  {
    switch (order_) {
      default:
        BLI_assert_unreachable();
        return 0;
      case XYZ:
      case XZY:
        return 0;
      case YXZ:
      case YZX:
        return 1;
      case ZXY:
      case ZYX:
        return 2;
    }
  }

  /**
   * Source Axis of the 2nd axis rotation.
   */
  int j_index() const
  {
    switch (order_) {
      default:
        BLI_assert_unreachable();
        return 0;
      case YXZ:
      case ZXY:
        return 0;
      case XYZ:
      case ZYX:
        return 1;
      case XZY:
      case YZX:
        return 2;
    }
  }

  /**
   * Source Axis of the 3rd axis rotation.
   */
  int k_index() const
  {
    switch (order_) {
      default:
        BLI_assert_unreachable();
        return 0;
      case YZX:
      case ZYX:
        return 0;
      case XZY:
      case ZXY:
        return 1;
      case XYZ:
      case YXZ:
        return 2;
    }
  }
};

/** \} */

using EulerXYZ = EulerXYZBase<float>;
using Euler3 = Euler3Base<float>;

}  // namespace blender::math
