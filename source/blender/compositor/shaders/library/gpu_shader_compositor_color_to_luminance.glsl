/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

void color_to_luminance(float4 color, float3 luminance_coefficients, out float result)
{
  result = get_luminance(color.rgb, luminance_coefficients);
}
