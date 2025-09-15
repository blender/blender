/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/image.h"

#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

/* Sky texture */

ccl_device float3 sky_radiance(KernelGlobals kg,
                               const bool multiple_scattering,
                               const float3 dir,
                               const uint32_t path_flag,
                               const ccl_private float *sky_data,
                               const uint texture_id)
{
  float3 pixel_bottom = make_float3(sky_data[0], sky_data[1], sky_data[2]);
  float3 pixel_top = make_float3(sky_data[3], sky_data[4], sky_data[5]);
  const float sun_elevation = sky_data[6];
  const float sun_rotation = sky_data[7];
  const float angular_diameter = sky_data[8];
  const float sun_intensity = sky_data[9];
  const float earth_intersection_angle = sky_data[10];
  const bool sun_disc = (angular_diameter >= 0.0f);
  const float2 direction = direction_to_spherical(dir);
  const float3 sun_dir = spherical_to_direction(sun_elevation - M_PI_2_F, sun_rotation - M_PI_2_F);
  const float sun_dir_angle = precise_angle(dir, sun_dir);
  const float half_angular = angular_diameter * 0.5f;
  const float dir_elevation = M_PI_2_F - direction.x;
  float3 rgb_sun = make_float3(0.0f, 0.0f, 0.0f);

  /* If the ray is inside the Sun disc and is not occluded by Earth's surface, render it, otherwise
   * render the sky. Alternatively, ignore the Sun if we're evaluating the background texture. */
  if (sun_disc && sun_dir_angle < half_angular &&
      !((path_flag & PATH_RAY_IMPORTANCE_BAKE) && kernel_data.background.use_sun_guiding) &&
      dir_elevation > earth_intersection_angle)
  {
    const float y = ((dir_elevation - sun_elevation) / angular_diameter) + 0.5f;
    const float3 xyz = interp(pixel_bottom, pixel_top, y);
    /* Limb darkening, coefficient is 0.6 */
    const float limb_darkening = (1.0f -
                                  0.6f * (1.0f - sqrtf(1.0f - sqr(sun_dir_angle / half_angular))));
    rgb_sun = xyz_to_rgb_clamped(kg, xyz) * limb_darkening;
  }
  const float x = fractf((-direction.y - M_PI_2_F + sun_rotation) / M_2PI_F);
  float3 rgb_sky;
  if (!multiple_scattering && dir.z < 0.0f) {
    /* Fade ground to black for Single Scattering model and disable Sun disc below horizon */
    rgb_sun = make_float3(0.0f, 0.0f, 0.0f);
    if (dir.z < -0.4f) {
      rgb_sky = make_float3(0.0f, 0.0f, 0.0f);
    }
    else {
      float fade = powf(1.0f + dir.z * 2.5f, 3.0f);
      const float3 xyz = make_float3(kernel_tex_image_interp(kg, texture_id, x, 0.508f));
      rgb_sky = xyz_to_rgb_clamped(kg, xyz) * fade;
    }
  }
  else {
    /* Undo the non-linear transformation from the sky LUT */
    const float dir_elevation_abs = (dir_elevation < 0.0f) ? -dir_elevation : dir_elevation;
    const float y = sqrtf(dir_elevation_abs / M_PI_2_F) * copysignf(1.0f, dir_elevation) * 0.5f +
                    0.5f;
    const float3 xyz = make_float3(kernel_tex_image_interp(kg, texture_id, x, y));
    rgb_sky = xyz_to_rgb_clamped(kg, xyz);
  }

  return rgb_sun * sun_intensity + rgb_sky;
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
  const bool multiple_scattering = node.w;
  float sky_data[11];
  const float3 dir = stack_load_float3(stack, dir_offset);
  float4 data = read_node_float(kg, &offset);
  sky_data[0] = data.x;
  sky_data[1] = data.y;
  sky_data[2] = data.z;
  sky_data[3] = data.w;
  data = read_node_float(kg, &offset);
  sky_data[4] = data.x;
  sky_data[5] = data.y;
  sky_data[6] = data.z;
  sky_data[7] = data.w;
  data = read_node_float(kg, &offset);
  sky_data[8] = data.x;
  sky_data[9] = data.y;
  sky_data[10] = data.z;
  const uint texture_id = __float_as_uint(data.w);

  /* Compute Sky */
  float3 rgb = sky_radiance(kg, multiple_scattering, dir, path_flag, sky_data, texture_id);

  stack_store_float3(stack, out_offset, rgb);
  return offset;
}

CCL_NAMESPACE_END
