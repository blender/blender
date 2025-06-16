/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/image.h"

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
                                       const ccl_private float *nishita_data,
                                       const uint texture_id)
{
  /* definitions */
  const float sun_elevation = nishita_data[0];
  const float sun_rotation = nishita_data[1];
  const float angular_diameter = nishita_data[2];
  const float sun_intensity = nishita_data[3];
  const bool sun_disc = (angular_diameter >= 0.0f);
  float3 xyz;
  /* convert dir to spherical coordinates */
  const float2 direction = direction_to_spherical(dir);
  /* render above the horizon */
  if (dir.z >= 0.0f) {
    /* definitions */
    const float3 sun_dir = geographical_to_direction(sun_elevation, sun_rotation);
    const float sun_dir_angle = precise_angle(dir, sun_dir);
    const float half_angular = angular_diameter * 0.5f;
    const float dir_elevation = M_PI_2_F - direction.x;

    /* If the ray is inside the sun disc, render it, otherwise render the sky.
     * Alternatively, ignore the sun if we're evaluating the background texture. */
    if (sun_disc && sun_dir_angle < half_angular &&
        !((path_flag & PATH_RAY_IMPORTANCE_BAKE) && kernel_data.background.use_sun_guiding))
    {
      /* get 2 pixels data */
      float y;

      /* sun interpolation */
      if (sun_elevation - half_angular > 0.0f) {
        if (sun_elevation + half_angular > 0.0f) {
          y = ((dir_elevation - sun_elevation) / angular_diameter) + 0.5f;
          xyz = interp(pixel_bottom, pixel_top, y) * sun_intensity;
        }
      }
      else {
        if (sun_elevation + half_angular > 0.0f) {
          y = dir_elevation / (sun_elevation + half_angular);
          xyz = interp(pixel_bottom, pixel_top, y) * sun_intensity;
        }
      }
      /* limb darkening, coefficient is 0.6f */
      const float limb_darkening = (1.0f - 0.6f * (1.0f - sqrtf(1.0f - sqr(sun_dir_angle /
                                                                           half_angular))));
      xyz *= limb_darkening;
    }
    /* sky */
    else {
      /* sky interpolation */
      const float x = fractf((-direction.y - M_PI_2_F + sun_rotation) / M_2PI_F);
      /* more pixels toward horizon compensation */
      const float y = safe_sqrtf(dir_elevation / M_PI_2_F);
      xyz = make_float3(kernel_tex_image_interp(kg, texture_id, x, y));
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
      const float x = fractf((-direction.y - M_PI_2_F + sun_rotation) / M_2PI_F);
      xyz = make_float3(kernel_tex_image_interp(kg, texture_id, x, -0.5)) * fade;
    }
  }

  /* convert to RGB */
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

  const float3 dir = stack_load_float3(stack, dir_offset);
  float3 f;

  /* Nishita */
  /* Define variables */
  float nishita_data[4];

  float4 data = read_node_float(kg, &offset);
  const float3 pixel_bottom = make_float3(data.x, data.y, data.z);
  float3 pixel_top;
  pixel_top.x = data.w;

  data = read_node_float(kg, &offset);
  pixel_top.y = data.x;
  pixel_top.z = data.y;
  nishita_data[0] = data.z;
  nishita_data[1] = data.w;

  data = read_node_float(kg, &offset);
  nishita_data[2] = data.x;
  nishita_data[3] = data.y;
  const uint texture_id = __float_as_uint(data.z);

  /* Compute Sky */
  f = sky_radiance_nishita(kg, dir, path_flag, pixel_bottom, pixel_top, nishita_data, texture_id);

  stack_store_float3(stack, out_offset, f);
  return offset;
}

CCL_NAMESPACE_END
