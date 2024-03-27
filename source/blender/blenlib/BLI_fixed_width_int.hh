/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cmath>

#include "BLI_string_ref.hh"
#include "BLI_unroll.hh"

namespace blender::fixed_width_int {

/**
 * An unsigned fixed width integer.
 *
 * For some algorithms, the largest cross platform integer type (`uint64_t`) is not large enough.
 * Then one has the choice to use some big-integer implementation like the one from GMP or one can
 * use fixed-width-integers as implemented here.
 *
 * Internally, this type combines multiple smaller integers into a bigger integer.
 */
template<typename T, int S> struct UIntF {
  static_assert(std::is_unsigned_v<T>);
  static_assert(S >= 1);

  /**
   * Array of smaller integers that make up the bigger integer. The first element is the least
   * significant digit.
   */
  std::array<T, S> v;

  /** Allow default construction. Note that the value is not initialized in this case. */
  UIntF() = default;

  /** Construct from a specific integer. */
  explicit UIntF(uint64_t value);

  /** Construct from a string. */
  explicit UIntF(StringRefNull str, int base = 10);

  /** Convert to a normal integer. Note that this may lose digits. */
  explicit operator uint64_t() const;

  /** Convert to floating point. This may lose precision. */
  explicit operator double() const;
  explicit operator float() const;

/* See `BLI_fixed_width_int_str.hh`. */
#ifdef WITH_GMP
  /** Update value based on the integer encoded in the string. */
  void set_from_str(StringRefNull str, int base = 10);

  /** Convert to a string. */
  std::string to_string(int base = 10) const;
#endif
};

/**
 * A signed fixed width integer. It's mostly the same as #UIntF, but signed.
 */
template<typename T, int S> struct IntF {
  static_assert(std::is_unsigned_v<T>);
  static_assert(S >= 1);

  /**
   * Array of smaller integers that make up the bigger integer. The first element is the least
   * significant digit.
   */
  std::array<T, S> v;

  /** Allow default construction. Note that the value is not initialized in this case. */
  IntF() = default;

  /** Construct from a specific integer. */
  explicit IntF(int64_t value);

  /** Support casting unsigned to signed fixed-width-int. */
  explicit IntF(const UIntF<T, S> &value);

  /** Construct from a string. */
  explicit IntF(StringRefNull str, int base = 10);

  /** Convert to a normal integer. Note that this may lose digits. */
  explicit operator int64_t() const;

  /** Convert to floating point. This may lose precision. */
  explicit operator double() const;
  explicit operator float() const;

  /** Support casting from signed to unsigned fixed-width-int. */
  explicit operator UIntF<T, S>() const;

/* See `BLI_fixed_width_int_str.hh`. */
#ifdef WITH_GMP
  /** Update value based on the integer encoded in the string. */
  void set_from_str(const StringRefNull str, const int base = 10);

  /** Convert to a string. */
  std::string to_string(int base = 10) const;
#endif
};

template<typename T> struct DoubleUIntType {
  using type = void;
};
template<> struct DoubleUIntType<uint8_t> {
  using type = uint16_t;
};
template<> struct DoubleUIntType<uint16_t> {
  using type = uint32_t;
};
template<> struct DoubleUIntType<uint32_t> {
  using type = uint64_t;
};
#ifndef _MSC_VER
template<> struct DoubleUIntType<uint64_t> {
  using type = __uint128_t;
};
#endif

/** Maps unsigned integer types to a type that's twice the size. E.g. uint16_t to uint32_t. */
template<typename T> using double_uint_type = typename DoubleUIntType<T>::type;

using UInt64_8 = UIntF<uint8_t, 8>;
using UInt64_16 = UIntF<uint16_t, 4>;
using UInt64_32 = UIntF<uint32_t, 2>;

using Int64_8 = IntF<uint8_t, 8>;
using Int64_16 = IntF<uint16_t, 4>;
using Int64_32 = IntF<uint32_t, 2>;

using UInt128_8 = UIntF<uint8_t, 16>;
using UInt128_16 = UIntF<uint16_t, 8>;
using UInt128_32 = UIntF<uint32_t, 4>;
using UInt128_64 = UIntF<uint64_t, 2>;

using UInt256_8 = UIntF<uint8_t, 32>;
using UInt256_16 = UIntF<uint16_t, 16>;
using UInt256_32 = UIntF<uint32_t, 8>;
using UInt256_64 = UIntF<uint64_t, 4>;

using Int128_8 = IntF<uint8_t, 16>;
using Int128_16 = IntF<uint16_t, 8>;
using Int128_32 = IntF<uint32_t, 4>;
using Int128_64 = IntF<uint64_t, 2>;

using Int256_8 = IntF<uint8_t, 32>;
using Int256_16 = IntF<uint16_t, 16>;
using Int256_32 = IntF<uint32_t, 8>;
using Int256_64 = IntF<uint64_t, 4>;

#ifdef _MSC_VER
using UInt128 = UInt128_32;
using UInt256 = UInt256_32;
using Int128 = Int128_32;
using Int256 = Int256_32;
#else
using UInt128 = UInt128_64;
using UInt256 = UInt256_64;
using Int128 = Int128_64;
using Int256 = Int256_64;
#endif

template<typename T, int S> inline UIntF<T, S>::UIntF(const uint64_t value)
{
  constexpr int Count = std::min(S, int(sizeof(decltype(value)) / sizeof(T)));
  constexpr int BitsPerT = 8 * sizeof(T);

  for (int i = 0; i < Count; i++) {
    this->v[i] = T(value >> (BitsPerT * i));
  }
  for (int i = Count; i < S; i++) {
    this->v[i] = 0;
  }
}

template<typename T, int S> inline IntF<T, S>::IntF(const int64_t value)
{
  constexpr int Count = std::min(S, int(sizeof(decltype(value)) / sizeof(T)));
  constexpr int BitsPerT = 8 * sizeof(T);

  for (int i = 0; i < Count; i++) {
    this->v[i] = T(value >> (BitsPerT * i));
  }
  const T sign_extend_fill = value < 0 ? T(-1) : T(0);
  for (int i = Count; i < S; i++) {
    this->v[i] = sign_extend_fill;
  }
}

template<typename T, int S> inline IntF<T, S>::IntF(const UIntF<T, S> &value) : v(value.v) {}

template<typename T, int S> UIntF<T, S>::UIntF(const StringRefNull str, const int base)
{
  this->set_from_str(str, base);
}

template<typename T, int S> IntF<T, S>::IntF(const StringRefNull str, const int base)
{
  this->set_from_str(str, base);
}

template<typename T, int S> inline UIntF<T, S>::operator uint64_t() const
{
  constexpr int Count = std::min(S, int(sizeof(uint64_t) / sizeof(T)));
  constexpr int BitsPerT = 8 * sizeof(T);

  uint64_t result = 0;
  for (int i = 0; i < Count; i++) {
    result |= uint64_t(this->v[i]) << (BitsPerT * i);
  }
  return result;
}

template<typename T, int S> inline UIntF<T, S>::operator double() const
{
  double result = double(this->v[0]);
  for (int i = 1; i < S; i++) {
    const T a = this->v[i];
    if (a == 0) {
      continue;
    }
    result += ldexp(a, 8 * sizeof(T) * i);
  }
  return result;
}

template<typename T, int S> inline UIntF<T, S>::operator float() const
{
  return float(double(*this));
}

template<typename T, int S> inline IntF<T, S>::operator int64_t() const
{
  return int64_t(uint64_t(UIntF<T, S>(*this)));
}

template<typename T, int S> inline IntF<T, S>::operator double() const
{
  if (is_negative(*this)) {
    return -double(-*this);
  }
  double result = double(this->v[0]);
  for (int i = 1; i < S; i++) {
    const T a = this->v[i];
    if (a == 0) {
      continue;
    }
    result += ldexp(a, 8 * sizeof(T) * i);
  }
  return result;
}

template<typename T, int S> inline IntF<T, S>::operator float() const
{
  return float(double(*this));
}

template<typename T, int S> inline IntF<T, S>::operator UIntF<T, S>() const
{
  UIntF<T, S> result;
  result.v = this->v;
  return result;
}

/**
 * Adds two fixed-width-integer together using the standard addition with carry algorithm taught
 * in schools. The main difference is that the digits here are not 0 to 9, but 0 to max(T).
 *
 * Due to the design of two's-complement numbers, this works for signed and unsigned
 * fixed-width-integer. The overflow behavior is wrap-around.
 *
 * \param T: Type for individual digits.
 * \param T2: Integer type that is twice as large as T.
 * \param S: Number of digits of type T in each fixed-width-integer.
 */
template<typename T, typename T2, int S>
inline void generic_add(T *__restrict dst, const T *a, const T *b)
{
  constexpr int shift = 8 * sizeof(T);
  T2 carry = 0;
  unroll<S>([&](auto i) {
    const T2 ai = T2(a[i]);
    const T2 bi = T2(b[i]);
    const T2 ri = ai + bi + carry;
    dst[i] = T(ri);
    carry = ri >> shift;
  });
}

/**
 * Similar to #generic_add, but for subtraction.
 */
template<typename T, typename T2, int S>
inline void generic_sub(T *__restrict dst, const T *a, const T *b)
{
  T2 carry = 0;
  unroll<S>([&](auto i) {
    const T2 ai = T2(a[i]);
    const T2 bi = T2(b[i]);
    const T2 ri = ai - bi - carry;
    dst[i] = T(ri);
    carry = ri > ai;
  });
}

/** Similar to #generic_add, but for unsigned multiplication. */
template<typename T, typename T2, int S>
inline void generic_unsigned_mul(T *__restrict dst, const T *a, const T *b)
{
  constexpr int shift = 8 * sizeof(T);

  T2 r[S] = {};

  for (int i = 0; i < S; i++) {
    const T2 bi = T2(b[i]);
    T2 carry = 0;
    for (int j = 0; j < S - i; j++) {
      const T2 rji = T2(a[j]) * bi + carry;
      carry = rji >> shift;
      r[i + j] += T2(T(rji));
    }
  }

  T2 carry = 0;
  for (int i = 0; i < S; i++) {
    const T2 ri = r[i] + carry;
    carry = ri >> shift;
    dst[i] = T(ri);
  }
}

template<typename T, int Size, BLI_ENABLE_IF((!std::is_void_v<double_uint_type<T>>))>
inline UIntF<T, Size> operator+(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  UIntF<T, Size> result;
  generic_add<T, double_uint_type<T>, Size>(result.v.data(), a.v.data(), b.v.data());
  return result;
}

template<typename T, int Size, BLI_ENABLE_IF((!std::is_void_v<double_uint_type<T>>))>
inline IntF<T, Size> operator+(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  IntF<T, Size> result;
  generic_add<T, double_uint_type<T>, Size>(result.v.data(), a.v.data(), b.v.data());
  return result;
}

template<typename T, int Size>
inline UIntF<T, Size> operator-(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  UIntF<T, Size> result;
  generic_sub<T, double_uint_type<T>, Size>(result.v.data(), a.v.data(), b.v.data());
  return result;
}

template<typename T, int Size>
inline IntF<T, Size> operator-(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  IntF<T, Size> result;
  generic_sub<T, double_uint_type<T>, Size>(result.v.data(), a.v.data(), b.v.data());
  return result;
}

template<typename T, int Size, BLI_ENABLE_IF((!std::is_void_v<double_uint_type<T>>))>
inline UIntF<T, Size> operator*(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  UIntF<T, Size> result;
  generic_unsigned_mul<T, double_uint_type<T>, Size>(result.v.data(), a.v.data(), b.v.data());
  return result;
}

/**
 * Using this function is faster than using the comparison operator. Only a single bit has to be
 * checked to determine if the value is negative.
 */
template<typename T, int Size> bool is_negative(const IntF<T, Size> &a)
{
  return (a.v[Size - 1] & (T(1) << (sizeof(T) * 8 - 1))) != 0;
}

template<typename T, int Size> inline bool is_zero(const UIntF<T, Size> &a)
{
  bool result = true;
  unroll<Size>([&](auto i) { result &= (a.v[i] == 0); });
  return result;
}

template<typename T, int Size> inline bool is_zero(const IntF<T, Size> &a)
{
  bool result = true;
  unroll<Size>([&](auto i) { result &= (a.v[i] == 0); });
  return result;
}

template<typename T, int Size, BLI_ENABLE_IF((!std::is_void_v<double_uint_type<T>>))>
inline IntF<T, Size> operator*(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  using UIntF = UIntF<T, Size>;
  using IntF = IntF<T, Size>;

  /* Signed multiplication is implemented in terms of unsigned multiplication. */
  const bool is_negative_a = is_negative(a);
  const bool is_negative_b = is_negative(b);
  if (is_negative_a && is_negative_b) {
    return IntF(UIntF(-a) * UIntF(-b));
  }
  if (is_negative_a) {
    return -IntF(UIntF(-a) * UIntF(b));
  }
  if (is_negative_b) {
    return -IntF(UIntF(a) * UIntF(-b));
  }
  return IntF(UIntF(a) * UIntF(b));
}

template<typename T, int Size> inline IntF<T, Size> operator-(const IntF<T, Size> &a)
{
  IntF<T, Size> result;
  for (int i = 0; i < Size; i++) {
    result.v[i] = ~a.v[i];
  }
  return result + IntF<T, Size>(1);
}

template<typename T, int Size> inline void operator+=(UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  a = a + b;
}

template<typename T, int Size> inline void operator+=(IntF<T, Size> &a, const IntF<T, Size> &b)
{
  a = a + b;
}

template<typename T, int Size> inline void operator-=(UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  a = a - b;
}

template<typename T, int Size> inline void operator-=(IntF<T, Size> &a, const IntF<T, Size> &b)
{
  a = a - b;
}

template<typename T, int Size> inline void operator*=(UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  a = a * b;
}

template<typename T, int Size> inline void operator*=(IntF<T, Size> &a, const IntF<T, Size> &b)
{
  a = a * b;
}

template<typename T, int Size>
inline bool operator==(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  return a.v == b.v;
}

template<typename T, int Size>
inline bool operator==(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return a.v == b.v;
}

template<typename T, int Size>
inline bool operator!=(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  return a.v != b.v;
}

template<typename T, int Size>
inline bool operator!=(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return a.v != b.v;
}

template<typename T, size_t Size>
inline int compare_reversed_order(const std::array<T, Size> &a, const std::array<T, Size> &b)
{
  for (int i = Size - 1; i >= 0; i--) {
    if (a[i] < b[i]) {
      return -1;
    }
    if (a[i] > b[i]) {
      return 1;
    }
  }
  return 0;
}

template<typename T, int Size>
inline bool operator<(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  const bool is_negative_a = is_negative(a);
  const bool is_negative_b = is_negative(b);
  if (is_negative_a == is_negative_b) {
    return compare_reversed_order(a.v, b.v) < 0;
  }
  return is_negative_a;
}

template<typename T, int Size>
inline bool operator<=(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  const bool is_negative_a = is_negative(a);
  const bool is_negative_b = is_negative(b);
  if (is_negative_a == is_negative_b) {
    return compare_reversed_order(a.v, b.v) <= 0;
  }
  return is_negative_a;
}

template<typename T, int Size>
inline bool operator>(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  const bool is_negative_a = is_negative(a);
  const bool is_negative_b = is_negative(b);
  if (is_negative_a == is_negative_b) {
    return compare_reversed_order(a.v, b.v) > 0;
  }
  return is_negative_b;
}

template<typename T, int Size>
inline bool operator>=(const IntF<T, Size> &a, const IntF<T, Size> &b)
{
  const bool is_negative_a = is_negative(a);
  const bool is_negative_b = is_negative(b);
  if (is_negative_a == is_negative_b) {
    return compare_reversed_order(a.v, b.v) >= 0;
  }
  return is_negative_b;
}

template<typename T, int Size>
inline bool operator<(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return compare_reversed_order(a.v, b.v) < 0;
}

template<typename T, int Size>
inline bool operator<=(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return compare_reversed_order(a.v, b.v) <= 0;
}

template<typename T, int Size>
inline bool operator>(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return compare_reversed_order(a.v, b.v) > 0;
}

template<typename T, int Size>
inline bool operator>=(const UIntF<T, Size> &a, const UIntF<T, Size> &b)
{
  return compare_reversed_order(a.v, b.v) >= 0;
}

}  // namespace blender::fixed_width_int
