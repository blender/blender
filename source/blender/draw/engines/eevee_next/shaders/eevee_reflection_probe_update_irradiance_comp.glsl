/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to extract spherical harmonics cooefs from octahedral mapped reflection probe. */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

void atlas_store(vec4 sh_coefficient, ivec2 atlas_coord, int layer)
{
  for (int x = 0; x < IRRADIANCE_GRID_BRICK_SIZE; x++) {
    for (int y = 0; y < IRRADIANCE_GRID_BRICK_SIZE; y++) {
      for (int z = 0; z < IRRADIANCE_GRID_BRICK_SIZE; z++) {
        ivec3 brick_coord = ivec3(x, y, z);
        imageStore(irradiance_atlas_img,
                   ivec3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + brick_coord,
                   sh_coefficient);
      }
    }
  }
}

shared vec4 cooefs[gl_WorkGroupSize.x][4];

void main()
{
  SphericalHarmonicL1 cooef;
  cooef.L0.M0 = vec4(0.0);
  cooef.L1.Mn1 = vec4(0.0);
  cooef.L1.M0 = vec4(0.0);
  cooef.L1.Mp1 = vec4(0.0);

  ReflectionProbeData probe_data = reflection_probe_buf[reflection_probe_index];
  const int subdivision_64 = 5;
  float layer_mipmap = clamp(
      subdivision_64 - probe_data.layer_subdivision, 0, REFLECTION_PROBE_MIPMAP_LEVELS);

  /* Perform multiple sample. */
  uint store_index = gl_LocalInvocationID.x;
  float total_samples = float(gl_WorkGroupSize.x * REFLECTION_PROBE_SH_SAMPLES_PER_GROUP);
  float sample_weight = 4.0 * M_PI / total_samples;
  float sample_offset = float(gl_LocalInvocationID.x * REFLECTION_PROBE_SH_SAMPLES_PER_GROUP);
  for (int sample_index = 0; sample_index < REFLECTION_PROBE_SH_SAMPLES_PER_GROUP; sample_index++)
  {
    vec2 rand = fract(hammersley_2d(sample_index + sample_offset, total_samples));
    vec3 direction = sample_sphere(rand);
    vec4 light = reflection_probes_sample(direction, layer_mipmap, probe_data);
    spherical_harmonics_encode_signal_sample(direction, light * sample_weight, cooef);
  }
  cooefs[store_index][0] = cooef.L0.M0;
  cooefs[store_index][1] = cooef.L1.Mn1;
  cooefs[store_index][2] = cooef.L1.M0;
  cooefs[store_index][3] = cooef.L1.Mp1;

  barrier();
  if (gl_LocalInvocationID.x == 0) {
    /* Join results */
    vec4 result[4];
    result[0] = vec4(0.0);
    result[1] = vec4(0.0);
    result[2] = vec4(0.0);
    result[3] = vec4(0.0);

    for (uint i = 0; i < gl_WorkGroupSize.x; i++) {
      result[0] += cooefs[i][0];
      result[1] += cooefs[i][1];
      result[2] += cooefs[i][2];
      result[3] += cooefs[i][3];
    }

    ivec2 atlas_coord = ivec2(0, 0);
    atlas_store(result[0], atlas_coord, 0);
    atlas_store(result[1], atlas_coord, 1);
    atlas_store(result[2], atlas_coord, 2);
    atlas_store(result[3], atlas_coord, 3);
  }
}
