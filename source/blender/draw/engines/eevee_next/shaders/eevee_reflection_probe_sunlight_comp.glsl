/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Sum all Suns extracting during remapping to octahedral map.
 * Dispatch only one thread-group that sums. */

#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_mapping_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

shared vec3 local_radiance[gl_WorkGroupSize.x];
shared vec4 local_direction[gl_WorkGroupSize.x];

void main()
{
  SphereProbeSunLight sun;
  sun.radiance = vec3(0.0);
  sun.direction = vec4(0.0);

  /* First sum onto the local memory. */
  uint valid_data_len = probe_remap_dispatch_size.x * probe_remap_dispatch_size.y;
  const uint iter_count = uint(SPHERE_PROBE_MAX_HARMONIC) / gl_WorkGroupSize.x;
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
  const uint group_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
  for (uint stride = group_size / 2; stride > 0; stride /= 2) {
    barrier();
    if (local_index < stride) {
      local_radiance[local_index] += local_radiance[local_index + stride];
      local_direction[local_index] += local_direction[local_index + stride];
    }
  }

  barrier();
  if (gl_LocalInvocationIndex == 0u) {
    sunlight_buf.color = local_radiance[0];

    /* Normalize the sum to get the mean direction. The length of the vector gives us the size of
     * the sun light. */
    float len;
    vec3 direction = normalize_and_get_length(local_direction[0].xyz / local_direction[0].w, len);

    mat3x3 tx = transpose(from_up_axis(direction));
    /* Convert to transform. */
    sunlight_buf.object_to_world.x = vec4(tx[0], 0.0);
    sunlight_buf.object_to_world.y = vec4(tx[1], 0.0);
    sunlight_buf.object_to_world.z = vec4(tx[2], 0.0);

    /* Auto sun angle. */
    float sun_angle_cos = 2.0 * len - 1.0;
    float sun_angle = acos(sun_angle_cos);
    /* Compute tangent from cosine.  */
    float sun_angle_tan = sqrt(-1.0 + 1.0 / square(sun_angle_cos));
    /* Clamp value to avoid float imprecision artifacts. */
    float sun_radius = clamp(sun_angle_tan, 0.001, 20.0);

    /* Convert irradiance to radiance. */
    float shape_power = M_1_PI * (1.0 + 1.0 / square(sun_radius));
    float point_power = 1.0;

    sunlight_buf.power[LIGHT_DIFFUSE] = shape_power;
    sunlight_buf.power[LIGHT_SPECULAR] = shape_power;
    sunlight_buf.power[LIGHT_TRANSMISSION] = shape_power;
    sunlight_buf.power[LIGHT_VOLUME] = point_power;

#if USE_LIGHT_UNION
    sunlight_buf.sun.radius = sun_radius;
    sunlight_buf.sun.shadow_angle = sun_angle;
#else
    sunlight_buf.do_not_access_directly.radius_squared = sun_radius;
    sunlight_buf.do_not_access_directly._pad1 = sun_angle;
#endif
  }
}
