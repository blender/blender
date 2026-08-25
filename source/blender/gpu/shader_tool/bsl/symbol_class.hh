/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "resource.hh"
#include "symbol_scope.hh"
#include "symbol_template.hh"

namespace bsl {

namespace builtin {

enum ClassFlag : uint16_t {
  /* Bitfield Masks & Flags (16-bit layout) */
  RANK_SHIFT = 3,
  MATRIX_SHIFT = 9,
  /* clang-format off */
  LEN_MASK     = 0b0000'0000'0000'0111, /* Bits 0..2: Component length (1..4). */
  RANK_MASK    = 0b0000'0000'0111'1000, /* Bits 3..6: Type promotion rank. */
  FLAG_NUMERIC = 0b0000'0000'1000'0000, /* Bit 7:  Numeric flag. */
  FLAG_INTEGER = 0b0000'0001'0000'0000, /* Bit 8:  Integer flag. */
  MATRIX_MASK  = 0b0000'1110'0000'0000, /* Bit 9..11: Matrix column length (2..4). */
  FLAG_SIGN    = 0b0001'0000'0000'0000, /* Bit 12: Signedness bit. */
  /* clang-format on */

  BASE_MASK = FLAG_NUMERIC | FLAG_INTEGER | MATRIX_MASK | FLAG_SIGN | RANK_MASK,

  /* Base Type Encodings */
  CLASS_BOOL = (0 << RANK_SHIFT),
  CLASS_CHAR = (1 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER | FLAG_SIGN,
  CLASS_UCHAR = (2 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER,
  CLASS_SHORT = (3 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER | FLAG_SIGN,
  CLASS_USHORT = (4 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER,
  CLASS_INT = (5 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER | FLAG_SIGN,
  CLASS_UINT = (6 << RANK_SHIFT) | FLAG_NUMERIC | FLAG_INTEGER,
  CLASS_HALF = (7 << RANK_SHIFT) | FLAG_NUMERIC,
  CLASS_FLOAT = (8 << RANK_SHIFT) | FLAG_NUMERIC,
  CLASS_MAT = (9 << RANK_SHIFT),
};

enum ClassId : uint16_t {

  /* Concrete Type Identifiers */
  bool_t = CLASS_BOOL | 1,
  bool2_t = CLASS_BOOL | 2,
  bool3_t = CLASS_BOOL | 3,
  bool4_t = CLASS_BOOL | 4,

  char_t = CLASS_CHAR | 1,
  char2_t = CLASS_CHAR | 2,
  char3_t = CLASS_CHAR | 3,
  char4_t = CLASS_CHAR | 4,

  float_t = CLASS_FLOAT | 1,
  float2_t = CLASS_FLOAT | 2,
  float3_t = CLASS_FLOAT | 3,
  float4_t = CLASS_FLOAT | 4,

  half_t = CLASS_HALF | 1,
  half2_t = CLASS_HALF | 2,
  half3_t = CLASS_HALF | 3,
  half4_t = CLASS_HALF | 4,

  int_t = CLASS_INT | 1,
  int2_t = CLASS_INT | 2,
  int3_t = CLASS_INT | 3,
  int4_t = CLASS_INT | 4,

  short_t = CLASS_SHORT | 1,
  short2_t = CLASS_SHORT | 2,
  short3_t = CLASS_SHORT | 3,
  short4_t = CLASS_SHORT | 4,

  uchar_t = CLASS_UCHAR | 1,
  uchar2_t = CLASS_UCHAR | 2,
  uchar3_t = CLASS_UCHAR | 3,
  uchar4_t = CLASS_UCHAR | 4,

  uint_t = CLASS_UINT | 1,
  uint2_t = CLASS_UINT | 2,
  uint3_t = CLASS_UINT | 3,
  uint4_t = CLASS_UINT | 4,

  ushort_t = CLASS_USHORT | 1,
  ushort2_t = CLASS_USHORT | 2,
  ushort3_t = CLASS_USHORT | 3,
  ushort4_t = CLASS_USHORT | 4,

  float2x2_t = CLASS_MAT | (2 << MATRIX_SHIFT) | 2,
  float2x3_t = CLASS_MAT | (2 << MATRIX_SHIFT) | 3,
  float2x4_t = CLASS_MAT | (2 << MATRIX_SHIFT) | 4,
  float3x2_t = CLASS_MAT | (3 << MATRIX_SHIFT) | 2,
  float3x3_t = CLASS_MAT | (3 << MATRIX_SHIFT) | 3,
  float3x4_t = CLASS_MAT | (3 << MATRIX_SHIFT) | 4,
  float4x2_t = CLASS_MAT | (4 << MATRIX_SHIFT) | 2,
  float4x3_t = CLASS_MAT | (4 << MATRIX_SHIFT) | 3,
  float4x4_t = CLASS_MAT | (4 << MATRIX_SHIFT) | 4,

  Invalid
};

builtin::ClassId binary_operator_return_type(SymbolClass *lhs, TokenType op, SymbolClass *rhs);
builtin::ClassId unary_prefix_operator_return_type(TokenType op, SymbolClass *rhs);
builtin::ClassId unary_suffix_operator_return_type(SymbolClass *lhs, TokenType op);

}  // namespace builtin

/**
 * Container for a class symbol and its metadata.
 */
struct SymbolClass : SymbolScope {
  ResourceTableType srt_type = ResourceTableType::NONE;

  SymbolClassTemplate *template_data = nullptr;

  SymbolFunction *operator_subscript = nullptr;

  int size = -1;  /* -1 is for not computed yet. */
  int align = -1; /* -1 is for not computed yet. */

  builtin::ClassId builtin_class = builtin::ClassId::Invalid;

  /* For builtin types, number of components. */
  int8_t comp_len = 0;

  bool is_anonymous = false;
  bool is_union = false;
  bool is_enum = false;
  bool is_std140_compatible = false;
  bool is_std430_compatible = false;
  bool is_error = false;
  /* e.g. image2D, sampler2d, string_t ... */
  bool is_opaque = false;

  SymbolClass(SymbolScope *parent, ast::ClassDecl decl, const string &suffix = "");

  SymbolClass(SymbolScope *parent,
              Token tok,
              const string &builtin_id,
              int size_in_bytes = 1,
              int align_in_bytes = 1,
              builtin::ClassId builtin_class = builtin::ClassId::Invalid)
      : SymbolScope(parent, tok, builtin_id, CLASS),
        size(size_in_bytes),
        align(align_in_bytes),
        builtin_class(builtin_class)
  {
  }

  void ensure_size_and_align(NodeErrorHandler *err_handler = nullptr, bool check_st430 = false);

  bool is_srt() const
  {
    return srt_type != ResourceTableType::NONE;
  }

  bool is_builtin() const
  {
    return builtin_class != builtin::ClassId::Invalid;
  }

  /* Structure to hold the flattened member data */
  struct MemberNode {
    SymbolClass *type;
    string path;
    int absolute_offset;
  };

  Result<vector<MemberNode>, TokenException> get_all_members_flat() const;

 private:
  static optional<TokenException> get_flat_members_recursive(const SymbolClass *cls,
                                                             const string &current_path,
                                                             int current_offset,
                                                             vector<MemberNode> &out_buffer);
};

}  // namespace bsl
