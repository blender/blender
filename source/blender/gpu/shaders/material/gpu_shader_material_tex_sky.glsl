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

/* Preetham */
/* lam03+lam4: 5 floats passed as vec4+float */
float sky_perez_function(float4 lam03, float lam4, float theta, float gamma)
{
  float ctheta = cos(theta);
  float cgamma = cos(gamma);

  return (1.0f + lam03[0] * exp(lam03[1] / ctheta)) *
         (1.0f + lam03[2] * exp(lam03[3] * gamma) + lam4 * cgamma * cgamma);
}

float3 xyY_to_xyz(float x, float y, float Y)
{
  float X, Z;

  if (y != 0.0f) {
    X = (x / y) * Y;
  }
  else {
    X = 0.0f;
  }

  if (y != 0.0f && Y != 0.0f) {
    Z = ((1.0f - x - y) / y) * Y;
  }
  else {
    Z = 0.0f;
  }

  return float3(X, Y, Z);
}

void node_tex_sky_preetham(float3 co,
                           float4 config_Y03,
                           float config_Y4,
                           float4 config_x03,
                           float config_x4,
                           float4 config_y03,
                           float config_y4,
                           float2 sun_angles,
                           float3 radiance,
                           float3 xyz_to_r,
                           float3 xyz_to_g,
                           float3 xyz_to_b,
                           out float4 color)
{
  /* convert vector to spherical coordinates */
  float3 spherical = sky_spherical_coordinates(co);
  float theta = spherical[0];
  float phi = spherical[1];

  float suntheta = sun_angles[0];
  float sunphi = sun_angles[1];

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2 - 0.001f);

  /* compute xyY color space values */
  float Y = radiance[0] * sky_perez_function(config_Y03, config_Y4, theta, gamma);
  float x = radiance[1] * sky_perez_function(config_x03, config_x4, theta, gamma);
  float y = radiance[2] * sky_perez_function(config_y03, config_y4, theta, gamma);

  /* convert to RGB */
  float3 xyz = xyY_to_xyz(x, y, Y);
  color = float4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1);
}

/* Hosek / Wilkie */
float sky_radiance_hosekwilkie(
    float4 config03, float4 config47, float config8, float theta, float gamma)
{
  float ctheta = cos(theta);
  float cgamma = cos(gamma);

  float expM = exp(config47[0] * gamma);
  float rayM = cgamma * cgamma;
  float mieM = (1.0f + rayM) / pow((1.0f + config8 * config8 - 2.0f * config8 * cgamma), 1.5f);
  float zenith = sqrt(ctheta);

  return (1.0f + config03[0] * exp(config03[1] / (ctheta + 0.01f))) *
         (config03[2] + config03[3] * expM + config47[1] * rayM + config47[2] * mieM +
          config47[3] * zenith);
}

void node_tex_sky_hosekwilkie(float3 co,
                              float4 config_x03,
                              float4 config_x47,
                              float4 config_y03,
                              float4 config_y47,
                              float4 config_z03,
                              float4 config_z47,
                              float3 config_xyz8,
                              float2 sun_angles,
                              float3 radiance,
                              float3 xyz_to_r,
                              float3 xyz_to_g,
                              float3 xyz_to_b,
                              out float4 color)
{
  /* convert vector to spherical coordinates */
  float3 spherical = sky_spherical_coordinates(co);
  float theta = spherical[0];
  float phi = spherical[1];

  float suntheta = sun_angles[0];
  float sunphi = sun_angles[1];

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2 - 0.001f);

  float3 xyz;
  xyz.x = sky_radiance_hosekwilkie(config_x03, config_x47, config_xyz8[0], theta, gamma) *
          radiance.x;
  xyz.y = sky_radiance_hosekwilkie(config_y03, config_y47, config_xyz8[1], theta, gamma) *
          radiance.y;
  xyz.z = sky_radiance_hosekwilkie(config_z03, config_z47, config_xyz8[2], theta, gamma) *
          radiance.z;

  color = float4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1);
}

void node_tex_sky_nishita(float3 co,
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

  /* Undo the non-linear transformation from the sky LUT. */
  float dir_elevation_abs = (dir_elevation < 0.0f) ? -dir_elevation : dir_elevation;
  y = sqrt(dir_elevation_abs / M_PI_2) * sign(dir_elevation) * 0.5f + 0.5f;

  /* Look up color in the precomputed map and convert to RGB. */
  xyz = fade * texture(ima, float3(x, y, layer)).rgb;
  color = float4(dot(xyz_to_r, xyz), dot(xyz_to_g, xyz), dot(xyz_to_b, xyz), 1.0f);
}
