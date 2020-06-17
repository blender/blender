/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Sky texture */

ccl_device float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
  float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
  return safe_acosf(cospsi);
}

/*
 * "A Practical Analytic Model for Daylight"
 * A. J. Preetham, Peter Shirley, Brian Smits
 */
ccl_device float sky_perez_function(float *lam, float theta, float gamma)
{
  float ctheta = cosf(theta);
  float cgamma = cosf(gamma);

  return (1.0f + lam[0] * expf(lam[1] / ctheta)) *
         (1.0f + lam[2] * expf(lam[3] * gamma) + lam[4] * cgamma * cgamma);
}

ccl_device float3 sky_radiance_preetham(KernelGlobals *kg,
                                        float3 dir,
                                        float sunphi,
                                        float suntheta,
                                        float radiance_x,
                                        float radiance_y,
                                        float radiance_z,
                                        float *config_x,
                                        float *config_y,
                                        float *config_z)
{
  /* convert vector to spherical coordinates */
  float2 spherical = direction_to_spherical(dir);
  float theta = spherical.x;
  float phi = spherical.y;

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2_F - 0.001f);

  /* compute xyY color space values */
  float x = radiance_y * sky_perez_function(config_y, theta, gamma);
  float y = radiance_z * sky_perez_function(config_z, theta, gamma);
  float Y = radiance_x * sky_perez_function(config_x, theta, gamma);

  /* convert to RGB */
  float3 xyz = xyY_to_xyz(x, y, Y);
  return xyz_to_rgb(kg, xyz);
}

/*
 * "An Analytic Model for Full Spectral Sky-Dome Radiance"
 * Lukas Hosek, Alexander Wilkie
 */
ccl_device float sky_radiance_internal(float *configuration, float theta, float gamma)
{
  float ctheta = cosf(theta);
  float cgamma = cosf(gamma);

  float expM = expf(configuration[4] * gamma);
  float rayM = cgamma * cgamma;
  float mieM = (1.0f + rayM) / powf((1.0f + configuration[8] * configuration[8] -
                                     2.0f * configuration[8] * cgamma),
                                    1.5f);
  float zenith = sqrtf(ctheta);

  return (1.0f + configuration[0] * expf(configuration[1] / (ctheta + 0.01f))) *
         (configuration[2] + configuration[3] * expM + configuration[5] * rayM +
          configuration[6] * mieM + configuration[7] * zenith);
}

ccl_device float3 sky_radiance_hosek(KernelGlobals *kg,
                                     float3 dir,
                                     float sunphi,
                                     float suntheta,
                                     float radiance_x,
                                     float radiance_y,
                                     float radiance_z,
                                     float *config_x,
                                     float *config_y,
                                     float *config_z)
{
  /* convert vector to spherical coordinates */
  float2 spherical = direction_to_spherical(dir);
  float theta = spherical.x;
  float phi = spherical.y;

  /* angle between sun direction and dir */
  float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2_F - 0.001f);

  /* compute xyz color space values */
  float x = sky_radiance_internal(config_x, theta, gamma) * radiance_x;
  float y = sky_radiance_internal(config_y, theta, gamma) * radiance_y;
  float z = sky_radiance_internal(config_z, theta, gamma) * radiance_z;

  /* convert to RGB and adjust strength */
  return xyz_to_rgb(kg, make_float3(x, y, z)) * (M_2PI_F / 683);
}

/* Nishita improved sky model */
ccl_device float3 geographical_to_direction(float lat, float lon)
{
  return make_float3(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat));
}

