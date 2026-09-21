/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <vector>

namespace bsl {

/**
 * Container for the value of a constexpr.
 *
 * Note that we do not support smaller types (e.g. char, ushort, ...) because they might not exist
 * in the target language. Supporting them would mean having potentially 2 different result for the
 * same expression depending on whether or not it is constexpr.
 */
struct ConstexprValue {
  enum CompType {
    BOOL = 0x00,
    UINT = 0x10,
    INT = 0x20,
    FLOAT = 0x30,
  };

  enum Type {
    INVALID = 0,
    BOOL1 = CompType::BOOL | 0x01,
    BOOL2 = CompType::BOOL | 0x02,
    BOOL3 = CompType::BOOL | 0x03,
    BOOL4 = CompType::BOOL | 0x04,
    UINT1 = CompType::UINT | 0x11,
    UINT2 = CompType::UINT | 0x12,
    UINT3 = CompType::UINT | 0x13,
    UINT4 = CompType::UINT | 0x14,
    INT1 = CompType::INT | 0x21,
    INT2 = CompType::INT | 0x22,
    INT3 = CompType::INT | 0x23,
    INT4 = CompType::INT | 0x24,
    FLOAT1 = CompType::FLOAT | 0x31,
    FLOAT2 = CompType::FLOAT | 0x32,
    FLOAT3 = CompType::FLOAT | 0x33,
    FLOAT4 = CompType::FLOAT | 0x34,
  } type_ = INVALID;

  bool is_array() const
  {
    return values.size() > comp_len();
  }

  bool is_scalar() const
  {
    return comp_len() == 1;
  }

  int comp_len() const
  {
    return type_ & 0x0F;
  }

  CompType comp_type() const
  {
    return CompType(type_ & 0xF0);
  }

  static uint32_t cast_value(CompType from, CompType to, uint32_t value)
  {
    if (to == FLOAT && from != FLOAT) {
      return std::bit_cast<uint32_t>(float(value));
    }
    if (to != FLOAT && from == FLOAT) {
      return static_cast<int>(std::bit_cast<float>(value));
    }
    return value;
  }

  void convert_or_broadcast(Type dst, std::vector<uint32_t> &r) const
  {
    int dst_len = dst & 0x0F;
    CompType dst_comp = CompType(dst & 0xF0);
    r.resize(dst_len);

    if (is_array()) {
      /* Invalid cast. */
      return;
    }

    if (is_scalar()) {
      /* Broadcast. */
      r = std::vector<uint32_t>(dst_len, cast_value(comp_type(), dst_comp, values[0]));
      return;
    }

    if (comp_len() == dst_len) {
      /* Convert. */
      for (int i = 0; i < dst_len; ++i) {
        r[i] = cast_value(comp_type(), dst_comp, values[i]);
      }
      return;
    }
    /* Invalid cast. */
  }

  /* Returning bytes of the target type. */
  uint32_t convert_scalar(CompType dst_comp) const
  {
    if (is_array()) {
      /* Invalid cast. */
      return 0;
    }

    if (is_scalar()) {
      /* Convert. */
      return cast_value(comp_type(), dst_comp, values[0]);
    }
    /* Invalid cast. Trying to cast a non scalar into a scalar. */
    assert(0);
    return 0;
  }

  std::vector<uint32_t> values = {0};

  explicit ConstexprValue(int32_t s) : type_(INT1), values({uint32_t(s)}) {}
  explicit ConstexprValue(uint32_t s) : type_(UINT1), values({s}) {}
  explicit ConstexprValue(bool s) : type_(BOOL1), values({s}) {}
  explicit ConstexprValue(float s) : type_(FLOAT1), values({std::bit_cast<uint32_t>(s)}) {}

  explicit ConstexprValue(Type t, const ConstexprValue &x) : type_(t)
  {
    x.convert_or_broadcast(t, values);
  }

  explicit ConstexprValue(Type t, const ConstexprValue &x, const ConstexprValue &y)
      : type_(t), values({x.convert_scalar(comp_type()), y.convert_scalar(comp_type())})
  {
  }

  explicit ConstexprValue(Type t,
                          const ConstexprValue &x,
                          const ConstexprValue &y,
                          const ConstexprValue &z)
      : type_(t),
        values({x.convert_scalar(comp_type()),
                y.convert_scalar(comp_type()),
                z.convert_scalar(comp_type())})
  {
  }

  explicit ConstexprValue(Type t,
                          const ConstexprValue &x,
                          const ConstexprValue &y,
                          const ConstexprValue &z,
                          const ConstexprValue &w)
      : type_(t),
        values({x.convert_scalar(comp_type()),
                y.convert_scalar(comp_type()),
                z.convert_scalar(comp_type()),
                w.convert_scalar(comp_type())})
  {
  }

  /* --- Unary Arithmetic & Bitwise Operators --- */

  ConstexprValue operator+() const
  {
    return *this;
  }

