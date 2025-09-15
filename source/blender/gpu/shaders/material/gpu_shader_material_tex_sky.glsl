/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float3 sky_spherical_coordinates(float3 dir)
{
  return float3(M_PI_2 - atan(dir.z, length(dir.xy)), atan(dir.x, dir.y), 0.0f);
}

void node_tex_sky(float3 co,
                  float sky_type,
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
  float dir_elevation = M_PI_2 - spherical.x;
  float x = (spherical.y + M_PI + sun_rotation) / (2.0f * M_PI);
  float fade = 1.0f;
  float y;

  if (sky_type == 0.0f && co.z < -0.4f) {
    /* Black ground if Single Scattering model */
    color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    return;
  }
  if (sky_type == 0.0f && co.z < 0.0f) {
    /* Ground fade */
    fade = pow(1.0f + co.z * 2.5f, 3.0f);
    y = 0.508f;
  }
  else {
    /* Undo the non-linear transformation from the sky LUT. */
    float dir_elevation_abs = (dir_elevation < 0.0f) ? -dir_elevation : dir_elevation;
    y = sqrt(dir_elevation_abs / M_PI_2) * sign(dir_elevation) * 0.5f + 0.5f;
  }
  /* Look up color in the precomputed map and convert to RGB. */
  xyz = fade * texture(ima, float3(x, y, layer)).rgb;
  color = float4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1.0f);
}
