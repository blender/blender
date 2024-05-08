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
  /* Since we are putting texel centers on the edges of the octahedron, the shape of a texel can be
   * anything from a simple quad (at the Z=0 poles), to a 4 pointed start (at the Z=+-1 poles)
   * passing by arrow tail shapes (at the X=0 and Y=0 edges).
   *
   * But we can leverage the symmetries of the octahedral mapping. Given that all oddly shaped
   * texels are on the X=0 and Y=0 planes, we first fold all texels to the first quadrant.
   *
   * The texel footprint clipped to a quadrant is a well defined spherical quad. We then multiply
   * by the number of clipped shape the real texel sustains. This number is 2 at the X=0 and Y=0
   * edges (the arrow shaped pixels) and 4 at the Z=+-1 poles (4 pointed start shaped pixels).
   *
   * The sum of all texels solid angle should be 4 PI (area of sphere). */

  /* Wrap to bottom left quadrant. */
  int half_size = write_co.extent >> 1;
  int padded_size = write_co.extent - 2;
  ivec2 wrapped_texel;
  wrapped_texel.x = (local_texel.x >= half_size) ? (padded_size - local_texel.x) : local_texel.x;
  wrapped_texel.y = (local_texel.y >= half_size) ? (padded_size - local_texel.y) : local_texel.y;

  vec2 texel_corner_v00 = vec2(wrapped_texel) + vec2(-0.5, -0.5);
  vec2 texel_corner_v10 = vec2(wrapped_texel) + vec2(+0.5, -0.5);
  vec2 texel_corner_v01 = vec2(wrapped_texel) + vec2(-0.5, +0.5);
  vec2 texel_corner_v11 = vec2(wrapped_texel) + vec2(+0.5, +0.5);
  /* Clamp to well defined shape in spherical domain. */
  texel_corner_v00 = clamp(texel_corner_v00, vec2(0.0), vec2(half_size - 1));
  texel_corner_v10 = clamp(texel_corner_v10, vec2(0.0), vec2(half_size - 1));
  texel_corner_v01 = clamp(texel_corner_v01, vec2(0.0), vec2(half_size - 1));
  texel_corner_v11 = clamp(texel_corner_v11, vec2(0.0), vec2(half_size - 1));
  /* Convert to point on sphere. */
  vec3 v00 = sphere_probe_texel_to_direction(texel_corner_v00, write_co, sample_co);
  vec3 v10 = sphere_probe_texel_to_direction(texel_corner_v10, write_co, sample_co);
  vec3 v01 = sphere_probe_texel_to_direction(texel_corner_v01, write_co, sample_co);
  vec3 v11 = sphere_probe_texel_to_direction(texel_corner_v11, write_co, sample_co);
  /* The solid angle functions expect normalized vectors. */
  v00 = normalize(v00);
  v10 = normalize(v10);
  v01 = normalize(v01);
  v11 = normalize(v11);
  /* Compute solid angle of the spherical quad. */
  float texel_clipped_solid_angle = quad_solid_angle(v00, v10, v01, v11);
  /* Multiply by the symmetric halves that we omitted.
   * Also important to note that we avoid weighting the same pixel more than it's total sampled
   * footprint if it is duplicated in another pixel of the map. So border pixels do not require any
   * special treatment. Only the center cross needs it. */
  if (wrapped_texel.x == half_size - 1) {
    texel_clipped_solid_angle *= 2.0;
  }
  if (wrapped_texel.y == half_size - 1) {
    texel_clipped_solid_angle *= 2.0;
  }
  return texel_clipped_solid_angle;
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
      vec2(local_texel), write_coord, sample_coord, wrapped_uv);
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

  float clamp_world = uniform_buf.clamp.world;
  radiance = colorspace_brightness_clamp_max(radiance, clamp_world);

  if (!any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    vec3 out_radiance = colorspace_brightness_clamp_max(radiance, clamp_indirect);

    ivec3 texel = ivec3(local_texel + write_coord.offset, write_coord.layer);
    imageStore(atlas_img, texel, vec4(out_radiance, 1.0));
  }

  if (extract_sh) {
    float sample_weight = octahedral_texel_solid_angle(local_texel, write_coord, sample_coord);

    const uint local_index = gl_LocalInvocationIndex;
    const uint group_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

    /* Parallel sum. Result is stored inside local_radiance[0]. */
    local_radiance[local_index] = radiance.xyzz * sample_weight;
    uint stride = group_size / 2;
    for (int i = 0; i < 10; i++) {
      barrier();
      if (local_index < stride) {
        local_radiance[local_index] += local_radiance[local_index + stride];
      }
      stride /= 2;
    }

    barrier();
    if (gl_LocalInvocationIndex == 0u) {
      /* Find the middle point of the whole thread-group. Use it as light vector.
       * Note that this is an approximation since the footprint of a thread-group is not
       * necessarily a convex polygons (with center of gravity at midpoint).
       * But the actual error introduce by this approximation is not perceivable. */
      /* FIXME(fclem): The error IS very perceivable for resolution lower than a quadrant. */
      ivec2 max_group_texel = local_texel + ivec2(gl_WorkGroupSize.xy);
      /* Min direction is the local direction since this is only ran by thread 0. */
      vec3 min_direction = normalize(direction);
      vec3 max_direction = normalize(
          sphere_probe_texel_to_direction(vec2(max_group_texel), write_coord, sample_coord));
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
