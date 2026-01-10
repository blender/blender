/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/image.h"

#include "kernel/svm/types.h"
#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

/* Sky texture */

ccl_device float sky_angle_between(const float thetav,
                                   const float phiv,
                                   const float theta,
                                   const float phi)
{
  const float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
  return safe_acosf(cospsi);
}

/*
 * "A Practical Analytic Model for Daylight"
 * A. J. Preetham, Peter Shirley, Brian Smits
 */
ccl_device float sky_perez_function(const ccl_private float *lam,
                                    const float theta,
                                    const float gamma)
{
  const float ctheta = cosf(theta);
  const float cgamma = cosf(gamma);

  return (1.0f + lam[0] * expf(lam[1] / ctheta)) *
         (1.0f + lam[2] * expf(lam[3] * gamma) + lam[4] * cgamma * cgamma);
}

ccl_device float3 sky_radiance_preetham(KernelGlobals kg,
                                        const float3 dir,
                                        const float sunphi,
                                        const float suntheta,
                                        const float radiance_x,
                                        const float radiance_y,
                                        const float radiance_z,
                                        ccl_private float *config_x,
                                        ccl_private float *config_y,
                                        ccl_private float *config_z)
{
  /* convert vector to spherical coordinates */
  const float2 spherical = direction_to_spherical(dir);
  float theta = spherical.x;
  const float phi = -spherical.y + M_PI_2_F;

  /* angle between sun direction and dir */
  const float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2_F - 0.001f);

  /* compute xyY color space values */
  const float x = radiance_y * sky_perez_function(config_y, theta, gamma);
  const float y = radiance_z * sky_perez_function(config_z, theta, gamma);
  const float Y = radiance_x * sky_perez_function(config_x, theta, gamma);

  /* convert to RGB */
  const float3 xyz = xyY_to_xyz(x, y, Y);
  return xyz_to_rgb_clamped(kg, xyz);
}

/*
 * "An Analytic Model for Full Spectral Sky-Dome Radiance"
 * Lukas Hosek, Alexander Wilkie
 */
ccl_device float sky_radiance_internal(const ccl_private float *configuration,
                                       const float theta,
                                       const float gamma)
{
  const float ctheta = cosf(theta);
  const float cgamma = cosf(gamma);

  const float expM = expf(configuration[4] * gamma);
  const float rayM = cgamma * cgamma;
  const float mieM = (1.0f + rayM) / powf((1.0f + configuration[8] * configuration[8] -
                                           2.0f * configuration[8] * cgamma),
                                          1.5f);
  const float zenith = sqrtf(ctheta);

  return (1.0f + configuration[0] * expf(configuration[1] / (ctheta + 0.01f))) *
         (configuration[2] + configuration[3] * expM + configuration[5] * rayM +
          configuration[6] * mieM + configuration[7] * zenith);
}

ccl_device float3 sky_radiance_hosek(KernelGlobals kg,
                                     const float3 dir,
                                     const float sunphi,
                                     const float suntheta,
                                     const float radiance_x,
                                     const float radiance_y,
                                     const float radiance_z,
                                     ccl_private float *config_x,
                                     ccl_private float *config_y,
                                     ccl_private float *config_z)
{
  /* convert vector to spherical coordinates */
  const float2 spherical = direction_to_spherical(dir);
  float theta = spherical.x;
  const float phi = -spherical.y + M_PI_2_F;

  /* angle between sun direction and dir */
  const float gamma = sky_angle_between(theta, phi, suntheta, sunphi);

  /* clamp theta to horizon */
  theta = min(theta, M_PI_2_F - 0.001f);

  /* compute xyz color space values */
  const float x = sky_radiance_internal(config_x, theta, gamma) * radiance_x;
  const float y = sky_radiance_internal(config_y, theta, gamma) * radiance_y;
  const float z = sky_radiance_internal(config_z, theta, gamma) * radiance_z;

  /* convert to RGB and adjust strength */
  return xyz_to_rgb_clamped(kg, make_float3(x, y, z)) * (M_2PI_F / 683);
}

/* Nishita improved sky model */
ccl_device float3 geographical_to_direction(const float lat, const float lon)
{
  return spherical_to_direction(lat - M_PI_2_F, lon - M_PI_2_F);
}

