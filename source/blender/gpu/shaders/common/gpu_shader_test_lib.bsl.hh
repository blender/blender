/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"
#include "gpu_shader_math_base.bsl.hh"           /* IWYU pragma: export */
#include "gpu_shader_math_vector_compare.bsl.hh" /* IWYU pragma: export */

#include "GPU_shader_shared.hh"

/* clang-format off */
#ifndef GPU_METAL
bool is_integer(bool /*v*/) { return true; }
#endif
bool is_integer(uint /*v*/) { return true; }
bool is_integer(int /*v*/) { return true; }
bool is_integer(float /*v*/) { return false; }
bool is_integer(int2 /*v*/) { return true; }
bool is_integer(int3 /*v*/) { return true; }
bool is_integer(int4 /*v*/) { return true; }
bool is_integer(uint2 /*v*/) { return true; }
bool is_integer(uint3 /*v*/) { return true; }
bool is_integer(uint4 /*v*/) { return true; }
bool is_integer(float2 /*v*/) { return false; }
bool is_integer(float3 /*v*/) { return false; }
bool is_integer(float4 /*v*/) { return false; }
bool is_integer(float2x2 /*v*/) { return false; }
bool is_integer(float2x3 /*v*/) { return false; }
bool is_integer(float2x4 /*v*/) { return false; }
bool is_integer(float3x2 /*v*/) { return false; }
bool is_integer(float3x3 /*v*/) { return false; }
bool is_integer(float3x4 /*v*/) { return false; }
bool is_integer(float4x2 /*v*/) { return false; }
bool is_integer(float4x3 /*v*/) { return false; }
bool is_integer(float4x4 /*v*/) { return false; }

int mat_row_len(float2x2 /*v*/) { return 2; }
int mat_row_len(float2x3 /*v*/) { return 3; }
int mat_row_len(float2x4 /*v*/) { return 4; }
int mat_row_len(float3x2 /*v*/) { return 2; }
int mat_row_len(float3x3 /*v*/) { return 3; }
int mat_row_len(float3x4 /*v*/) { return 4; }
int mat_row_len(float4x2 /*v*/) { return 2; }
int mat_row_len(float4x3 /*v*/) { return 3; }
int mat_row_len(float4x4 /*v*/) { return 4; }

int mat_col_len(float2x2 /*v*/) { return 2; }
int mat_col_len(float2x3 /*v*/) { return 2; }
int mat_col_len(float2x4 /*v*/) { return 2; }
int mat_col_len(float3x2 /*v*/) { return 3; }
int mat_col_len(float3x3 /*v*/) { return 3; }
int mat_col_len(float3x4 /*v*/) { return 3; }
int mat_col_len(float4x2 /*v*/) { return 4; }
int mat_col_len(float4x3 /*v*/) { return 4; }
int mat_col_len(float4x4 /*v*/) { return 4; }
int mat_col_len(int2 /*v*/) { return 2; }
int mat_col_len(int3 /*v*/) { return 3; }
int mat_col_len(int4 /*v*/) { return 4; }
int mat_col_len(uint2 /*v*/) { return 2; }
int mat_col_len(uint3 /*v*/) { return 3; }
int mat_col_len(uint4 /*v*/) { return 4; }
int mat_col_len(float2 /*v*/) { return 2; }
int mat_col_len(float3 /*v*/) { return 3; }
int mat_col_len(float4 /*v*/) { return 4; }

#ifndef GPU_METAL
uint to_type(bool /*v*/) { return TEST_TYPE_BOOL; }
#endif
uint to_type(uint /*v*/) { return TEST_TYPE_UINT; }
uint to_type(int /*v*/) { return TEST_TYPE_INT; }
uint to_type(float /*v*/) { return TEST_TYPE_FLOAT; }
uint to_type(int2 /*v*/) { return TEST_TYPE_IVEC2; }
uint to_type(int3 /*v*/) { return TEST_TYPE_IVEC3; }
uint to_type(int4 /*v*/) { return TEST_TYPE_IVEC4; }
uint to_type(uint2 /*v*/) { return TEST_TYPE_UVEC2; }
uint to_type(uint3 /*v*/) { return TEST_TYPE_UVEC3; }
uint to_type(uint4 /*v*/) { return TEST_TYPE_UVEC4; }
uint to_type(float2 /*v*/) { return TEST_TYPE_VEC2; }
uint to_type(float3 /*v*/) { return TEST_TYPE_VEC3; }
uint to_type(float4 /*v*/) { return TEST_TYPE_VEC4; }
uint to_type(float2x2 /*v*/) { return TEST_TYPE_MAT2X2; }
uint to_type(float2x3 /*v*/) { return TEST_TYPE_MAT2X3; }
uint to_type(float2x4 /*v*/) { return TEST_TYPE_MAT2X4; }
uint to_type(float3x2 /*v*/) { return TEST_TYPE_MAT3X2; }
uint to_type(float3x3 /*v*/) { return TEST_TYPE_MAT3X3; }
uint to_type(float3x4 /*v*/) { return TEST_TYPE_MAT3X4; }
uint to_type(float4x2 /*v*/) { return TEST_TYPE_MAT4X2; }
uint to_type(float4x3 /*v*/) { return TEST_TYPE_MAT4X3; }
uint to_type(float4x4 /*v*/) { return TEST_TYPE_MAT4X4; }
/* clang-format on */

