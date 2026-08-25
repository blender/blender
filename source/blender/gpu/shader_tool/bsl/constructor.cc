/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_table.hh"

namespace bsl {
using namespace std;
using namespace blender::gpu::shader::parser;
using namespace blender::gpu::shader::parser::ast;

/* Generates every single legal BSL constructor dynamically. */
vector<SymbolTable::BuiltinFunc> SymbolTable::generate_all_constructors(
    const vector<BuiltinType> &types)
{
  vector<BuiltinFunc> funcs;

  /* Vector and Scalar Constructors (restricted to matching component types). */
  for (const auto [type, base] : types) {
    SymbolClass *s = base;
    SymbolClass *v2 = make_type(base, 2);
    SymbolClass *v3 = make_type(base, 3);
    SymbolClass *v4 = make_type(base, 4);

    switch (type->comp_len) {
      case 1:
        /* Scalar Copy: float(float) */
        funcs.push_back({s, s->identifier, {s}});
        break;
      case 2:
        /* Copy: float2(float2) */
        funcs.push_back({v2, v2->identifier, {v2}});
        /* Broadcast: float2(float) */
        funcs.push_back({v2, v2->identifier, {s}});
        /* Component-wise (1+1): float2(float, float) */
        funcs.push_back({v2, v2->identifier, {s, s}});
        break;
      case 3:
        /* Copy: float3(float3) */
        funcs.push_back({v3, v3->identifier, {v3}});
        /* Broadcast: float3(float) */
        funcs.push_back({v3, v3->identifier, {s}});
        /* 1 + 1 + 1: float3(float, float, float) */
        funcs.push_back({v3, v3->identifier, {s, s, s}});
        /* 1 + 2: float3(float, float2) */
        funcs.push_back({v3, v3->identifier, {s, v2}});
        /* 2 + 1: float3(float2, float) */
        funcs.push_back({v3, v3->identifier, {v2, s}});
        break;
      case 4:
        /* Copy: float4(float4) */
        funcs.push_back({v4, v4->identifier, {v4}});
        /* Broadcast: float4(float) */
        funcs.push_back({v4, v4->identifier, {s}});
        /* 1 + 1 + 1 + 1: float4(float, float, float, float) */
        funcs.push_back({v4, v4->identifier, {s, s, s, s}});
        /* 1 + 3: float4(float, float3) */
        funcs.push_back({v4, v4->identifier, {s, v3}});
        /* 3 + 1: float4(float3, float) */
        funcs.push_back({v4, v4->identifier, {v3, s}});
        /* 2 + 2: float4(float2, float2) */
        funcs.push_back({v4, v4->identifier, {v2, v2}});
        /* 2 + 1 + 1: float4(float2, float, float) */
        funcs.push_back({v4, v4->identifier, {v2, s, s}});
        /* 1 + 2 + 1: float4(float, float2, float) */
        funcs.push_back({v4, v4->identifier, {s, v2, s}});
        /* 1 + 1 + 2: float4(float, float, float2) */
        funcs.push_back({v4, v4->identifier, {s, s, v2}});
        break;
    }
  }

  /* Matrix Constructors (restricted to matching "float" component type). */
  for (int n = 2; n <= 4; ++n) {
    for (int m = 2; m <= 4; ++m) {
      SymbolClass *row = make_type(float_cls, m);
      SymbolClass *mat = make_type(row, n);
      /* Broadcast: float3x3(float) */
      funcs.push_back({mat, mat->identifier, {float_cls}});
      /* All-Scalar: float3x3(float, float, ...) [N*N scalars] */
      funcs.push_back({mat, mat->identifier, vector<SymbolClass *>(n * m, float_cls)});
      /* All-Vector: float3x3(float3, float3, float3) [N vectors] */
      funcs.push_back({mat, mat->identifier, vector<SymbolClass *>(n, row)});
    }
  }

  return funcs;
}

}  // namespace bsl
