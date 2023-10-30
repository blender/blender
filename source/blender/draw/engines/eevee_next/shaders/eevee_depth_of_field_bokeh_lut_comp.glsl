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

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

void main()
{
  vec2 gather_uv = ((vec2(gl_GlobalInvocationID.xy) + 0.5) / float(DOF_BOKEH_LUT_SIZE));
  /* Center uv in range [-1..1]. */
  gather_uv = gather_uv * 2.0 - 1.0;

  vec2 slight_focus_texel = vec2(gl_GlobalInvocationID.xy) - float(dof_max_slight_focus_radius);

  float radius = length(gather_uv);

  if (dof_buf.bokeh_blades > 0.0) {
    /* NOTE: atan(y,x) has output range [-M_PI..M_PI], so add 2pi to avoid negative angles. */
    float theta = atan(gather_uv.y, gather_uv.x) + M_2PI;
    float r = length(gather_uv);

    radius /= circle_to_polygon_radius(dof_buf.bokeh_blades, theta - dof_buf.bokeh_rotation);

    float theta_new = circle_to_polygon_angle(dof_buf.bokeh_blades, theta);
    float r_new = circle_to_polygon_radius(dof_buf.bokeh_blades, theta_new);

    theta_new -= dof_buf.bokeh_rotation;

    gather_uv = r_new * vec2(-cos(theta_new), sin(theta_new));

    {
      /* Slight focus distance */
      slight_focus_texel *= dof_buf.bokeh_anisotropic_scale_inv;
      float theta = atan(slight_focus_texel.y, -slight_focus_texel.x) + M_2PI;
      slight_focus_texel /= circle_to_polygon_radius(dof_buf.bokeh_blades,
                                                     theta + dof_buf.bokeh_rotation);
    }
  }
  else {
    gather_uv *= safe_rcp(length(gather_uv));
  }

  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  /* For gather store the normalized UV. */
  imageStore(out_gather_lut_img, texel, gather_uv.xyxy);
  /* For scatter store distance. LUT will be scaled by COC. */
  imageStore(out_scatter_lut_img, texel, vec4(radius));
  /* For slight focus gather store pixel perfect distance. */
  imageStore(out_resolve_lut_img, texel, vec4(length(slight_focus_texel)));
}
