/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Bokeh Look Up Table: This outputs a radius multiplier to shape the sampling in gather pass or
 * the scatter sprite appearance. This is only used if bokeh shape is either anamorphic or is not
 * a perfect circle.
 * We correct samples spacing for polygonal bokeh shapes. However, we do not for anamorphic bokeh
 * as it is way more complex and expensive to do.
 */

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

float polygon_sides_length(float sides_count)
{
  return 2.0 * sin(M_PI / sides_count);
}

/* Returns intersection ratio between the radius edge at theta and the polygon edge.
 * Start first corners at theta == 0. */
float circle_to_polygon_radius(float sides_count, float theta)
{
  /* From Graphics Gems from CryENGINE 3 (Siggraph 2013) by Tiago Sousa (slide 36). */
  float side_angle = M_2PI / sides_count;
  float halfside_angle = side_angle * 0.5;
  return cos(side_angle * 0.5) /
         cos(theta - side_angle * floor((sides_count * theta + M_PI) / M_2PI));
}

/* Remap input angle to have homogeneous spacing of points along a polygon edge.
 * Expect theta to be in [0..2pi] range. */
float circle_to_polygon_angle(float sides_count, float theta)
{
  float side_angle = M_2PI / sides_count;
  float halfside_angle = side_angle * 0.5;
  float side = floor(theta / side_angle);
  /* Length of segment from center to the middle of polygon side. */
  float adjacent = circle_to_polygon_radius(sides_count, 0.0);

  /* This is the relative position of the sample on the polygon half side. */
  float local_theta = theta - side * side_angle;
  float ratio = (local_theta - halfside_angle) / halfside_angle;

  float halfside_len = polygon_sides_length(sides_count) * 0.5;
  float opposite = ratio * halfside_len;

  /* NOTE: atan(y_over_x) has output range [-M_PI_2..M_PI_2]. */
  float final_local_theta = atan(opposite / adjacent);

  return side * side_angle + final_local_theta;
}

void main()
{
  /* Center uv in range [-1..1]. */
  vec2 uv = uvcoordsvar.xy * 2.0 - 1.0;

  float radius = length(uv);

  vec2 texel = floor(gl_FragCoord.xy) - float(DOF_MAX_SLIGHT_FOCUS_RADIUS);

  if (bokehSides > 0.0) {
    /* NOTE: atan(y,x) has output range [-M_PI..M_PI], so add 2pi to avoid negative angles. */
    float theta = atan(uv.y, uv.x) + M_2PI;
    float r = length(uv);

    radius /= circle_to_polygon_radius(bokehSides, theta - bokehRotation);

    float theta_new = circle_to_polygon_angle(bokehSides, theta);
    float r_new = circle_to_polygon_radius(bokehSides, theta_new);

    theta_new -= bokehRotation;

    uv = r_new * vec2(-cos(theta_new), sin(theta_new));

    {
      /* Slight focus distance */
      texel *= bokehAnisotropyInv;
      float theta = atan(texel.y, -texel.x) + M_2PI;
      texel /= circle_to_polygon_radius(bokehSides, theta + bokehRotation);
    }
  }
  else {
    uv *= safe_rcp(length(uv));
  }

  /* For gather store the normalized UV. */
  outGatherLut = uv;
  /* For scatter store distance. */
  outScatterLut = radius;
  /* For slight focus gather store pixel perfect distance. */
  outResolveLut = length(texel);
}
