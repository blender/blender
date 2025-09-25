/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifndef USE_GPU_SHADER_CREATE_INFO

#  include "GPU_shader_shared_utils.hh"

struct TestOutputRawData;
#endif

/* NOTE: float3 has differing stride and alignment rules across different GPU back-ends. If 12 byte
 * stride and alignment is essential, use `packed_float3` to avoid data read issues. This is
 * required in the common use-case where a float3 and an int/float are paired together for optimal
 * data transfer. */

enum GPUKeyframeShapes : uint32_t {
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

#define MAX_SOCKET_PARAMETERS 4
#define MAX_SOCKET_INSTANCE 32

/* Node Socket shader parameters. Must match the shader layout of "gpu_shader_2D_node_socket". */
struct NodeSocketShaderParameters {
  float4 rect;
  float4 color_inner;
  float4 color_outline;
  float outline_thickness;
  float outline_offset;
  float shape;
  float aspect;
};
BLI_STATIC_ASSERT_ALIGN(NodeSocketShaderParameters, 16)

/* Per link data. */
struct NodeLinkData {
  float4 start_color;
  float4 end_color;
  float2 bezier_P0;
  float2 bezier_P1;
  float2 bezier_P2;
  float2 bezier_P3;
  uint color_ids;
  float dash_length;
  float dash_factor;
  float dash_alpha;
  float dim_factor;
  float thickness;
  float aspect;
  bool32_t do_arrow;
  bool32_t do_muted;
  bool32_t has_back_link;
  float _pad0;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(NodeLinkData, 16)

/* Data common to all links. */
struct NodeLinkUniformData {
  float4 colors[6];
  float aspect;
  float arrow_size;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(NodeLinkUniformData, 16)

struct GPencilStrokeData {
  float2 viewport;
  float pixsize;
  float objscale;
  float pixfactor;
  int xraymode;
  int caps_start;
  int caps_end;
  bool32_t keep_size;
  bool32_t fill_stroke;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(GPencilStrokeData, 16)

struct GPUClipPlanes {
  float4x4 ClipModelMatrix;
  float4 world[6];
};
BLI_STATIC_ASSERT_ALIGN(GPUClipPlanes, 16)

struct SimpleLightingData {
  float4 l_color;
  packed_float3 light;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(SimpleLightingData, 16)

#define MAX_CALLS 16

struct MultiIconCallData {
  float4 calls_data[MAX_CALLS * 3];
};
BLI_STATIC_ASSERT_ALIGN(MultiIconCallData, 16)

#define GPU_SEQ_STRIP_DRAW_DATA_LEN 256

enum GPUSeqFlags : uint32_t {
  GPU_SEQ_FLAG_BACKGROUND = (1u << 0u),
  GPU_SEQ_FLAG_SINGLE_IMAGE = (1u << 1u),
  GPU_SEQ_FLAG_COLOR_BAND = (1u << 2u),
  GPU_SEQ_FLAG_TRANSITION = (1u << 3u),
  GPU_SEQ_FLAG_LOCKED = (1u << 4u),
  GPU_SEQ_FLAG_MISSING_TITLE = (1u << 5u),
  GPU_SEQ_FLAG_MISSING_CONTENT = (1u << 6u),
  GPU_SEQ_FLAG_SELECTED = (1u << 7u),
  GPU_SEQ_FLAG_ACTIVE = (1u << 8u),
  GPU_SEQ_FLAG_HIGHLIGHT = (1u << 9u),
  GPU_SEQ_FLAG_BORDER = (1u << 10u),
  GPU_SEQ_FLAG_SELECTED_LH = (1u << 11u),
  GPU_SEQ_FLAG_SELECTED_RH = (1u << 12u),
  GPU_SEQ_FLAG_OVERLAP = (1u << 15u),
  GPU_SEQ_FLAG_CLAMPED = (1u << 16u),

  GPU_SEQ_FLAG_ANY_HANDLE = GPU_SEQ_FLAG_SELECTED_LH | GPU_SEQ_FLAG_SELECTED_RH
};

/* Glyph for text rendering. */
struct GlyphQuad {
  int4 position;
  float4 glyph_color; /* Cannot be name `color` because of metal macros. */
  int2 glyph_size;
  int offset;
  uint flags;
};
BLI_STATIC_ASSERT_ALIGN(GlyphQuad, 16)

/* VSE per-strip data for timeline rendering. */
struct SeqStripDrawData {
  /* Horizontal strip positions (1.0 is one frame). */
  float left_handle, right_handle;  /* Left and right strip sides. */
  float content_start, content_end; /* Start and end of actual content (only relevant for strips
                                     * that have holdout regions). */
  float handle_width;
  /* Vertical strip positions (1.0 is one channel). */
  float bottom;
  float top;
  float strip_content_top; /* Content coordinate, i.e. below title bar if there is one. */
  uint flags;              /* GPUSeqFlags bitmask. */
  /* Strip colors, each is uchar4 packed with equivalent of packUnorm4x8. */
  uint col_background;
  uint col_outline;
  uint col_color_band;
  uint col_transition_in, col_transition_out;
  float _pad0, _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SeqStripDrawData, 16)
BLI_STATIC_ASSERT(sizeof(SeqStripDrawData) * GPU_SEQ_STRIP_DRAW_DATA_LEN <= 16384,
                  "SeqStripDrawData UBO must not exceed minspec UBO size (16384)")

/* VSE per-thumbnail data for timeline rendering. */
struct SeqStripThumbData {
  float left, right, bottom, top; /* Strip rectangle positions. */
  float x1, y1, x2, y2;           /* Thumbnail rectangle positions. */
  float u1, v1, u2, v2;           /* Thumbnail UVs. */
  float4 tint_color;
};
BLI_STATIC_ASSERT_ALIGN(SeqStripThumbData, 16)
BLI_STATIC_ASSERT(sizeof(SeqStripThumbData) * GPU_SEQ_STRIP_DRAW_DATA_LEN <= 16384,
                  "SeqStripThumbData UBO must not exceed minspec UBO size (16384)")

/* VSE global data for timeline rendering. */
struct SeqContextDrawData {
  float round_radius;
  float pixelsize;
  uint col_back;
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(SeqContextDrawData, 16)

/* VSE scope point rasterizer data. */
struct SeqScopeRasterData {
  uint col_r;
  uint col_g;
  uint col_b;
  uint col_a;
};

struct GreasePencilStrokeData {
  packed_float3 position;
  float stroke_thickness;
  float4 stroke_color;
};
BLI_STATIC_ASSERT_ALIGN(GreasePencilStrokeData, 16)

enum TestStatus : uint32_t {
  TEST_STATUS_NONE = 0u,
  TEST_STATUS_PASSED = 1u,
  TEST_STATUS_FAILED = 2u,
};
enum TestType : uint32_t {
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
BLI_STATIC_ASSERT_ALIGN(TestOutputRawData, 16)

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
BLI_STATIC_ASSERT_ALIGN(TestOutput, 16)

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
