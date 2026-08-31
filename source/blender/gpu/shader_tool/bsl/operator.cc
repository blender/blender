/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_table.hh"

namespace bsl {

enum OperatorType : uint16_t {
  /* clang-format off */
  /* Operand Constraints (Bits 11..13) */
  OP_NUMERIC = 0b0000'1000'0000'0000,
  OP_INTEGER = 0b0001'0000'0000'0000,
  OP_BOOL    = 0b0010'0000'0000'0000,

  /* Operator Categories (Bits 0..5) */
  OP_ARITHMETIC = 0b0000'0000'0000'0001,
  OP_BITWISE    = 0b0000'0000'0000'0010,
  OP_SHIFT      = 0b0000'0000'0000'0100,
  OP_EQUALITY   = 0b0000'0000'0000'1000,
  OP_RELATIONAL = 0b0000'0000'0001'0000,
  OP_LOGICAL    = 0b0000'0000'0010'0000,
  OP_ASSIGN     = 0b0000'0000'0100'0000,

  /* clang-format on */

  OP_INVALID = 0b1111'1111'1111'1111,
};

static OperatorType to_operator_type(TokenType tok_type)
{
  switch (tok_type) {
    case Plus:
      return OperatorType(OP_NUMERIC | OP_ARITHMETIC);
    case Minus:
      return OperatorType(OP_NUMERIC | OP_ARITHMETIC);
    case Multiply:
      return OperatorType(OP_NUMERIC | OP_ARITHMETIC);
    case Divide:
      return OperatorType(OP_NUMERIC | OP_ARITHMETIC);
    case Increment:
      return OperatorType(OP_NUMERIC);
    case Decrement:
      return OperatorType(OP_NUMERIC);

    case Modulo:
      return OperatorType(OP_INTEGER | OP_ARITHMETIC);
    case And:
      return OperatorType(OP_INTEGER | OP_BITWISE);
    case Or:
      return OperatorType(OP_INTEGER | OP_BITWISE);
    case Xor:
      return OperatorType(OP_INTEGER | OP_BITWISE);
    case BitwiseNot:
      return OperatorType(OP_INTEGER);

    case LShift:
      return OperatorType(OP_INTEGER | OP_SHIFT);
    case RShift:
      return OperatorType(OP_INTEGER | OP_SHIFT);

    case Equal:
      return OperatorType(OP_EQUALITY);
    case NotEqual:
      return OperatorType(OP_EQUALITY);

    case LThan:
      return OperatorType(OP_NUMERIC | OP_RELATIONAL);
    case GThan:
      return OperatorType(OP_NUMERIC | OP_RELATIONAL);
    case LEqual:
      return OperatorType(OP_NUMERIC | OP_RELATIONAL);
    case GEqual:
      return OperatorType(OP_NUMERIC | OP_RELATIONAL);

    case Not:
      return OperatorType(OP_BOOL);
    case LogicalAnd:
      return OperatorType(OP_BOOL | OP_LOGICAL);
    case LogicalOr:
      return OperatorType(OP_BOOL | OP_LOGICAL);

    case Assign:
    case AssignAdd:
    case AssignSub:
    case AssignMul:
    case AssignDiv:
    case AssignLShift:
    case AssignRShift:
      return OperatorType(OP_ASSIGN);
    default:
      return OP_INVALID;
  }
}

namespace builtin {

ClassId binary_operator_return_type(SymbolClass *lhs, TokenType op, SymbolClass *rhs)
{
  using namespace builtin;
  /* Validate inputs and binary capability. */
  if (lhs->builtin_class == ClassId::Invalid || rhs->builtin_class == ClassId::Invalid) {
    return ClassId::Invalid;
  }

  const uint16_t l = uint16_t(lhs->builtin_class);
  const uint16_t r = uint16_t(rhs->builtin_class);
  const OperatorType o = to_operator_type(op);

  if (o == OP_ASSIGN) {
    return lhs->builtin_class;
  }

  if (l & MATRIX_MASK) {
    uint16_t col = (l & MATRIX_MASK) >> MATRIX_SHIFT;
    uint16_t row = l & LEN_MASK;
    return (r == (CLASS_FLOAT | col)) ? ClassId(CLASS_FLOAT | row) : ClassId::Invalid;
  }
  if (r & MATRIX_MASK) {
    uint16_t col = (r & MATRIX_MASK) >> MATRIX_SHIFT;
    uint16_t row = r & LEN_MASK;
    return (l == (CLASS_FLOAT | row)) ? ClassId(CLASS_FLOAT | col) : ClassId::Invalid;
  }

  /* Logical operators (&&, ||): scalar bool only. */
  if (o & OP_LOGICAL) {
    return (l == ClassId::bool_t && r == ClassId::bool_t) ? ClassId::bool_t : ClassId::Invalid;
  }

  /* Enforce scalar base category (Numeric vs Integer). */
  if ((o & OP_NUMERIC) && (!(l & FLAG_NUMERIC) || !(r & FLAG_NUMERIC))) {
    return ClassId::Invalid;
  }

  if ((o & OP_INTEGER) && (!(l & FLAG_INTEGER) || !(r & FLAG_INTEGER))) {
    return ClassId::Invalid;
  }

  const uint16_t l_len = l & LEN_MASK;
  const uint16_t r_len = r & LEN_MASK;
  assert(l_len != 0 && r_len != 0);

  /* Bitwise Shifts (<<, >>): RHS must be scalar or match LHS length. */
  if (o & OP_SHIFT) {
    return (r_len == 1 || r_len == l_len) ? ClassId(l) : ClassId::Invalid;
  }

  /* Equality (==, !=): Scalars of exact matching base types only. */
  if (o & OP_EQUALITY) {
    /* Comparing float to int is not allowed. */
    if ((l & FLAG_INTEGER) != (r & FLAG_INTEGER)) {
      return ClassId::Invalid;
    }
    /* Comparing integers of different signedness is not allowed. */
    if ((l & FLAG_SIGN) != (r & FLAG_SIGN)) {
      /* TODO. Codebase still uses this. Patch it all at once. */
      // return ClassId::Invalid;
    }
    return (l_len == 1 && r_len == 1) ? ClassId::bool_t : ClassId::Invalid;
  }

  /* Relational (<, >, <=, >=): Scalar numerics only. */
  if (o & OP_RELATIONAL) {
    return (l_len == 1 && r_len == 1) ? ClassId::bool_t : ClassId::Invalid;
  }

  uint16_t target_len = l_len;
  if (l_len != r_len) {
    /* Vector Dimension Promotion (Scalar-Vector / Vector-Scalar). */
    if (l_len == 1) {
      target_len = r_len;
    }
    else if (r_len == 1) {
      target_len = l_len;
    }
    else {
      return ClassId::Invalid;
    }
  }

  /* Base Type Promotion: Higher rank base wins. */
  const uint16_t l_rank = l & RANK_MASK;
  const uint16_t r_rank = r & RANK_MASK;
  const uint16_t promoted_base = (l_rank > r_rank) ? (l & BASE_MASK) : (r & BASE_MASK);

  return static_cast<ClassId>(promoted_base | target_len);
}

ClassId unary_prefix_operator_return_type(TokenType op, SymbolClass *rhs)
{
  using namespace builtin;
  const uint16_t r = uint16_t(rhs->builtin_class);
  const OperatorType o = to_operator_type(op);

  if (r == ClassId::Invalid) {
    return ClassId::Invalid;
  }

  if (o & OP_BOOL) {
    return (r == ClassId::bool_t) ? ClassId::bool_t : ClassId::Invalid;
  }
  if ((o & OP_NUMERIC) && !(r & FLAG_NUMERIC)) {
    return ClassId::Invalid;
  }
  if ((o & OP_INTEGER) && !(r & FLAG_INTEGER)) {
    return ClassId::Invalid;
  }

  return rhs->builtin_class;
}

ClassId unary_suffix_operator_return_type(SymbolClass *lhs, TokenType op)
{
  using namespace builtin;
  const uint16_t l = uint16_t(lhs->builtin_class);
  const OperatorType o = to_operator_type(op);

  if (l == ClassId::Invalid) {
    return ClassId::Invalid;
  }

  if ((o & OP_NUMERIC) && !(l & FLAG_NUMERIC)) {
    return ClassId::Invalid;
  }

  return lhs->builtin_class;
}

}  // namespace builtin

SymbolClass *SymbolTable::to_class(builtin::ClassId id) const
{
  switch (id) {
    case builtin::ClassId::bool_t:
      return bool_cls;
    case builtin::ClassId::bool2_t:
      return bool2_cls;
    case builtin::ClassId::bool3_t:
      return bool3_cls;
    case builtin::ClassId::bool4_t:
      return bool4_cls;
    case builtin::ClassId::char_t:
      return char_cls;
    case builtin::ClassId::char2_t:
      return char2_cls;
    case builtin::ClassId::char3_t:
      return char3_cls;
    case builtin::ClassId::char4_t:
      return char4_cls;
    case builtin::ClassId::float_t:
      return float_cls;
    case builtin::ClassId::float2_t:
      return float2_cls;
    case builtin::ClassId::float3_t:
      return float3_cls;
    case builtin::ClassId::float4_t:
      return float4_cls;
    case builtin::ClassId::half_t:
      return half_cls;
    case builtin::ClassId::half2_t:
      return half2_cls;
    case builtin::ClassId::half3_t:
      return half3_cls;
    case builtin::ClassId::half4_t:
      return half4_cls;
    case builtin::ClassId::int_t:
      return int_cls;
    case builtin::ClassId::int2_t:
      return int2_cls;
    case builtin::ClassId::int3_t:
      return int3_cls;
    case builtin::ClassId::int4_t:
      return int4_cls;
    case builtin::ClassId::short_t:
      return short_cls;
    case builtin::ClassId::short2_t:
      return short2_cls;
    case builtin::ClassId::short3_t:
      return short3_cls;
    case builtin::ClassId::short4_t:
      return short4_cls;
    case builtin::ClassId::uchar_t:
      return uchar_cls;
    case builtin::ClassId::uchar2_t:
      return uchar2_cls;
    case builtin::ClassId::uchar3_t:
      return uchar3_cls;
    case builtin::ClassId::uchar4_t:
      return uchar4_cls;
    case builtin::ClassId::uint_t:
      return uint_cls;
    case builtin::ClassId::uint2_t:
      return uint2_cls;
    case builtin::ClassId::uint3_t:
      return uint3_cls;
    case builtin::ClassId::uint4_t:
      return uint4_cls;
    case builtin::ClassId::ushort_t:
      return ushort_cls;
    case builtin::ClassId::ushort2_t:
      return ushort2_cls;
    case builtin::ClassId::ushort3_t:
      return ushort3_cls;
    case builtin::ClassId::ushort4_t:
      return ushort4_cls;
    case builtin::ClassId::float2x2_t:
      return float2x2_cls;
    case builtin::ClassId::float2x3_t:
      return float2x3_cls;
    case builtin::ClassId::float2x4_t:
      return float2x4_cls;
    case builtin::ClassId::float3x2_t:
      return float3x2_cls;
    case builtin::ClassId::float3x3_t:
      return float3x3_cls;
    case builtin::ClassId::float3x4_t:
      return float3x4_cls;
    case builtin::ClassId::float4x2_t:
      return float4x2_cls;
    case builtin::ClassId::float4x3_t:
      return float4x3_cls;
    case builtin::ClassId::float4x4_t:
      return float4x4_cls;
    default:
      return err_cls;
  }
}

SymbolClass *SymbolTable::make_type(SymbolClass *base, int size)
{
  /* Size 1 corresponds to the scalar base type itself. */
  if (size == 1) {
    return base;
  }

  if (base == float_cls) {
    switch (size) {
      case 2:
        return float2_cls;
      case 3:
        return float3_cls;
      case 4:
        return float4_cls;
      default:
        break;
    }
  }
  else if (base == int_cls) {
    switch (size) {
      case 2:
        return int2_cls;
      case 3:
        return int3_cls;
      case 4:
        return int4_cls;
      default:
        break;
    }
  }
  else if (base == uint_cls) {
    switch (size) {
      case 2:
        return uint2_cls;
      case 3:
        return uint3_cls;
      case 4:
        return uint4_cls;
      default:
        break;
    }
  }
  else if (base == bool_cls) {
    switch (size) {
      case 2:
        return bool2_cls;
      case 3:
        return bool3_cls;
      case 4:
        return bool4_cls;
      default:
        break;
    }
  }
  else if (base == half_cls) {
    switch (size) {
      case 2:
        return half2_cls;
      case 3:
        return half3_cls;
      case 4:
        return half4_cls;
      default:
        break;
    }
  }
  else if (base == char_cls) {
    switch (size) {
      case 2:
        return char2_cls;
      case 3:
        return char3_cls;
      case 4:
        return char4_cls;
      default:
        break;
    }
  }
  else if (base == uchar_cls) {
    switch (size) {
      case 2:
        return uchar2_cls;
      case 3:
        return uchar3_cls;
      case 4:
        return uchar4_cls;
      default:
        break;
    }
  }
  else if (base == short_cls) {
    switch (size) {
      case 2:
        return short2_cls;
      case 3:
        return short3_cls;
      case 4:
        return short4_cls;
      default:
        break;
    }
  }
  else if (base == ushort_cls) {
    switch (size) {
      case 2:
        return ushort2_cls;
      case 3:
        return ushort3_cls;
      case 4:
        return ushort4_cls;
      default:
        break;
    }
  }
  else if (base == float2_cls) {
    switch (size) {
      case 2:
        return float2x2_cls;
      case 3:
        return float3x2_cls;
      case 4:
        return float4x2_cls;
      default:
        break;
    }
  }
  else if (base == float3_cls) {
    switch (size) {
      case 2:
        return float2x3_cls;
      case 3:
        return float3x3_cls;
      case 4:
        return float4x3_cls;
      default:
        break;
    }
  }
  else if (base == float4_cls) {
    switch (size) {
      case 2:
        return float2x4_cls;
      case 3:
        return float3x4_cls;
      case 4:
        return float4x4_cls;
      default:
        break;
    }
  }
  return nullptr;
}

/* Generates all scalar and vector types (e.g. int, int2, int3, int4). */
vector<SymbolTable::BuiltinType> SymbolTable::generate_builtin_types()
{
  vector<BuiltinType> types{
      {char_cls, char_cls},    {short_cls, short_cls},    {int_cls, int_cls},
      {uchar_cls, uchar_cls},  {ushort_cls, ushort_cls},  {uint_cls, uint_cls},
      {half_cls, half_cls},    {float_cls, float_cls},

      {char2_cls, char_cls},   {short2_cls, short_cls},   {int2_cls, int_cls},
      {uchar2_cls, uchar_cls}, {ushort2_cls, ushort_cls}, {uint2_cls, uint_cls},
      {half2_cls, half_cls},   {float2_cls, float_cls},

      {char3_cls, char_cls},   {short3_cls, short_cls},   {int3_cls, int_cls},
      {uchar3_cls, uchar_cls}, {ushort3_cls, ushort_cls}, {uint3_cls, uint_cls},
      {half3_cls, half_cls},   {float3_cls, float_cls},

      {char4_cls, char_cls},   {short4_cls, short_cls},   {int4_cls, int_cls},
      {uchar4_cls, uchar_cls}, {ushort4_cls, ushort_cls}, {uint4_cls, uint_cls},
      {half4_cls, half_cls},   {float4_cls, float_cls},
  };
  return types;
}

}  // namespace bsl