  ConstexprValue operator-() const
  {
    return constexpr_op([](auto &&l, auto && /*r*/) { return -l; }, *this, *this);
  }

  ConstexprValue operator~() const
  {
    return constexpr_op([](auto &&l, auto && /*r*/) { return ~int(l); }, *this, *this);
  }

  ConstexprValue operator!() const
  {
    return constexpr_op([](auto &&l, auto && /*r*/) { return int(!int(l)); }, *this, *this);
  }

  // /* --- Pre/Post Increment & Decrement --- */

  ConstexprValue &operator++()
  {
    for (auto &v : values) {
      ++v;
    }
    return *this;
  }
  ConstexprValue &operator--()
  {
    for (auto &v : values) {
      --v;
    }
    return *this;
  }

  ConstexprValue operator++(int)
  {
    ConstexprValue tmp = *this;
    ++(*this);
    return tmp;
  }
  ConstexprValue operator--(int)
  {
    ConstexprValue tmp = *this;
    --(*this);
    return tmp;
  }

  /* --- Compound Assignment Operators --- */

#define COMPOUND_ASSIGN(op, ...) \
  ConstexprValue operator op(const ConstexprValue &rhs) \
  { \
    *this = constexpr_op([](auto &&l, auto &&r) { return __VA_ARGS__; }, *this, rhs); \
    return *this; \
  }

  COMPOUND_ASSIGN(+=, l + r)
  COMPOUND_ASSIGN(-=, l - r)
  COMPOUND_ASSIGN(*=, l *r)
  COMPOUND_ASSIGN(/=, l / r)
  /* Cast operands for int operations. Undefined operation are filtered upfront. */
  COMPOUND_ASSIGN(%=, cast_to_int_if_not_uint(l) % cast_to_int_if_not_uint(r))
  COMPOUND_ASSIGN(&=, cast_to_int_if_not_uint(l) & cast_to_int_if_not_uint(r))
  COMPOUND_ASSIGN(|=, cast_to_int_if_not_uint(l) | cast_to_int_if_not_uint(r))
  COMPOUND_ASSIGN(^=, cast_to_int_if_not_uint(l) ^ cast_to_int_if_not_uint(r))
  COMPOUND_ASSIGN(<<=, cast_to_int_if_not_uint(l) << cast_to_int_if_not_uint(r))
  COMPOUND_ASSIGN(>>=, cast_to_int_if_not_uint(l) >> cast_to_int_if_not_uint(r))

#undef COMPOUND_ASSIGN

  const uint32_t &comp_raw(int i) const
  {
    return values[std::min(i, int(values.size() - 1))];
  }
  uint32_t &comp_raw(int i)
  {
    return values[std::min(i, int(values.size() - 1))];
  }

  template<typename T> T comp_as(int i) const
  {
    return std::bit_cast<T>(comp_raw(i));
  }

  template<typename T> void set_as(int i, T v)
  {
    comp_raw(i) = std::bit_cast<uint32_t>(v);
  }

  template<typename T> T comp_cast(int i) const
  {
    switch (comp_type()) {
      case BOOL:
        return static_cast<T>(bool(std::bit_cast<int32_t>(comp_raw(i))));
      case UINT:
        return static_cast<T>(std::bit_cast<uint32_t>(comp_raw(i)));
      case INT:
        return static_cast<T>(std::bit_cast<int32_t>(comp_raw(i)));
      case FLOAT:
        return static_cast<T>(std::bit_cast<float>(comp_raw(i)));
    }
    return 0;
  }

