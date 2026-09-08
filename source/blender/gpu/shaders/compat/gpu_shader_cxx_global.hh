/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include "gpu_shader_cxx_vector.hh"

/* -------------------------------------------------------------------- */
/** \name Special Variables
 * \{ */

namespace gl_VertexShader {

extern const int gl_VertexID;
extern const int gl_InstanceID;
extern const int gl_BaseVertex;
extern const int gpu_BaseInstance;
extern const int gpu_InstanceIndex;
float4 gl_Position = float4(0);
float gl_PointSize = 0;
float gl_ClipDistance[6] = {0};
int gpu_Layer = 0;
int gpu_ViewportIndex = 0;

}  // namespace gl_VertexShader

namespace gl_FragmentShader {

extern const float4 gl_FragCoord;
const bool gl_FrontFacing = true;
const float2 gl_PointCoord = float2(0);
const int gl_PrimitiveID = 0;
float gl_FragDepth = 0;
const float gl_ClipDistance[6] = {0};
const int gpu_Layer = 0;
const int gpu_ViewportIndex = 0;

}  // namespace gl_FragmentShader

/* Outside of namespace to be used in create infos. */
constexpr uint3 gl_WorkGroupSize = uint3(16, 16, 16);

namespace gl_ComputeShader {

extern const uint3 gl_NumWorkGroups;
extern const uint3 gl_WorkGroupID;
extern const uint3 gl_LocalInvocationID;
extern const uint3 gl_GlobalInvocationID;
extern const uint gl_LocalInvocationIndex;

}  // namespace gl_ComputeShader

/** \} */
