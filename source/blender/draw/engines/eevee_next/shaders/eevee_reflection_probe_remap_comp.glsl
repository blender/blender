/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_mapping_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

shared vec4 local_radiance[gl_WorkGroupSize.x * gl_WorkGroupSize.y];

float triangle_solid_angle(vec3 A, vec3 B, vec3 C)
{
  return 2.0 * atan(abs(dot(A, cross(B, C))), (1.0 + dot(B, C) + dot(A, C) + dot(A, B)));
}

float quad_solid_angle(vec3 A, vec3 B, vec3 C, vec3 D)
{
  return triangle_solid_angle(A, B, C) + triangle_solid_angle(C, B, D);
}

float octahedral_texel_solid_angle(ivec2 local_texel,
                                   SphereProbePixelArea write_co,
                                   SphereProbeUvArea sample_co)
{
  if (any(equal(local_texel, ivec2(write_co.extent - 1)))) {
    /* Do not weight these border pixels that are redundant. */
    return 0.0;
  }
  /* Since we are pouting texel centers on the edges of the octahedron, the shape of a texel can be
   * anything from a simple quad (at the Z=0 poles), to a 4 pointed start (at the Z=+-1 poles)
   * passing by arrow tail shapes (at the X=0 and Y=0 edges). So while it would be more correct to
   * account for all these shapes (using 8 triangles), it proves to be quite involved with all the
   * corner cases. Instead, we compute the area as if the texels were not aligned with the edges.
   * This simplify things at the cost of making the weighting a tiny bit off for every pixels.
   * The sum of all texels is still giving 4 PI. */
  vec3 v00 = sphere_probe_texel_to_direction(local_texel + ivec2(-1, -1), write_co, sample_co);
  vec3 v10 = sphere_probe_texel_to_direction(local_texel + ivec2(+0, -1), write_co, sample_co);
  vec3 v20 = sphere_probe_texel_to_direction(local_texel + ivec2(-1, -1), write_co, sample_co);
  vec3 v01 = sphere_probe_texel_to_direction(local_texel + ivec2(-1, +0), write_co, sample_co);
  vec3 v11 = sphere_probe_texel_to_direction(local_texel + ivec2(+0, +0), write_co, sample_co);
  vec3 v21 = sphere_probe_texel_to_direction(local_texel + ivec2(+1, +0), write_co, sample_co);
  vec3 v02 = sphere_probe_texel_to_direction(local_texel + ivec2(-1, +1), write_co, sample_co);
  vec3 v12 = sphere_probe_texel_to_direction(local_texel + ivec2(+0, +1), write_co, sample_co);
  vec3 v22 = sphere_probe_texel_to_direction(local_texel + ivec2(+1, +1), write_co, sample_co);
  /* The solid angle functions expect normalized vectors. */
  v00 = normalize(v00);
  v10 = normalize(v10);
  v20 = normalize(v20);
  v01 = normalize(v01);
  v11 = normalize(v11);
  v21 = normalize(v21);
  v02 = normalize(v02);
  v12 = normalize(v12);
  v22 = normalize(v22);
#if 0 /* Has artifacts, is marginally more correct. */
  /* For some reason quad_solid_angle(v10, v20, v11, v21) gives some strange artifacts at Z=0. */
  return 0.25 * (quad_solid_angle(v00, v10, v01, v11) + quad_solid_angle(v10, v20, v11, v21) +
                 quad_solid_angle(v01, v11, v02, v12) + quad_solid_angle(v11, v21, v12, v22));
#else
  /* Choosing the positive quad (0,0) > (+1,+1) for stability. */
  return quad_solid_angle(v11, v21, v12, v22);
#endif
}

void main()
{
  SphereProbeUvArea world_coord = reinterpret_as_atlas_coord(world_coord_packed);
  SphereProbeUvArea sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  SphereProbePixelArea write_coord = reinterpret_as_write_coord(write_coord_packed);

  /* Texel in probe. */
  ivec2 local_texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 wrapped_uv;
  vec3 direction = sphere_probe_texel_to_direction(
      local_texel, write_coord, sample_coord, wrapped_uv);
  vec4 radiance_and_transmittance = texture(cubemap_tx, direction);
  vec3 radiance = radiance_and_transmittance.xyz;

  float opacity = 1.0 - radiance_and_transmittance.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(probe_coord_packed, world_coord_packed));
  if (!is_world && opacity != 1.0) {
    vec2 world_uv = wrapped_uv * world_coord.scale + world_coord.offset;
    vec4 world_radiance = textureLod(atlas_tx, vec3(world_uv, world_coord.layer), 0.0);
    radiance.rgb = mix(world_radiance.rgb, radiance.rgb, opacity);
  }

  radiance = colorspace_brightness_clamp_max(radiance, probe_brightness_clamp);

  if (!any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    ivec3 texel = ivec3(local_texel + write_coord.offset, write_coord.layer);
    imageStore(atlas_img, texel, vec4(radiance, 1.0));
  }

  if (extract_sh) {
    float sample_weight = octahedral_texel_solid_angle(local_texel, write_coord, sample_coord);

    const uint local_index = gl_LocalInvocationIndex;
    const uint group_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

    /* Parallel sum. Result is stored inside local_radiance[0]. */
    local_radiance[local_index] = radiance.xyzz * sample_weight;
    for (uint stride = group_size / 2; stride > 0; stride /= 2) {
      barrier();
      if (local_index < stride) {
        local_radiance[local_index] += local_radiance[local_index + stride];
      }
    }

    barrier();
    if (gl_LocalInvocationIndex == 0u) {
      /* Find the middle point of the whole thread-group. Use it as light vector.
       * Note that this is an approximation since the footprint of a thread-group is not
       * necessarily a convex polygons (with center of gravity at midpoint).
       * But the actual error introduce by this approximation is not perceivable. */
      ivec2 max_group_texel = local_texel + ivec2(gl_WorkGroupSize.xy);
      /* Min direction is the local direction since this is only ran by thread 0. */
      vec3 min_direction = normalize(direction);
      vec3 max_direction = normalize(
          sphere_probe_texel_to_direction(max_group_texel, write_coord, sample_coord));
      vec3 L = normalize(min_direction + max_direction);
      /* Convert radiance to spherical harmonics. */
      SphericalHarmonicL1 sh;
      sh.L0.M0 = vec4(0.0);
      sh.L1.Mn1 = vec4(0.0);
      sh.L1.M0 = vec4(0.0);
      sh.L1.Mp1 = vec4(0.0);
      /* TODO(fclem): Cleanup: Should spherical_harmonics_encode_signal_sample return a new sh
       * instead of adding to it? */
      spherical_harmonics_encode_signal_sample(L, local_radiance[0], sh);
      /* Outputs one SH for each thread-group. */
      uint work_group_index = gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;
      out_sh[work_group_index].L0_M0 = sh.L0.M0;
      out_sh[work_group_index].L1_Mn1 = sh.L1.Mn1;
      out_sh[work_group_index].L1_M0 = sh.L1.M0;
      out_sh[work_group_index].L1_Mp1 = sh.L1.Mp1;
    }
  }
}
