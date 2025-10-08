/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Classes to represent rotation angles. They can be used as 2D rotation or as building blocks for
 * other rotation types.
 *
 * Each `blender::math::Angle***<T>` implements the same interface and can be swapped easily.
 * However, they differ in each operation's efficiency, storage size and the range or group of
 * angles that can be stored.
 *
 * This design allows some function overloads to be more efficient with certain types.
 */

#include <ostream>

#include "BLI_math_base.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender::math {

/**
 * A `blender::math::AngleRadianBase<T>` is a typical radian angle.
 * - Storage : `1 * sizeof(T)`
 * - Range : [-inf..inf]
 * - Fast : Everything not slow.
 * - Slow : `cos()`, `sin()`, `tan()`, `AngleRadian(cos, sin)`
 */
template<typename T> struct AngleRadianBase {
 private:
  T value_;

 public:
  AngleRadianBase() = default;

  AngleRadianBase(const T &radian) : value_(radian) {};
  explicit AngleRadianBase(const T &cos, const T &sin) : value_(math::atan2(sin, cos)) {};

  /** Static functions. */

  static AngleRadianBase identity()
  {
    return 0;
  }

  static AngleRadianBase from_degree(const T &degrees)
  {
    return degrees * T(numbers::pi / 180.0);
  }

  /** Conversions. */

  /* Return angle value in radian. */
  explicit operator T() const
  {
    return value_;
  }

  /* Return angle value in degree. */
  T degree() const
  {
    return value_ * T(180.0 / numbers::pi);
  }

  /* Return angle value in radian. */
  T radian() const
  {
    return value_;
  }

  /** Methods. */

  /**
   * Return the angle wrapped inside [-pi..pi] interval. Basically `(angle + pi) % 2pi - pi`.
   */
  AngleRadianBase wrapped() const
  {
    return math::mod_periodic(value_ + T(numbers::pi), T(2 * numbers::pi)) - T(numbers::pi);
  }

  /**
   * Return the angle wrapped inside [-pi..pi] interval around a \a reference .
   * Basically `(angle - reference + pi) % 2pi - pi + reference` .
   * This means the interpolation between the returned value and \a reference will always take the
   * shortest path.
   */
  AngleRadianBase wrapped_around(const AngleRadianBase &reference) const
  {
    return reference + (*this - reference).wrapped();
  }

  /** Operators. */

  friend AngleRadianBase operator+(const AngleRadianBase &a, const AngleRadianBase &b)
  {
    return a.value_ + b.value_;
  }

  friend AngleRadianBase operator-(const AngleRadianBase &a, const AngleRadianBase &b)
  {
    return a.value_ - b.value_;
  }

  friend AngleRadianBase operator*(const AngleRadianBase &a, const AngleRadianBase &b)
  {
    return a.value_ * b.value_;
  }

  friend AngleRadianBase operator/(const AngleRadianBase &a, const AngleRadianBase &b)
  {
    return a.value_ / b.value_;
  }

  friend AngleRadianBase operator-(const AngleRadianBase &a)
  {
    return -a.value_;
  }

  AngleRadianBase &operator+=(const AngleRadianBase &b) &
  {
    value_ += b.value_;
    return *this;
  }

  AngleRadianBase &operator-=(const AngleRadianBase &b) &
  {
    value_ -= b.value_;
    return *this;
  }

  AngleRadianBase &operator*=(const AngleRadianBase &b) &
  {
    value_ *= b.value_;
    return *this;
  }

  AngleRadianBase &operator/=(const AngleRadianBase &b) &
  {
    value_ /= b.value_;
    return *this;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(AngleRadianBase, value_)

  friend std::ostream &operator<<(std::ostream &stream, const AngleRadianBase &rot)
  {
    return stream << "AngleRadian(" << rot.value_ << ")";
  }
};

/**
 * A `blender::math::AngleCartesianBase<T>` stores the angle as cosine + sine tuple.
 * - Storage : `2 * sizeof(T)`
 * - Range : [-pi..pi]
 * - Fast : `cos()`, `sin()`, `tan()`, `AngleCartesian(cos, sin)`
 * - Slow : Everything not fast.
 * It is only useful for intermediate representation when converting to other rotation types (eg:
 * AxisAngle > Quaternion) and for creating rotations from 2D points. In general it offers an
 * advantage when trigonometric values of an angle are required but not directly the angle itself.
 * It is also a nice shortcut for using the trigonometric identities.
 */
template<typename T> struct AngleCartesianBase {
 private:
  T cos_;
  T sin_;

 public:
  AngleCartesianBase() = default;

  /**
   * Create an angle from a (x, y) position on the unit circle.
   */
  AngleCartesianBase(const T &x, const T &y) : cos_(x), sin_(y)
  {
    BLI_assert(math::abs(x * x + y * y - T(1)) < T(1e-4));
  }

  /**
   * Create an angle from a radian value.
   */
  explicit AngleCartesianBase(const T &radian)
      : AngleCartesianBase(math::cos(radian), math::sin(radian)) {};
  explicit AngleCartesianBase(const AngleRadianBase<T> &angle)
      : AngleCartesianBase(math::cos(angle.radian()), math::sin(angle.radian())) {};

  /** Static functions. */

  static AngleCartesianBase identity()
  {
    return {1, 0};
  }

  static AngleCartesianBase from_degree(const T &degrees)
  {
    return AngleCartesianBase(degrees * T(numbers::pi / 180.0));
  }

  /**
   * Create an angle from a (x, y) position on the 2D plane.
   * Fallback to identity if (x, y) is origin (0, 0).
   */
  static AngleCartesianBase from_point(const T &x, const T &y)
  {
    T norm = math::sqrt(x * x + y * y);
    if (norm == 0) {
      return identity();
    }
    return AngleCartesianBase(x / norm, y / norm);
  }

  /** Conversions. */

  /* Return angle value in radian. */
  explicit operator T() const
  {
    return math::atan2(sin_, cos_);
  }

  /* Return angle value in degree. */
  T degree() const
  {
    return T(*this) * T(180.0 / numbers::pi);
  }

  /* Return angle value in radian. */
  T radian() const
  {
    return T(*this);
  }

  /** Methods. */

  T cos() const
  {
    return cos_;
  }

  T sin() const
  {
    return sin_;
  }

  T tan() const
  {
    return sin_ / cos_;
  }

  /** Operators. */

  /**
   * NOTE: These use the trigonometric identities:
   * https://en.wikipedia.org/wiki/List_of_trigonometric_identities
   * (see Angle_sum_and_difference_identities, Multiple-angle_formulae and Half-angle_formulae)
   *
   * There are no identities for (arbitrary) product or quotient of angles.
   * Better leave these unimplemented to avoid accidentally using `atan` everywhere (which is the
   * purpose of this class).
   */

  friend AngleCartesianBase operator+(const AngleCartesianBase &a, const AngleCartesianBase &b)
  {
    return {a.cos_ * b.cos_ - a.sin_ * b.sin_, a.sin_ * b.cos_ + a.cos_ * b.sin_};
  }

  friend AngleCartesianBase operator-(const AngleCartesianBase &a, const AngleCartesianBase &b)
  {
    return {a.cos_ * b.cos_ + a.sin_ * b.sin_, a.sin_ * b.cos_ - a.cos_ * b.sin_};
  }

  friend AngleCartesianBase operator*(const AngleCartesianBase &a, const T &b)
  {
    if (b == T(2)) {
      return {a.cos_ * a.cos_ - a.sin_ * a.sin_, T(2) * a.sin_ * a.cos_};
    }
    if (b == T(3)) {
      return {T(4) * (a.cos_ * a.cos_ * a.cos_) - T(3) * a.cos_,
              T(3) * a.sin_ - T(4) * (a.sin_ * a.sin_ * a.sin_)};
    }
    BLI_assert_msg(0,
                   "Arbitrary angle product isn't supported with AngleCartesianBase<T> for "
                   "performance reason. Use AngleRadianBase<T> instead.");
    return identity();
  }

  friend AngleCartesianBase operator*(const T &b, const AngleCartesianBase &a)
  {
    return a * b;
  }

  friend AngleCartesianBase operator/(const AngleCartesianBase &a, const T &divisor)
  {
    if (divisor == T(2)) {
      /* Still costly but faster than using `atan()`. */
      AngleCartesianBase result = {math::sqrt((T(1) + a.cos_) / T(2)),
                                   math::sqrt((T(1) - a.cos_) / T(2))};
      /* Recover sign only for sine. Cosine of half angle is given to be positive or 0 since the
       * angle stored in #AngleCartesianBase is in the range [-pi..pi]. */
      /* TODO(fclem): Could use `copysign` here. */
      if (a.sin_ < T(0)) {
        result.sin_ = -result.sin_;
      }
      return result;
    }
    BLI_assert_msg(0,
                   "Arbitrary angle quotient isn't supported with AngleCartesianBase<T> for "
                   "performance reason. Use AngleRadianBase<T> instead.");
    return identity();
  }

  friend AngleCartesianBase operator-(const AngleCartesianBase &a)
  {
    return {a.cos_, -a.sin_};
  }

  AngleCartesianBase &operator+=(const AngleCartesianBase &b) &
  {
    *this = *this + b;
    return *this;
  }

  AngleCartesianBase &operator*=(const T &b) &
  {
    *this = *this * b;
    return *this;
  }

  AngleCartesianBase &operator-=(const AngleCartesianBase &b) &
  {
    *this = *this - b;
    return *this;
  }

  AngleCartesianBase &operator/=(const T &b) &
  {
    *this = *this / b;
    return *this;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(AngleCartesianBase, cos_, sin_)

  friend std::ostream &operator<<(std::ostream &stream, const AngleCartesianBase &rot)
  {
    return stream << "AngleCartesian(x=" << rot.cos_ << ", y=" << rot.sin_ << ")";
  }
};

/**
 * A `blender::math::AngleFraction<T>` stores a radian angle as quotient.
 * - Storage : `2 * sizeof(int64_t)`
 * - Range : [-INT64_MAX..INT64_MAX] but angle must be expressed as fraction (be in Q subset).
 * - Fast : Everything not slow.
 * - Slow : `cos()`, `sin()`, `tan()` for angles not optimized.
 *
 * It offers the best accuracy for fractions of Pi radian angles. For instance
 * `sin(AngleFraction::tau() * n - AngleFraction::pi() / 2)` will exactly return `-1` for any `n`
 * within [-INT_MAX..INT_MAX]. This holds true even with very high radian values.
 *
 * Arithmetic operators are relatively cheap (4 operations for addition, 2 for multiplication) but
 * not as cheap as a `AngleRadian`. Another nice property is that the `cos()` and `sin()` functions
 * give symmetric results around the circle.
 *
 * NOTE: Prefer converting to `blender::math::AngleCartesianBase<T>` if both `cos()` and `sin()`
 * are needed. This will save some computation.
 *
 * Any operation becomes undefined if either the numerator or the denominator overflows.
 *
 * The `T` template parameter only serves as type for the computed values like `cos()` or
 * `radian()`.
 */
template<typename T = float> struct AngleFraction {
 private:
  /**
   * The angle is stored as a fraction of pi.
   */
  int64_t numerator_;
  int64_t denominator_;

  /**
   * Constructor is left private as we do not want the user of this class to create invalid
   * fractions.
   */
  AngleFraction(int64_t numerator, int64_t denominator = 1)
      : numerator_(numerator), denominator_(denominator) {};

 public:
  /** Static functions. */

  static AngleFraction identity()
  {
    return {0};
  }

  static AngleFraction pi()
  {
    return {1};
  }

  static AngleFraction tau()
  {
    return {2};
  }

  /** Conversions. */

  /* Return angle value in degree. */
  T degree() const
  {
    return T(numerator_ * 180) / T(denominator_);
  }

  /* Return angle value in radian. */
  T radian() const
  {
    /* This can be refined at will. This tries to reduce the float precision error to a minimum. */
    const bool is_negative = numerator_ < 0;
    /* TODO jump table. */
    if (abs(numerator_) == denominator_ * 2) {
      return is_negative ? T(-numbers::pi * 2) : T(numbers::pi * 2);
    }
    if (abs(numerator_) == denominator_) {
      return is_negative ? T(-numbers::pi) : T(numbers::pi);
    }
    if (numerator_ == 0) {
      return T(0);
    }
    if (abs(numerator_) * 2 == denominator_) {
      return is_negative ? T(-numbers::pi * 0.5) : T(numbers::pi * 0.5);
    }
    if (abs(numerator_) * 4 == denominator_) {
      return is_negative ? T(-numbers::pi * 0.25) : T(numbers::pi * 0.25);
    }
    /* TODO(fclem): No idea if this is precise or not. Just doing something for now. */
    const int64_t number_of_pi = numerator_ / denominator_;
    const int64_t slice_numerator = numerator_ - number_of_pi * denominator_;
    T slice_of_pi;
    /* Avoid integer overflow. */
    /* TODO(fclem): This is conservative. Could find a better threshold. */
    if (slice_numerator > 0xFFFFFFFF || denominator_ > 0xFFFFFFFF) {
      /* Certainly loose precision. */
      slice_of_pi = T(numbers::pi) * slice_numerator / T(denominator_);
    }
    else {
      /* Pi as a fraction can be expressed as 80143857 / 25510582 with 15th digit of precision. */
      slice_of_pi = T(slice_numerator * 80143857) / T(denominator_ * 25510582);
    }
    /* If angle is inside [-pi..pi] range, `number_of_pi` is 0 and has no effect on precision. */
    return slice_of_pi + T(numbers::pi) * number_of_pi;
  }

  /** Methods. */

  /**
   * Return the angle wrapped inside [-pi..pi] interval. Basically `(angle + pi) % 2pi - pi`.
   */
  AngleFraction wrapped() const
  {
    if (abs(numerator_) <= denominator_) {
      return *this;
    }
    return {mod_periodic(numerator_ + denominator_, denominator_ * 2) - denominator_,
            denominator_};
  }

  /**
   * Return the angle wrapped inside [-pi..pi] interval around a \a reference .
   * Basically `(angle - reference + pi) % 2pi - pi + reference` .
   * This means the interpolation between the returned value and \a reference will always take the
   * shortest path.
   */
  AngleFraction wrapped_around(const AngleFraction &reference) const
  {
    return reference + (*this - reference).wrapped();
  }

  /** Operators. */

  /**
   * We only allow operations on fractions of pi.
   * So we cannot implement things like `AngleFraction::pi() + 1` or `AngleFraction::pi() * 0.5`.
   */

  friend AngleFraction operator+(const AngleFraction &a, const AngleFraction &b)
  {
    if (a.denominator_ == b.denominator_) {
      return {a.numerator_ + b.numerator_, a.denominator_};
    }
    return {(a.numerator_ * b.denominator_) + (b.numerator_ * a.denominator_),
            a.denominator_ * b.denominator_};
  }

  friend AngleFraction operator-(const AngleFraction &a, const AngleFraction &b)
  {
    return a + (-b);
  }

  friend AngleFraction operator*(const AngleFraction &a, const AngleFraction &b)
  {
    return {a.numerator_ * b.numerator_, a.denominator_ * b.denominator_};
  }

  friend AngleFraction operator/(const AngleFraction &a, const AngleFraction &b)
  {
    return a * AngleFraction(b.denominator_, b.numerator_);
  }

  friend AngleFraction operator*(const AngleFraction &a, const int64_t &b)
  {
    return a * AngleFraction(b);
  }

  friend AngleFraction operator/(const AngleFraction &a, const int64_t &b)
  {
    return a / AngleFraction(b);
  }

  friend AngleFraction operator*(const int64_t &a, const AngleFraction &b)
  {
    return AngleFraction(a) * b;
  }

  friend AngleFraction operator/(const int64_t &a, const AngleFraction &b)
  {
    return AngleFraction(a) / b;
  }

  friend AngleFraction operator+(const AngleFraction &a)
  {
    return a;
  }

  friend AngleFraction operator-(const AngleFraction &a)
  {
    return {-a.numerator_, a.denominator_};
  }

  AngleFraction &operator+=(const AngleFraction &b) &
  {
    return *this = *this + b;
  }

  AngleFraction &operator-=(const AngleFraction &b) &
  {
    return *this = *this - b;
  }

  AngleFraction &operator*=(const AngleFraction &b) &
  {
    return *this = *this * b;
  }

  AngleFraction &operator/=(const AngleFraction &b) &
  {
    return *this = *this / b;
  }

  AngleFraction &operator*=(const int64_t &b) &
  {
    return *this = *this * b;
  }

  AngleFraction &operator/=(const int64_t &b) &
  {
    return *this = *this / b;
  }

  friend bool operator==(const AngleFraction &a, const AngleFraction &b)
  {
    if (a.numerator_ == 0 && b.numerator_ == 0) {
      return true;
    }
    if (a.denominator_ == b.denominator_) {
      return a.numerator_ == b.numerator_;
    }
    return a.numerator_ * b.denominator_ == b.numerator_ * a.denominator_;
  }

  friend bool operator!=(const AngleFraction &a, const AngleFraction &b)
  {
    return !(a == b);
  }

  friend std::ostream &operator<<(std::ostream &stream, const AngleFraction &rot)
  {
    return stream << "AngleFraction(num=" << rot.numerator_ << ", denom=" << rot.denominator_
                  << ")";
  }

  operator AngleCartesianBase<T>() const
  {
    AngleFraction a = this->wrapped();
    BLI_assert(abs(a.numerator_) <= a.denominator_);
    BLI_assert(a.denominator_ > 0);

    /* By default, creating a circle from an integer: calling #sinf & #cosf on the fraction
     * doesn't create symmetrical values (because floats can't represent Pi exactly). Resolve this
     * when the rotation is calculated from a fraction by mapping the `numerator` to lower values
     * so X/Y values for points around a circle are exactly symmetrical, see #87779.
     *
     * Multiply both the `numerator` and `denominator` by 4 to ensure we can divide the circle
     * into 8 octants. For each octant, we then use symmetry and negation to bring the `numerator`
     * closer to the origin where precision is highest.
     */
    /* Save negative sign so we cane assume unsigned angle for the rest of the computation.. */
    const bool is_negative = a.numerator_ < 0;
    /* Multiply numerator the same as denominator. */
    a.numerator_ = abs(a.numerator_) * 4;
    /* Determine the octant. */
    const int64_t octant = a.numerator_ / a.denominator_;
    const int64_t rest = a.numerator_ - octant * a.denominator_;
    /* Ensure denominator is a multiple of 4. */
    a.denominator_ *= 4;

    /* TODO jump table. */
    T x, y;
    /* If rest is 0, the angle is an angle with precise value. */
    if (rest == 0) {
      switch (octant) {
        case 0:
        case 4:
          x = T(1);
          y = T(0);
          break;
        case 2:
          x = T(0);
          y = T(1);
          break;
        case 1:
        case 3:
          x = y = math::rcp(T(numbers::sqrt2));
          break;
        default:
          BLI_assert_unreachable();
      }
    }
    else {
      switch (octant) {
        case 4:
          /* -Pi or Pi case. */
        case 0:
          /* Primary octant, nothing to do. */
          break;
        case 1:
          /* Pi / 2 - angle. */
          a.numerator_ = a.denominator_ / 2 - a.numerator_;
          break;
        case 2:
          /* Angle - Pi / 2. */
          a.numerator_ = a.numerator_ - a.denominator_ / 2;
          break;
        case 3:
          /* Pi - angle. */
          a.numerator_ = a.denominator_ - a.numerator_;
          break;
        default:
          BLI_assert_unreachable();
      }
      /* Resulting angle should be oscillating in [0..pi/4] range. */
      BLI_assert(a.numerator_ >= 0 && a.numerator_ <= a.denominator_ / 4);
      T angle = T(numbers::pi) * (T(a.numerator_) / T(a.denominator_));
      x = math::cos(angle);
      y = math::sin(angle);
      /* Diagonal symmetry "unfolding". */
      if (ELEM(octant, 1, 2)) {
        std::swap(x, y);
      }
    }
    /* Y axis symmetry. */
    if (octant >= 2) {
      x = -x;
    }
    /* X axis symmetry. */
    if (is_negative) {
      y = -y;
    }
    return AngleCartesianBase<T>(x, y);
  }
};

template<typename T> T cos(const AngleRadianBase<T> &a)
{
  return cos(a.radian());
}
template<typename T> T sin(const AngleRadianBase<T> &a)
{
  return sin(a.radian());
}
template<typename T> T tan(const AngleRadianBase<T> &a)
{
  return tan(a.radian());
}

template<typename T> T cos(const AngleCartesianBase<T> &a)
{
  return a.cos();
}
template<typename T> T sin(const AngleCartesianBase<T> &a)
{
  return a.sin();
}
template<typename T> T tan(const AngleCartesianBase<T> &a)
{
  return a.tan();
}

template<typename T> T cos(const AngleFraction<T> &a)
{
  return cos(AngleCartesianBase<T>(a));
}
template<typename T> T sin(const AngleFraction<T> &a)
{
  return sin(AngleCartesianBase<T>(a));
}
template<typename T> T tan(const AngleFraction<T> &a)
{
  return tan(AngleCartesianBase<T>(a));
}

using AngleRadian = AngleRadianBase<float>;
using AngleCartesian = AngleCartesianBase<float>;

}  // namespace blender::math
