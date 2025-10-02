/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Orthonormal rotation and orientation.
 *
 * A practical reminder:
 * - Forward is typically the positive Y direction in Blender.
 * - Up is typically the positive Z direction in Blender.
 * - Right is typically the positive X direction in Blender.
 * - Blender uses right handedness.
 * - For cross product, forward = thumb, up = index, right = middle finger.
 *
 * The basis changes for each space:
 * - Object: X-right, Y-forward, Z-up
 * - World: X-right, Y-forward, Z-up
 * - Armature Bone: X-right, Y-forward, Z-up (with forward being the root to tip direction)
 * - Curve Tangent-Space: X-left, Y-up, Z-forward
 */

#include <iosfwd>

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Axes
 * \{ */

/**
 * An enum class representing one of the 3 basis axes.
 * This is implemented using a class to allow operators and methods.
 * NOTE: While this represents a 3D axis it can still be used to generate 2D basis vectors.
 */
class Axis {
 public:
  enum class Value : int8_t {
    /* Must start at 0. Used as indices in tables and vectors. */
    X = 0,
    Y,
    Z,
  };

  constexpr static Value X = Value::X;
  constexpr static Value Y = Value::Y;
  constexpr static Value Z = Value::Z;

 private:
  Value axis_;

 public:
  Axis() = default;

  constexpr Axis(const Value axis) : axis_(axis) {};

  /** Convert an uppercase axis character 'X', 'Y' or 'Z' to an enum value. */
  constexpr static Axis from_char(char axis_char)
  {
    const Axis axis = static_cast<Value>(axis_char - 'X');
    BLI_assert(int(Value::X) <= axis.as_int() && axis.as_int() <= int(Value::Z));
    return axis;
  }

  /** Allow casting from DNA enums stored as short / int. */
  constexpr static Axis from_int(const int axis_int)
  {
    const Axis axis = static_cast<Value>(axis_int);
    BLI_assert(Axis::X <= axis && axis <= Axis::Z);
    return axis;
  }

  /* Allow usage in `switch()` statements and comparisons. */
  constexpr operator Value() const
  {
    return axis_;
  }

  constexpr int as_int() const
  {
    return int(axis_);
  }

  /** Avoid hell. */
  explicit operator bool() const = delete;

  friend std::ostream &operator<<(std::ostream &stream, const Axis axis);
};

/**
 * An enum class representing one of the 6 axis aligned direction.
 * This is implemented using a class to allow operators and methods.
 * NOTE: While this represents a 3D axis it can still be used to generate 2D basis vectors.
 */
class AxisSigned {
 public:
  enum class Value : int8_t {
    /* Match #eTrackToAxis_Modes */
    /* Must start at 0. Used as indices in tables and vectors. */
    X_POS = 0,
    Y_POS = 1,
    Z_POS = 2,
    X_NEG = 3,
    Y_NEG = 4,
    Z_NEG = 5,
  };

  constexpr static Value X_POS = Value::X_POS;
  constexpr static Value Y_POS = Value::Y_POS;
  constexpr static Value Z_POS = Value::Z_POS;
  constexpr static Value X_NEG = Value::X_NEG;
  constexpr static Value Y_NEG = Value::Y_NEG;
  constexpr static Value Z_NEG = Value::Z_NEG;

 private:
  Value axis_;

 public:
  AxisSigned() = default;

  constexpr AxisSigned(Value axis) : axis_(axis) {};
  constexpr AxisSigned(Axis axis) : axis_(from_int(axis.as_int())) {};

  /** Allow casting from DNA enums stored as short / int. */
  constexpr static AxisSigned from_int(int axis_int)
  {
    const AxisSigned axis = static_cast<Value>(axis_int);
    BLI_assert(AxisSigned::X_POS <= axis && axis <= AxisSigned::Z_NEG);
    return axis;
  }

  /** Return the axis without the sign. It changes the type whereas abs(axis) doesn't. */
  constexpr Axis axis() const
  {
    return Axis::from_int(this->as_int() % 3);
  }

  /** Return the opposing axis. */
  AxisSigned operator-() const
  {
    return from_int((this->as_int() + 3) % 6);
  }

  /** Return next enum value. */
  AxisSigned next_after() const
  {
    return from_int((this->as_int() + 1) % 6);
  }

  /** Allow usage in `switch()` statements and comparisons. */
  constexpr operator Value() const
  {
    return axis_;
  }

  constexpr int as_int() const
  {
    return int(axis_);
  }

  /** Returns -1 if axis is negative, 1 otherwise. */
  constexpr int sign() const
  {
    return is_negative() ? -1 : 1;
  }

  /** Returns true if axis is negative, false otherwise. */
  constexpr bool is_negative() const
  {
    return int(axis_) > int(Value::Z_POS);
  }

  /** Avoid hell. */
  explicit operator bool() const = delete;

  friend std::ostream &operator<<(std::ostream &stream, const AxisSigned axis);
};

constexpr bool operator<=(const Axis::Value a, const Axis::Value b)
{
  return int(a) <= int(b);
}

constexpr bool operator<=(const AxisSigned::Value a, const AxisSigned::Value b)
{
  return int(a) <= int(b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Axes Utilities
 * \{ */

template<> inline AxisSigned abs(const AxisSigned &axis)
{
  return axis.axis();
}

[[nodiscard]] inline int sign(const AxisSigned &axis)
{
  return axis.sign();
}

/**
 * Returns the cross direction from two basis direction using the right hand rule.
 * This is much faster than true cross product if the vectors are basis vectors.
 * Any ill-formed case will return a orthogonal axis to \a a but will also trigger an assert. It is
 * better to filter these cases upstream.
 */
[[nodiscard]] inline AxisSigned cross(const AxisSigned a, const AxisSigned b)
{
  BLI_assert_msg(abs(a) != abs(b), "Axes must not be colinear.");
  switch (a) {
    case AxisSigned::X_POS:
      switch (b) {
        case AxisSigned::X_POS:
          break; /* Ill-defined. */
        case AxisSigned::Y_POS:
          return AxisSigned::Z_POS;
        case AxisSigned::Z_POS:
          return AxisSigned::Y_NEG;
        case AxisSigned::X_NEG:
          break; /* Ill-defined. */
        case AxisSigned::Y_NEG:
          return AxisSigned::Z_NEG;
        case AxisSigned::Z_NEG:
          return AxisSigned::Y_POS;
      }
      break;
    case AxisSigned::Y_POS:
      switch (b) {
        case AxisSigned::X_POS:
          return AxisSigned::Z_NEG;
        case AxisSigned::Y_POS:
          break; /* Ill-defined. */
        case AxisSigned::Z_POS:
          return AxisSigned::X_POS;
        case AxisSigned::X_NEG:
          return AxisSigned::Z_POS;
        case AxisSigned::Y_NEG:
          break; /* Ill-defined. */
        case AxisSigned::Z_NEG:
          return AxisSigned::X_NEG;
      }
      break;
    case AxisSigned::Z_POS:
      switch (b) {
        case AxisSigned::X_POS:
          return AxisSigned::Y_POS;
        case AxisSigned::Y_POS:
          return AxisSigned::X_NEG;
        case AxisSigned::Z_POS:
          break; /* Ill-defined. */
        case AxisSigned::X_NEG:
          return AxisSigned::Y_NEG;
        case AxisSigned::Y_NEG:
          return AxisSigned::X_POS;
        case AxisSigned::Z_NEG:
          break; /* Ill-defined. */
      }
      break;
    case AxisSigned::X_NEG:
      switch (b) {
        case AxisSigned::X_POS:
          break; /* Ill-defined. */
        case AxisSigned::Y_POS:
          return AxisSigned::Z_NEG;
        case AxisSigned::Z_POS:
          return AxisSigned::Y_POS;
        case AxisSigned::X_NEG:
          break; /* Ill-defined. */
        case AxisSigned::Y_NEG:
          return AxisSigned::Z_POS;
        case AxisSigned::Z_NEG:
          return AxisSigned::Y_NEG;
      }
      break;
    case AxisSigned::Y_NEG:
      switch (b) {
        case AxisSigned::X_POS:
          return AxisSigned::Z_POS;
        case AxisSigned::Y_POS:
          break; /* Ill-defined. */
        case AxisSigned::Z_POS:
          return AxisSigned::X_NEG;
        case AxisSigned::X_NEG:
          return AxisSigned::Z_NEG;
        case AxisSigned::Y_NEG:
          break; /* Ill-defined. */
        case AxisSigned::Z_NEG:
          return AxisSigned::X_POS;
      }
      break;
    case AxisSigned::Z_NEG:
      switch (b) {
        case AxisSigned::X_POS:
          return AxisSigned::Y_NEG;
        case AxisSigned::Y_POS:
          return AxisSigned::X_POS;
        case AxisSigned::Z_POS:
          break; /* Ill-defined. */
        case AxisSigned::X_NEG:
          return AxisSigned::Y_POS;
        case AxisSigned::Y_NEG:
          return AxisSigned::X_NEG;
        case AxisSigned::Z_NEG:
          break; /* Ill-defined. */
      }
      break;
  }
  return a.next_after();
}

/** Create basis vector. */
template<typename T> T to_vector(const Axis axis)
{
  BLI_assert(axis.as_int() < T::type_length);
  T vec{};
  vec[axis.as_int()] = 1;
  return vec;
}

/** Create signed basis vector. */
template<typename T> T to_vector(const AxisSigned axis)
{
  BLI_assert(abs(axis) <= AxisSigned::from_int(T::type_length - 1));
  T vec{};
  vec[abs(axis).as_int()] = axis.is_negative() ? -1 : 1;
  return vec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CartesianBasis
 * \{ */

/**
 * An `blender::math::CartesianBasis` represents an orientation that is aligned with the basis
 * axes. This type of rotation is fast, precise and adds more meaning to the code that uses it.
 */
struct CartesianBasis {
  VecBase<AxisSigned, 3> axes = {AxisSigned::X_POS, AxisSigned::Y_POS, AxisSigned::Z_POS};

  CartesianBasis() = default;

  /**
   * Create an arbitrary basis orientation.
   * Handedness can be flipped but an axis cannot be present twice.
   */
  CartesianBasis(const AxisSigned x, const AxisSigned y, const AxisSigned z) : axes(x, y, z)
  {
    BLI_assert(abs(x) != abs(y));
    BLI_assert(abs(y) != abs(z));
    BLI_assert(abs(z) != abs(x));
  }

  const AxisSigned &x() const
  {
    return axes.x;
  }

  const AxisSigned &y() const
  {
    return axes.y;
  }

  const AxisSigned &z() const
  {
    return axes.z;
  }

  AxisSigned &x()
  {
    return axes.x;
  }

  AxisSigned &y()
  {
    return axes.y;
  }

  AxisSigned &z()
  {
    return axes.z;
  }

  friend std::ostream &operator<<(std::ostream &stream, const CartesianBasis &rot);
};

/**
 * Create an CartesianBasis using two orthogonal axes.
 * The third axis is chosen by right hand rule to follow blender coordinate system.
 * \a forward is Y axis in blender coordinate system.
 * \a up is Z axis in blender coordinate system.
 * \note \a forward and \a up must be different axes.
 */
[[nodiscard]] inline CartesianBasis from_orthonormal_axes(const AxisSigned forward,
                                                          const AxisSigned up)
{
  BLI_assert(math::abs(forward) != math::abs(up));
  return {cross(forward, up), forward, up};
}

/**
 * Create an CartesianBasis for converting from \a a orientation to \a b orientation.
 */
[[nodiscard]] inline CartesianBasis rotation_between(const CartesianBasis &a,
                                                     const CartesianBasis &b)
{
  CartesianBasis basis;
  basis.axes[abs(b.x()).as_int()] = (sign(b.x()) != sign(a.x())) ? -abs(a.x()) : abs(a.x());
  basis.axes[abs(b.y()).as_int()] = (sign(b.y()) != sign(a.y())) ? -abs(a.y()) : abs(a.y());
  basis.axes[abs(b.z()).as_int()] = (sign(b.z()) != sign(a.z())) ? -abs(a.z()) : abs(a.z());
  return basis;
}

/**
 * Create an CartesianBasis for converting from an \a a orientation defined only by its forward
 * vector to a \a b orientation defined only by its forward vector.
 * Rotation is given to be non flipped and deterministic.
 */
[[nodiscard]] inline CartesianBasis rotation_between(const AxisSigned a_forward,
                                                     const AxisSigned b_forward)
{
  /* Pick predictable next axis. */
  AxisSigned a_up = abs(a_forward.next_after());
  AxisSigned b_up = abs(b_forward.next_after());

  if (sign(a_forward) != sign(b_forward)) {
    /* Flip both axis (up and right) so resulting rotation matrix sign remains positive. */
    b_up = -b_up;
  }
  return rotation_between(from_orthonormal_axes(a_forward, a_up),
                          from_orthonormal_axes(b_forward, b_up));
}

template<typename T>
[[nodiscard]] inline VecBase<T, 3> transform_point(const CartesianBasis &basis,
                                                   const VecBase<T, 3> &v)
{
  VecBase<T, 3> result;
  result[basis.x().axis().as_int()] = basis.x().is_negative() ? -v[0] : v[0];
  result[basis.y().axis().as_int()] = basis.y().is_negative() ? -v[1] : v[1];
  result[basis.z().axis().as_int()] = basis.z().is_negative() ? -v[2] : v[2];
  return result;
}

/**
 * Return the inverse transformation represented by the given basis.
 * This is conceptually the equivalent to a rotation matrix transpose, but much faster.
 */
[[nodiscard]] inline CartesianBasis invert(const CartesianBasis &basis)
{
  /* Returns the column where the `axis` is found in. The sign is taken from the axis value. */
  auto search_axis = [](const CartesianBasis &basis, const Axis axis) {
    if (basis.x().axis() == axis) {
      return basis.x().is_negative() ? AxisSigned::X_NEG : AxisSigned::X_POS;
    }
    if (basis.y().axis() == axis) {
      return basis.y().is_negative() ? AxisSigned::Y_NEG : AxisSigned::Y_POS;
    }
    return basis.z().is_negative() ? AxisSigned::Z_NEG : AxisSigned::Z_POS;
  };
  CartesianBasis result;
  result.x() = search_axis(basis, Axis::X);
  result.y() = search_axis(basis, Axis::Y);
  result.z() = search_axis(basis, Axis::Z);
  return result;
}

/** \} */

}  // namespace blender::math
