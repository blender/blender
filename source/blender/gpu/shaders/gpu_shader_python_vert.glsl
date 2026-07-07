/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * File replaced by the content passed through the python API to
 * `GPUShaderCreateInfo.vertex_source(source)`
 *
 * Note that the includes should match this file.
 */

#pragma once
/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

/* This is a mandatory include for python shader to allow correct color management. */
#include "gpu_shader_colorspace_lib.glsl"  // IWYU pragma: export

/* Expose user defined type before resource macros. */
#include "gpu_shader_python_typedef_lib.glsl"  // IWYU pragma: export

/* Replaced by resource macros. */
GPU_SHADER_CREATE_INFO(gpu_shader_python)
GPU_SHADER_CREATE_END()

void main() {}
