/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Sum all Suns extracting during remapping to octahedral map.
 * Dispatch only one thread-group that sums. */

#include "infos/eevee_lightprobe_sphere_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_sphere_sunlight)

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

shared float3 local_radiance[gl_WorkGroupSize.x];
shared float4 local_direction[gl_WorkGroupSize.x];

void main()
{
  SphereProbeSunLight sun;
  sun.radiance = float3(0.0f);
  sun.direction = float4(0.0f);

  /* First sum onto the local memory. */
  uint valid_data_len = probe_remap_dispatch_size.x * probe_remap_dispatch_size.y;
  constexpr uint iter_count = uint(SPHERE_PROBE_MAX_HARMONIC) / gl_WorkGroupSize.x;
  for (uint i = 0; i < iter_count; i++) {
    uint index = gl_WorkGroupSize.x * i + gl_LocalInvocationIndex;
    if (index >= valid_data_len) {
      break;
    }
    sun.radiance += in_sun[index].radiance;
    sun.direction += in_sun[index].direction;
  }

  /* Then sum across invocations. */
  const uint local_index = gl_LocalInvocationIndex;
  local_radiance[local_index] = sun.radiance;
  local_direction[local_index] = sun.direction;

  /* Parallel sum. */
  constexpr uint group_size = gl_WorkGroupSize.x;
  uint stride = group_size / 2;
  for (int i = 0; i < 10; i++) {
    barrier();
    if (local_index < stride) {
      local_radiance[local_index] += local_radiance[local_index + stride];
      local_direction[local_index] += local_direction[local_index + stride];
    }
    stride /= 2;
  }

  barrier();
  if (gl_LocalInvocationIndex == 0u) {
    sunlight_buf.color = local_radiance[0];

    /* Normalize the sum to get the mean direction. The length of the vector gives us the size of
     * the sun light. */
    float len;
    float3 direction = safe_normalize_and_get_length(local_direction[0].xyz / local_direction[0].w,
                                                     len);

    float3x3 tx = transpose(from_up_axis(direction));
    /* Convert to transform. */
    sunlight_buf.object_to_world.x = float4(tx[0], 0.0f);
    sunlight_buf.object_to_world.y = float4(tx[1], 0.0f);
    sunlight_buf.object_to_world.z = float4(tx[2], 0.0f);

    /* Auto sun angle. */
    float sun_angle_cos = 2.0f * len - 1.0f;
    /* Compute tangent from cosine. */
    float sun_angle_tan = sqrt(-1.0f + 1.0f / square(sun_angle_cos));
    /* Clamp value to avoid float imprecision artifacts. */
    float sun_radius = clamp(sun_angle_tan, 0.001f, 20.0f);

    /* Convert irradiance to radiance. */
    float shape_power = M_1_PI * (1.0f + 1.0f / square(sun_radius));
    float point_power = 1.0f;

    sunlight_buf.power[LIGHT_DIFFUSE] = shape_power;
    sunlight_buf.power[LIGHT_SPECULAR] = shape_power;
    sunlight_buf.power[LIGHT_TRANSMISSION] = shape_power;
    sunlight_buf.power[LIGHT_VOLUME] = point_power;

    /* NOTE: Use the radius from UI instead of auto sun size for now. */
  }
}
