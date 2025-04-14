/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 attenuated_streak = texture_load(streak_tx, texel) * attenuation_factor;
  float4 current_accumulated_streaks = imageLoad(accumulated_streaks_img, texel);
  float4 combined_streaks = current_accumulated_streaks + attenuated_streak;
  imageStore(accumulated_streaks_img, texel, float4(combined_streaks.rgb, 1.0f));
}