template<typename T> TestOutputRawData write_matrix(T v)
{
  TestOutputRawData raw;
  for (int c = 0; c < mat_col_len(v); c++) {
    for (int r = 0; r < mat_row_len(v); r++) {
      raw.data[c * mat_row_len(v) + r] = floatBitsToUint(v[c][r]);
    }
  }
  return raw;
}

template<typename T> TestOutputRawData write_float_vector(T v)
{
  TestOutputRawData raw;
  for (int c = 0; c < mat_col_len(v); c++) {
    raw.data[c] = floatBitsToUint(v[c]);
  }
  return raw;
}

template<typename T> TestOutputRawData write_int_vector(T v)
{
  TestOutputRawData raw;
  for (int c = 0; c < mat_col_len(v); c++) {
    raw.data[c] = uint(v[c]);
  }
  return raw;
}

TestOutputRawData write_float_scalar(float v)
{
  TestOutputRawData raw;
  raw.data[0] = floatBitsToUint(v);
  return raw;
}

TestOutputRawData write_int_scalar(uint v)
{
  TestOutputRawData raw;
  raw.data[0] = v;
  return raw;
}

template TestOutputRawData write_matrix<float2x2>(float2x2 v);
template TestOutputRawData write_matrix<float2x3>(float2x3 v);
template TestOutputRawData write_matrix<float2x4>(float2x4 v);
template TestOutputRawData write_matrix<float3x2>(float3x2 v);
template TestOutputRawData write_matrix<float3x3>(float3x3 v);
template TestOutputRawData write_matrix<float3x4>(float3x4 v);
template TestOutputRawData write_matrix<float4x2>(float4x2 v);
template TestOutputRawData write_matrix<float4x3>(float4x3 v);
template TestOutputRawData write_matrix<float4x4>(float4x4 v);
template TestOutputRawData write_float_vector<float2>(float2 v);
template TestOutputRawData write_float_vector<float3>(float3 v);
template TestOutputRawData write_float_vector<float4>(float4 v);
template TestOutputRawData write_int_vector<uint2>(uint2 v);
template TestOutputRawData write_int_vector<uint3>(uint3 v);
template TestOutputRawData write_int_vector<uint4>(uint4 v);

/* clang-format off */
#ifndef GPU_METAL
TestOutputRawData as_raw_data(bool v) { return write_int_scalar(uint(v)); }
#endif
TestOutputRawData as_raw_data(uint v) { return write_int_scalar(v); }
TestOutputRawData as_raw_data(int v) { return write_int_scalar(uint(v)); }
TestOutputRawData as_raw_data(float v) { return write_float_scalar(v); }
TestOutputRawData as_raw_data(int2 v) { return write_int_vector(uint2(v)); }
TestOutputRawData as_raw_data(int3 v) { return write_int_vector(uint3(v)); }
TestOutputRawData as_raw_data(int4 v) { return write_int_vector(uint4(v)); }
TestOutputRawData as_raw_data(uint2 v) { return write_int_vector(v); }
TestOutputRawData as_raw_data(uint3 v) { return write_int_vector(v); }
TestOutputRawData as_raw_data(uint4 v) { return write_int_vector(v); }
TestOutputRawData as_raw_data(float2 v) { return write_float_vector(v); }
TestOutputRawData as_raw_data(float3 v) { return write_float_vector(v); }
TestOutputRawData as_raw_data(float4 v) { return write_float_vector(v); }
TestOutputRawData as_raw_data(float2x2 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float2x3 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float2x4 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float3x2 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float3x3 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float3x4 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float4x2 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float4x3 v) { return write_matrix(v); }
TestOutputRawData as_raw_data(float4x4 v) { return write_matrix(v); }
/* clang-format on */

#ifdef GPU_METAL
/* Vector comparison in MSL return a `bvec`. Collapse it like in GLSL. */
#  define COLLAPSE_BOOL(OP) bool(all(OP))
#else
#  undef COLLAPSE_BOOL /* Silence warning caused by define grepping in info files. */
#  define COLLAPSE_BOOL(OP) (OP)
#endif

#define EXPECT_OP(OP, val1, val2) \
  test_output(as_raw_data(val1), as_raw_data(val2), COLLAPSE_BOOL(OP), to_type(val1))

#define EXPECT_EQ(result, expect) EXPECT_OP((result) == (expect), result, expect)
#define EXPECT_NE(result, expect) EXPECT_OP((result) != (expect), result, expect)
#define EXPECT_LE(result, expect) EXPECT_OP((result) <= (expect), result, expect)
#define EXPECT_LT(result, expect) EXPECT_OP((result) < (expect), result, expect)
#define EXPECT_GE(result, expect) EXPECT_OP((result) >= (expect), result, expect)
#define EXPECT_GT(result, expect) EXPECT_OP((result) > (expect), result, expect)

#define EXPECT_TRUE(result) EXPECT_OP(result, result, true)
#define EXPECT_FALSE(result) EXPECT_OP(!result, result, false)

#define EXPECT_NEAR(result, expect, threshold) \
  EXPECT_OP(is_equal(result, expect, threshold), result, expect)

struct ShaderTestOutput {
  [[storage(0, write)]] TestOutput (&out_test)[];
};

/* Must be followed by semicolon to not confuse the BSL parser. */
#define TEST(a, b)
