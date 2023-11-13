/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 uv = vec2(texel) / vec2(domain_size - ivec2(1));
  uv -= location;
  uv.y *= float(domain_size.y) / float(domain_size.x);
  uv = mat2(cos_angle, -sin_angle, sin_angle, cos_angle) * uv;
  bool is_inside = length(uv / radius) < 1.0;

  float base_mask_value = texture_load(base_mask_tx, texel).x;
  float value = texture_load(mask_value_tx, texel).x;

#if defined(CMP_NODE_MASKTYPE_ADD)
  float output_mask_value = is_inside ? max(base_mask_value, value) : base_mask_value;
#elif defined(CMP_NODE_MASKTYPE_SUBTRACT)
  float output_mask_value = is_inside ? clamp(base_mask_value - value, 0.0, 1.0) : base_mask_value;
#elif defined(CMP_NODE_MASKTYPE_MULTIPLY)
  float output_mask_value = is_inside ? base_mask_value * value : 0.0;
#elif defined(CMP_NODE_MASKTYPE_NOT)
  float output_mask_value = is_inside ? (base_mask_value > 0.0 ? 0.0 : value) : base_mask_value;
#endif

  imageStore(output_mask_img, texel, vec4(output_mask_value));
}
