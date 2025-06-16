/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
  float cospsi = sin(thetav) * sin(theta) * cos(phi - phiv) + cos(thetav) * cos(theta);

  if (cospsi > 1.0f) {
    return 0.0f;
  }
  if (cospsi < -1.0f) {
    return M_PI;
  }

  return acos(cospsi);
}

float3 sky_spherical_coordinates(float3 dir)
{
  return float3(M_PI_2 - atan(dir.z, length(dir.xy)), atan(dir.x, dir.y), 0.0f);
}

void node_tex_sky_nishita(float3 co,
                          float sun_rotation,
                          float3 xyz_to_r,
                          float3 xyz_to_g,
                          float3 xyz_to_b,
                          sampler2DArray ima,
                          float layer,
                          out float4 color)
{
  float3 spherical = sky_spherical_coordinates(co);

  float3 xyz;
  if (co.z < -0.4f) {
    /* too far below the horizon, just return black */
    color = float4(0, 0, 0, 1);
  }
  else {
    /* evaluate longitudinal position on the map */
    constexpr float tau = 6.28318530717958647692f;
    float x = (spherical.y + M_PI + sun_rotation) / tau;
    if (x > 1.0f) {
      x -= 1.0f;
    }

    float fade;
    float y;
    if (co.z < 0.0f) {
      /* we're below the horizon, so extend the map by blending from values at the horizon
       * to zero according to a cubic falloff */
      fade = 1.0f + co.z * 2.5f;
      fade = fade * fade * fade;
      y = 0.0f;
    }
    else {
      /* we're above the horizon, so compute the lateral position by inverting the remapped
       * coordinates that are preserve to have more detail near the horizon. */
      fade = 1.0f;
      y = sqrt((M_PI_2 - spherical.x) / M_PI_2);
    }

    /* look up color in the precomputed map and convert to RGB */
    xyz = fade * texture(ima, float3(x, y, layer)).rgb;
    color = float4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1);
  }
}