ccl_device float3 sky_radiance_nishita(KernelGlobals kg,
                                       const float3 dir,
                                       const uint32_t path_flag,
                                       const float3 pixel_bottom,
                                       const float3 pixel_top,
                                       const ccl_private float *sky_data,
                                       const uint texture_id)
{
  /* Definitions */
  const float sun_elevation = sky_data[0];
  const float sun_rotation = sky_data[1];
  const float angular_diameter = sky_data[2];
  const float sun_intensity = sky_data[3];
  const float earth_intersection_angle = sky_data[4];
  const bool sun_disc = (angular_diameter >= 0.0f);
  float3 xyz = zero_float3();
  const float2 direction = direction_to_spherical(dir);
  const float3 sun_dir = spherical_to_direction(sun_elevation - M_PI_2_F, sun_rotation - M_PI_2_F);
  const float sun_dir_angle = precise_angle(dir, sun_dir);
  const float half_angular = angular_diameter * 0.5f;
  const float dir_elevation = M_PI_2_F - direction.x;

  /* If the ray is inside the Sun disc, render it, otherwise render the sky.
   * Alternatively, ignore the Sun if we're evaluating the background texture. */
  if (sun_disc && sun_dir_angle < half_angular && dir_elevation > earth_intersection_angle &&
      !((path_flag & PATH_RAY_IMPORTANCE_BAKE) && kernel_data.background.use_sun_guiding))
  {
    /* Sun interpolation */
    const float y = ((dir_elevation - sun_elevation) / angular_diameter) + 0.5f;
    /* Limb darkening, coefficient is 0.6f */
    const float limb_darkening = (1.0f -
                                  0.6f * (1.0f - sqrtf(1.0f - sqr(sun_dir_angle / half_angular))));
    xyz = mix(pixel_bottom, pixel_top, y) * sun_intensity * limb_darkening;
  }

  /* Sky */
  const float x = fractf((-direction.y - M_PI_2_F + sun_rotation) * M_1_2PI_F);
  /* Undo the non-linear transformation from the sky LUT */
  const float y = copysignf(sqrtf(fabsf(dir_elevation) * M_2_PI_F), dir_elevation) * 0.5f + 0.5f;
  xyz += make_float3(kernel_image_interp(kg, texture_id, x, y));

  /* Convert to RGB */
  return xyz_to_rgb_clamped(kg, xyz);
}

ccl_device_noinline int svm_node_tex_sky(KernelGlobals kg,
                                         const uint32_t path_flag,
                                         ccl_private float *stack,
                                         const uint4 node,
                                         int offset)
{
  /* Load data */
  const uint dir_offset = node.y;
  const uint out_offset = node.z;
  const NodeSkyType sky_type = NodeSkyType(node.w);

  const float3 dir = stack_load_float3(stack, dir_offset);
  float3 rgb;

  /* Preetham and Hosek share the same data */
  if (sky_type == NODE_SKY_PREETHAM || sky_type == NODE_SKY_HOSEK) {
    /* Define variables */
    float sunphi;
    float suntheta;
    float radiance_x;
    float radiance_y;
    float radiance_z;
    float config_x[9];
    float config_y[9];
    float config_z[9];

    float4 data = read_node_float(kg, &offset);
    sunphi = data.x;
    suntheta = data.y;
    radiance_x = data.z;
    radiance_y = data.w;

    data = read_node_float(kg, &offset);
    radiance_z = data.x;
    config_x[0] = data.y;
    config_x[1] = data.z;
    config_x[2] = data.w;

    data = read_node_float(kg, &offset);
    config_x[3] = data.x;
    config_x[4] = data.y;
    config_x[5] = data.z;
    config_x[6] = data.w;

    data = read_node_float(kg, &offset);
    config_x[7] = data.x;
    config_x[8] = data.y;
    config_y[0] = data.z;
    config_y[1] = data.w;

    data = read_node_float(kg, &offset);
    config_y[2] = data.x;
    config_y[3] = data.y;
    config_y[4] = data.z;
    config_y[5] = data.w;

    data = read_node_float(kg, &offset);
    config_y[6] = data.x;
    config_y[7] = data.y;
    config_y[8] = data.z;
    config_z[0] = data.w;

    data = read_node_float(kg, &offset);
    config_z[1] = data.x;
    config_z[2] = data.y;
    config_z[3] = data.z;
    config_z[4] = data.w;

    data = read_node_float(kg, &offset);
    config_z[5] = data.x;
    config_z[6] = data.y;
    config_z[7] = data.z;
    config_z[8] = data.w;

    /* Compute Sky */
    if (sky_type == NODE_SKY_PREETHAM) {
      rgb = sky_radiance_preetham(kg,
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
      rgb = sky_radiance_hosek(kg,
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
    float4 data = read_node_float(kg, &offset);
    const float3 pixel_bottom = make_float3(data.x, data.y, data.z);
    float3 pixel_top;
    pixel_top.x = data.w;

    float sky_data[5];
    data = read_node_float(kg, &offset);
    pixel_top.y = data.x;
    pixel_top.z = data.y;
    sky_data[0] = data.z;
    sky_data[1] = data.w;

    data = read_node_float(kg, &offset);
    sky_data[2] = data.x;
    sky_data[3] = data.y;
    sky_data[4] = data.z;
    const uint texture_id = __float_as_uint(data.w);

    /* Compute Sky */
    rgb = sky_radiance_nishita(kg, dir, path_flag, pixel_bottom, pixel_top, sky_data, texture_id);
  }

  stack_store_float3(stack, out_offset, rgb);
  return offset;
}

CCL_NAMESPACE_END
