/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/gpu_srgb_to_framebuffer_space_info.hh"

SHADER_LIBRARY_CREATE_INFO(gpu_srgb_to_framebuffer_space)

/* Undefine the macro that avoids compilation errors. */
#undef blender_srgb_to_framebuffer_space

/* Raw python shaders don't have create infos and thus don't generate the needed `srgbTarget`
 * uniform automatically. For API compatibility, we sill define this loose uniform, but it will
 * not be parsed by the Metal or Vulkan backend. */
#ifdef GPU_RAW_PYTHON_SHADER
uniform bool srgbTarget = false;
#endif

vec4 blender_srgb_to_framebuffer_space(vec4 in_color)
{
  if (srgbTarget) {
    vec3 c = max(in_color.rgb, vec3(0.0f));
    vec3 c1 = c * (1.0f / 12.92f);
    vec3 c2 = pow((c + 0.055f) * (1.0f / 1.055f), vec3(2.4f));
    in_color.rgb = mix(c1, c2, step(vec3(0.04045f), c));
  }
  return in_color;
}
