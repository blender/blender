/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

#include "ocio_shader_shared.hh"

#include "gpu_shader_display_transform_fallback_lib.glsl"

/* Info is generated at runtime. */
GPU_SHADER_CREATE_INFO(OCIO_Display)
GPU_SHADER_CREATE_END()

/* Replaced by the OCIO generated code. */
float4 OCIO_to_scene_linear(float4 pixel)
{
  return pixel;
}

/* Replaced by the OCIO generated code. */
float4 OCIO_to_display(float4 pixel)
{
  return pixel;
}
