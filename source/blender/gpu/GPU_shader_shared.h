/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  include "GPU_shader_shared_utils.h"

typedef struct TestOutputRawData TestOutputRawData;
#endif

/* NOTE: float3 has differing stride and alignment rules across different GPU back-ends. If 12 byte
 * stride and alignment is essential, use `packed_float3` to avoid data read issues. This is
 * required in the common use-case where a float3 and an int/float are paired together for optimal
 * data transfer. */

enum eGPUKeyframeShapes {
  GPU_KEYFRAME_SHAPE_DIAMOND = (1u << 0u),
  GPU_KEYFRAME_SHAPE_CIRCLE = (1u << 1u),
  GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL = (1u << 2u),
  GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL = (1u << 3u),
  GPU_KEYFRAME_SHAPE_INNER_DOT = (1u << 4u),
  GPU_KEYFRAME_SHAPE_ARROW_END_MAX = (1u << 8u),
  GPU_KEYFRAME_SHAPE_ARROW_END_MIN = (1u << 9u),
  GPU_KEYFRAME_SHAPE_ARROW_END_MIXED = (1u << 10u),
  GPU_KEYFRAME_SHAPE_SQUARE = (GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL |
                               GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL),
};

struct NodeLinkData {
  float4 colors[3];
  /* bezierPts Is actually a float2, but due to std140 each element needs to be aligned to 16
   * bytes. */
  float4 bezierPts[4];
  bool1 doArrow;
  bool1 doMuted;
  float dim_factor;
  float thickness;
  float4 dash_params;
  float aspect;
  float arrowSize;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct NodeLinkData, 16)

struct NodeLinkInstanceData {
  float4 colors[6];
  float aspect;
  float arrowSize;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct NodeLinkInstanceData, 16)

struct GPencilStrokeData {
  float2 viewport;
  float pixsize;
  float objscale;
  float pixfactor;
  int xraymode;
  int caps_start;
  int caps_end;
  bool1 keep_size;
  bool1 fill_stroke;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct GPencilStrokeData, 16)

struct GPUClipPlanes {
  float4x4 ClipModelMatrix;
  float4 world[6];
};
BLI_STATIC_ASSERT_ALIGN(struct GPUClipPlanes, 16)

struct SimpleLightingData {
  float4 l_color;
  packed_float3 light;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct SimpleLightingData, 16)

#define MAX_CALLS 16

struct MultiIconCallData {
  float4 calls_data[MAX_CALLS * 3];
};
BLI_STATIC_ASSERT_ALIGN(struct MultiIconCallData, 16)

enum TestStatus {
  TEST_STATUS_NONE = 0u,
  TEST_STATUS_PASSED = 1u,
  TEST_STATUS_FAILED = 2u,
};
enum TestType {
  TEST_TYPE_BOOL = 0u,
  TEST_TYPE_UINT = 1u,
  TEST_TYPE_INT = 2u,
  TEST_TYPE_FLOAT = 3u,
  TEST_TYPE_IVEC2 = 4u,
  TEST_TYPE_IVEC3 = 5u,
  TEST_TYPE_IVEC4 = 6u,
  TEST_TYPE_UVEC2 = 7u,
  TEST_TYPE_UVEC3 = 8u,
  TEST_TYPE_UVEC4 = 9u,
  TEST_TYPE_VEC2 = 10u,
  TEST_TYPE_VEC3 = 11u,
  TEST_TYPE_VEC4 = 12u,
  TEST_TYPE_MAT2X2 = 13u,
  TEST_TYPE_MAT2X3 = 14u,
  TEST_TYPE_MAT2X4 = 15u,
  TEST_TYPE_MAT3X2 = 16u,
  TEST_TYPE_MAT3X3 = 17u,
  TEST_TYPE_MAT3X4 = 18u,
  TEST_TYPE_MAT4X2 = 19u,
  TEST_TYPE_MAT4X3 = 20u,
  TEST_TYPE_MAT4X4 = 21u,
};

/** \note Contains arrays of scalar. To be use only with SSBOs to avoid padding issues. */
struct TestOutputRawData {
  uint data[16];
};
BLI_STATIC_ASSERT_ALIGN(struct TestOutputRawData, 16)

struct TestOutput {
  TestOutputRawData expect;
  TestOutputRawData result;
  /** TestStatus. */
  uint status;
  /** Line error in the GLSL file. */
  int line;
  /** TestType of expect and result. */
  uint type;
  int _pad0;
};
BLI_STATIC_ASSERT_ALIGN(struct TestOutput, 16)

#ifdef GPU_SHADER
TestOutput test_output(
    TestOutputRawData expect, TestOutputRawData result, bool status, int line, uint type)
{
  TestOutput test;
  test.expect = expect;
  test.result = result;
  test.status = status ? TEST_STATUS_PASSED : TEST_STATUS_FAILED;
  test.line = line;
  test.type = type;
  return test;
}
#endif