  template<typename Func>
  static ConstexprValue constexpr_op(const Func &fn,
                                     const ConstexprValue &lhs,
                                     const ConstexprValue &rhs,
                                     const bool is_bool_result = false,
                                     const bool use_left_type = false)
  {
    const int dst_len = std::max(lhs.comp_len(), rhs.comp_len());
    const int promoted_comp = std::max(lhs.comp_type(), rhs.comp_type());
    const int dst_comp = is_bool_result ? BOOL : (use_left_type ? lhs.comp_type() : promoted_comp);
    ConstexprValue r{0};
    /* Type promotion. */
    r.type_ = ConstexprValue::Type(dst_comp | dst_len);
    r.values.resize(r.comp_len());

    if (lhs.is_array() && rhs.is_array()) {
      /* Error. */
      return r;
    }
    if (lhs.comp_len() == rhs.comp_len() || lhs.is_scalar() || rhs.is_scalar()) {
#define LOOP(A) \
  for (int i = 0; i < dst_len; ++i) { \
    A; \
  }

      switch (lhs.comp_type()) {
        case BOOL:
          switch (rhs.comp_type()) {
            case BOOL:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case UINT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<uint32_t>(i))));
              break;
            case INT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case FLOAT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<float>(i))));
              break;
          }
          break;
        case UINT:
          switch (rhs.comp_type()) {
            case BOOL:
              LOOP(r.set_as(i, fn(lhs.comp_as<uint32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case UINT:
              LOOP(r.set_as(i, fn(lhs.comp_as<uint32_t>(i), rhs.comp_as<uint32_t>(i))));
              break;
            case INT:
              LOOP(r.set_as(i, fn(lhs.comp_as<uint32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case FLOAT:
              LOOP(r.set_as(i, fn(lhs.comp_as<uint32_t>(i), rhs.comp_as<float>(i))));
              break;
          }
          break;
        case INT:
          switch (rhs.comp_type()) {
            case BOOL:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case UINT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<uint32_t>(i))));
              break;
            case INT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<int32_t>(i))));
              break;
            case FLOAT:
              LOOP(r.set_as(i, fn(lhs.comp_as<int32_t>(i), rhs.comp_as<float>(i))));
              break;
          }
          break;
        case FLOAT:
          switch (rhs.comp_type()) {
            case BOOL:
              LOOP(r.set_as(i, fn(lhs.comp_as<float>(i), rhs.comp_as<int32_t>(i))));
              break;
            case UINT:
              LOOP(r.set_as(i, fn(lhs.comp_as<float>(i), rhs.comp_as<uint32_t>(i))));
              break;
            case INT:
              LOOP(r.set_as(i, fn(lhs.comp_as<float>(i), rhs.comp_as<int32_t>(i))));
              break;
            case FLOAT:
              LOOP(r.set_as(i, fn(lhs.comp_as<float>(i), rhs.comp_as<float>(i))));
              break;
          }
          break;
      }

#undef LOOP

      return r;
    }
    /* Error */
    return r;
  }

  template<typename T> static constexpr auto cast_to_int_if_not_uint(T val)
  {
    if constexpr (std::is_same_v<T, unsigned int>) {
      return val;
    }
    else {
      return static_cast<int>(val);
    }
  }
};

template<typename Func> auto apply_integral(Func func, const auto &lhs, const auto &rhs)
{
  using T = std::decay_t<decltype(lhs)>;
  using U = std::decay_t<decltype(rhs)>;

  if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
    return func(lhs, rhs);
  }
  else {
    /* Invalid operands: operation require integral types. */
    assert(0);
    return 0;
  }
}

inline ConstexprValue operator+(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op([](auto &&l, auto &&r) { return l + r; }, lhs, rhs);
}

inline ConstexprValue operator-(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op([](auto &&l, auto &&r) { return l - r; }, lhs, rhs);
}

inline ConstexprValue operator*(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op([](auto &&l, auto &&r) { return l * r; }, lhs, rhs);
}

inline ConstexprValue operator/(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op([](auto &&l, auto &&r) { return l / r; }, lhs, rhs);
}

/* Cast result to int to allow bit-casting the result. */

inline ConstexprValue operator&&(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l && r); }, lhs, rhs, true);
}

inline ConstexprValue operator||(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l || r); }, lhs, rhs, true);
}

inline ConstexprValue operator==(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l == r); }, lhs, rhs, true);
}

inline ConstexprValue operator!=(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l != r); }, lhs, rhs, true);
}

inline ConstexprValue operator<(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l < r); }, lhs, rhs, true);
}

inline ConstexprValue operator<=(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l <= r); }, lhs, rhs, true);
}

inline ConstexprValue operator>(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l > r); }, lhs, rhs, true);
}

inline ConstexprValue operator>=(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) { return int(l >= r); }, lhs, rhs, true);
}

inline ConstexprValue operator%(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l % r; }, l, r);
      },
      lhs,
      rhs);
}

inline ConstexprValue operator&(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l & r; }, l, r);
      },
      lhs,
      rhs);
}

inline ConstexprValue operator|(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l | r; }, l, r);
      },
      lhs,
      rhs);
}

inline ConstexprValue operator^(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l ^ r; }, l, r);
      },
      lhs,
      rhs);
}

inline ConstexprValue operator<<(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l << r; }, l, r);
      },
      lhs,
      rhs,
      false,
      true);
}

inline ConstexprValue operator>>(const ConstexprValue &lhs, const ConstexprValue &rhs)
{
  return ConstexprValue::constexpr_op(
      [](auto &&l, auto &&r) {
        return apply_integral([](auto &&l, auto &&r) { return l >> r; }, l, r);
      },
      lhs,
      rhs,
      false,
      true);
}

inline bool contains_zero(const ConstexprValue &v)
{
  for (auto val : v.values) {
    if (val == 0) {
      return true;
    }
  }
  return false;
}

inline bool anyLessThan(const ConstexprValue &lhs, int rhs)
{
  for (auto val : lhs.values) {
    if (val < rhs) {
      return true;
    }
  }
  return false;
}

inline bool anyGreaterThanEqual(const ConstexprValue &lhs, int rhs)
{
  for (auto val : lhs.values) {
    if (val >= rhs) {
      return true;
    }
  }
  return false;
}

}  // namespace bsl
