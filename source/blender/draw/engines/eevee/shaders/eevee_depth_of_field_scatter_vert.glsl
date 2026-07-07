/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Scatter pass: Use sprites to scatter the color of very bright pixel to have higher quality blur.
 *
 * We only scatter one triangle per sprite and one sprite per 4 pixels to reduce vertex shader
 * invocations and overdraw.
 */

#include "infos/eevee_depth_of_field_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_depth_of_field_scatter)

#include "eevee_depth_of_field_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

void main()
{
  if (uint(gl_InstanceID) >= dof_buf.scatter_max_rect) {
    /* Very unlikely to happen but better avoid out of bound access. */
    gl_Position = float4(0.0f);
    return;
  }

  ScatterRect rect = scatter_list_buf[gl_InstanceID];

  interp_flat.color_and_coc1 = rect.color_and_coc[0];
  interp_flat.color_and_coc2 = rect.color_and_coc[1];
  interp_flat.color_and_coc3 = rect.color_and_coc[2];
  interp_flat.color_and_coc4 = rect.color_and_coc[3];

  float2 uv = float2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0f - 1.0f;
  uv = uv * rect.half_extent;

  gl_Position = float4(uv + rect.offset, 0.0f, 1.0f);
  /* NDC range [-1..1]. */
  gl_Position.xy = (gl_Position.xy / float2(textureSize(occlusion_tx, 0).xy)) * 2.0f - 1.0f;

  if (use_bokeh_lut) {
    /* Bias scale to avoid sampling at the texture's border. */
    interp_flat.distance_scale = (float(DOF_BOKEH_LUT_SIZE) / float(DOF_BOKEH_LUT_SIZE - 1));
    float2 uv_div = 1.0f / (interp_flat.distance_scale * abs(rect.half_extent));
    interp_noperspective.rect_uv1 = ((uv + quad_offsets[0]) * uv_div) * 0.5f + 0.5f;
    interp_noperspective.rect_uv2 = ((uv + quad_offsets[1]) * uv_div) * 0.5f + 0.5f;
    interp_noperspective.rect_uv3 = ((uv + quad_offsets[2]) * uv_div) * 0.5f + 0.5f;
    interp_noperspective.rect_uv4 = ((uv + quad_offsets[3]) * uv_div) * 0.5f + 0.5f;
    /* Only for sampling. */
    interp_flat.distance_scale *= reduce_max(abs(rect.half_extent));
  }
  else {
    interp_flat.distance_scale = 1.0f;
    interp_noperspective.rect_uv1 = uv + quad_offsets[0];
    interp_noperspective.rect_uv2 = uv + quad_offsets[1];
    interp_noperspective.rect_uv3 = uv + quad_offsets[2];
    interp_noperspective.rect_uv4 = uv + quad_offsets[3];
  }
}
