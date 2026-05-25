/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Bokeh Look Up Table: This outputs a radius multiplier to shape the sampling in gather pass or
 * the scatter sprite appearance. This is only used if bokeh shape is either anamorphic or is not
 * a perfect circle.
 * We correct samples spacing for polygonal bokeh shapes. However, we do not for anamorphic bokeh
 * as it is way more complex and expensive to do.
 */
#pragma once

#include "eevee_depth_of_field_lib.bsl.hh"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

namespace eevee::dof::bokeh_lut {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[uniform(0)]] const DepthOfFieldData &dof_buf;

  [[image(0, write, SFLOAT_16_16)]] image2D out_gather_lut_img;
  [[image(1, write, SFLOAT_16)]] image2D out_scatter_lut_img;
  [[image(2, write, SFLOAT_16)]] image2D out_resolve_lut_img;
};

[[compute, local_size(DOF_BOKEH_LUT_SIZE, DOF_BOKEH_LUT_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  float2 gather_uv = ((float2(global_id.xy) + 0.5f) / float(DOF_BOKEH_LUT_SIZE));
  /* Center uv in range [-1..1]. */
  gather_uv = gather_uv * 2.0f - 1.0f;

  float2 slight_focus_texel = float2(global_id.xy) - float(dof_max_slight_focus_radius);

  float radius = length(gather_uv);

  if (srt.dof_buf.bokeh_blades > 0.0f) {
    /* NOTE: atan(y,x) has output range [-M_PI..M_PI], so add 2pi to avoid negative angles. */
    float theta = atan(gather_uv.y, gather_uv.x) + M_TAU;

    radius /= circle_to_polygon_radius(srt.dof_buf.bokeh_blades,
                                       theta - srt.dof_buf.bokeh_rotation);

    float theta_new = circle_to_polygon_angle(srt.dof_buf.bokeh_blades, theta);
    float r_new = circle_to_polygon_radius(srt.dof_buf.bokeh_blades, theta_new);

    theta_new -= srt.dof_buf.bokeh_rotation;

    gather_uv = r_new * float2(-cos(theta_new), sin(theta_new));

    {
      /* Slight focus distance */
      slight_focus_texel *= srt.dof_buf.bokeh_anisotropic_scale_inv;
      float theta = atan(slight_focus_texel.y, -slight_focus_texel.x) + M_TAU;
      slight_focus_texel /= circle_to_polygon_radius(srt.dof_buf.bokeh_blades,
                                                     theta + srt.dof_buf.bokeh_rotation);
    }
  }
  else {
    gather_uv *= safe_rcp(length(gather_uv));
  }

  int2 texel = int2(global_id.xy);
  /* For gather store the normalized UV. */
  imageStoreFast(srt.out_gather_lut_img, texel, gather_uv.xyxy);
  /* For scatter store distance. LUT will be scaled by COC. */
  imageStoreFast(srt.out_scatter_lut_img, texel, float4(radius));
  /* For slight focus gather store pixel perfect distance. */
  imageStore(srt.out_resolve_lut_img, texel, float4(length(slight_focus_texel)));
}

}  // namespace eevee::dof::bokeh_lut

PipelineCompute eevee_depth_of_field_bokeh_lut(eevee::dof::bokeh_lut::comp_main);