ccl_device float3 sky_radiance_nishita(KernelGlobals *kg,
                                       float3 dir,
                                       float *nishita_data,
                                       uint texture_id)
{
  /* definitions */
  float sun_elevation = nishita_data[6];
  float sun_rotation = nishita_data[7];
  float angular_diameter = nishita_data[8];
  bool sun_disc = (angular_diameter > 0.0f);
  float3 xyz;
  /* convert dir to spherical coordinates */
  float2 direction = direction_to_spherical(dir);

  /* render above the horizon */
  if (dir.z >= 0.0f) {
    /* definitions */
    float3 sun_dir = geographical_to_direction(sun_elevation, sun_rotation + M_PI_2_F);
    float sun_dir_angle = acos(dot(dir, sun_dir));
    float half_angular = angular_diameter / 2.0f;
    float dir_elevation = M_PI_2_F - direction.x;

    /* if ray inside sun disc render it, otherwise render sky */
    if (sun_disc && sun_dir_angle < half_angular) {
      /* get 3 pixels data */
      float3 pixel_bottom = make_float3(nishita_data[0], nishita_data[1], nishita_data[2]);
      float3 pixel_top = make_float3(nishita_data[3], nishita_data[4], nishita_data[5]);
      float y;

      /* sun interpolation */
      if (sun_elevation - half_angular > 0.0f) {
        if (sun_elevation + half_angular > 0.0f) {
          y = ((dir_elevation - sun_elevation) / angular_diameter) + 0.5f;
          xyz = interp(pixel_bottom, pixel_top, y);
        }
      }
      else {
        if (sun_elevation + half_angular > 0.0f) {
          y = dir_elevation / (sun_elevation + half_angular);
          xyz = interp(pixel_bottom, pixel_top, y);
        }
      }
      /* limb darkening, coefficient is 0.6f */
      float limb_darkening = (1.0f -
                              0.6f * (1.0f - sqrtf(1.0f - sqr(sun_dir_angle / half_angular))));
      xyz *= limb_darkening;
    }
    /* sky */
    else {
      /* sky interpolation */
      float x = (direction.y + M_PI_F + sun_rotation) / M_2PI_F;
      float y = dir_elevation / M_PI_2_F;
      if (x > 1.0f) {
        x -= 1.0f;
      }
      xyz = float4_to_float3(kernel_tex_image_interp(kg, texture_id, x, y));
    }
  }
  /* ground */
  else {
    if (dir.z < -0.4f) {
      xyz = make_float3(0.0f, 0.0f, 0.0f);
    }
    else {
      /* black ground fade */
      float fade = 1.0f + dir.z * 2.5f;
      fade = sqr(fade) * fade;
      /* interpolation */
      float x = (direction.y + M_PI_F + sun_rotation) / M_2PI_F;
      if (x > 1.0f) {
        x -= 1.0f;
      }
      xyz = float4_to_float3(kernel_tex_image_interp(kg, texture_id, x, -0.5)) * fade;
    }
  }

  /* convert to rgb and adjust strength */
  return xyz_to_rgb(kg, xyz) * 120000.0f;
}

ccl_device void svm_node_tex_sky(
    KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
  /* Load data */
  uint dir_offset = node.y;
  uint out_offset = node.z;
  int sky_model = node.w;

  float3 dir = stack_load_float3(stack, dir_offset);
  float3 f;

  /* Preetham and Hosek share the same data */
  if (sky_model == 0 || sky_model == 1) {
    /* Define variables */
    float sunphi, suntheta, radiance_x, radiance_y, radiance_z;
    float config_x[9], config_y[9], config_z[9];

    float4 data = read_node_float(kg, offset);
    sunphi = data.x;
    suntheta = data.y;
    radiance_x = data.z;
    radiance_y = data.w;

    data = read_node_float(kg, offset);
    radiance_z = data.x;
    config_x[0] = data.y;
    config_x[1] = data.z;
    config_x[2] = data.w;

    data = read_node_float(kg, offset);
    config_x[3] = data.x;
    config_x[4] = data.y;
    config_x[5] = data.z;
    config_x[6] = data.w;

    data = read_node_float(kg, offset);
    config_x[7] = data.x;
    config_x[8] = data.y;
    config_y[0] = data.z;
    config_y[1] = data.w;

    data = read_node_float(kg, offset);
    config_y[2] = data.x;
    config_y[3] = data.y;
    config_y[4] = data.z;
    config_y[5] = data.w;

    data = read_node_float(kg, offset);
    config_y[6] = data.x;
    config_y[7] = data.y;
    config_y[8] = data.z;
    config_z[0] = data.w;

    data = read_node_float(kg, offset);
    config_z[1] = data.x;
    config_z[2] = data.y;
    config_z[3] = data.z;
    config_z[4] = data.w;

    data = read_node_float(kg, offset);
    config_z[5] = data.x;
    config_z[6] = data.y;
    config_z[7] = data.z;
    config_z[8] = data.w;

    /* Compute Sky */
    if (sky_model == 0) {
      f = sky_radiance_preetham(kg,
                                dir,
                                sunphi,
                                suntheta,
                                radiance_x,
                                radiance_y,
                                radiance_z,
                                config_x,
                                config_y,
                                config_z);
    }
    else {
      f = sky_radiance_hosek(kg,
                             dir,
                             sunphi,
                             suntheta,
                             radiance_x,
                             radiance_y,
                             radiance_z,
                             config_x,
                             config_y,
                             config_z);
    }
  }
  /* Nishita */
  else {
    /* Define variables */
    float nishita_data[9];

    float4 data = read_node_float(kg, offset);
    nishita_data[0] = data.x;
    nishita_data[1] = data.y;
    nishita_data[2] = data.z;
    nishita_data[3] = data.w;

    data = read_node_float(kg, offset);
    nishita_data[4] = data.x;
    nishita_data[5] = data.y;
    nishita_data[6] = data.z;
    nishita_data[7] = data.w;

    data = read_node_float(kg, offset);
    nishita_data[8] = data.x;
    uint texture_id = __float_as_uint(data.y);

    /* Compute Sky */
    f = sky_radiance_nishita(kg, dir, nishita_data, texture_id);
  }

  stack_store_float3(stack, out_offset, f);
}

CCL_NAMESPACE_END
