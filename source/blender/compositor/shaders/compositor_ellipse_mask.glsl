/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

/* TODO(fclem): deduplicate. */
#define CMP_NODE_MASKTYPE_ADD 0
#define CMP_NODE_MASKTYPE_SUBTRACT 1
#define CMP_NODE_MASKTYPE_MULTIPLY 2
#define CMP_NODE_MASKTYPE_NOT 3

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float2 uv = float2(texel) / float2(domain_size - int2(1));
  uv -= location;
  uv.y *= float(domain_size.y) / float(domain_size.x);
  uv = float2x2(cos_angle, -sin_angle, sin_angle, cos_angle) * uv;
  bool is_inside = length(uv / radius) < 1.0f;

  float base_mask_value = texture_load(base_mask_tx, texel).x;
  float value = texture_load(mask_value_tx, texel).x;

  float output_mask_value;
  if (node_type == CMP_NODE_MASKTYPE_ADD) {
    output_mask_value = is_inside ? max(base_mask_value, value) : base_mask_value;
  }
  else if (node_type == CMP_NODE_MASKTYPE_SUBTRACT) {
    output_mask_value = is_inside ? clamp(base_mask_value - value, 0.0f, 1.0f) : base_mask_value;
  }
  else if (node_type == CMP_NODE_MASKTYPE_MULTIPLY) {
    output_mask_value = is_inside ? base_mask_value * value : 0.0f;
  }
  else if (node_type == CMP_NODE_MASKTYPE_NOT) {
    output_mask_value = is_inside ? (base_mask_value > 0.0f ? 0.0f : value) : base_mask_value;
  }

  imageStore(output_mask_img, texel, float4(output_mask_value));
}
