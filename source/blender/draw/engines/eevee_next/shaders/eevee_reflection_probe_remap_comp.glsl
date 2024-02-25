/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_mapping_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)

void main()
{
  SphereProbeUvArea world_coord = reinterpret_as_atlas_coord(world_coord_packed);
  SphereProbeUvArea sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  SphereProbePixelArea write_coord = reinterpret_as_write_coord(write_coord_packed);

  /* Texel in probe. */
  ivec2 local_texel = ivec2(gl_GlobalInvocationID.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    return;
  }

  vec2 wrapped_uv;
  vec3 direction = sphere_probe_texel_to_direction(
      local_texel, write_coord, sample_coord, wrapped_uv);
  vec4 radiance_and_transmittance = texture(cubemap_tx, direction);
  vec3 radiance = radiance_and_transmittance.xyz;

  float opacity = 1.0 - radiance_and_transmittance.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(write_coord_packed, world_coord_packed));
  if (!is_world && opacity != 1.0) {
    vec2 world_uv = wrapped_uv * world_coord.scale + world_coord.offset;
    vec4 world_radiance = textureLod(atlas_tx, vec3(world_uv, world_coord.layer), 0.0);
    radiance.rgb = mix(world_radiance.rgb, radiance.rgb, opacity);
  }

  radiance = colorspace_brightness_clamp_max(radiance, probe_brightness_clamp);

  ivec3 texel = ivec3(local_texel + write_coord.offset, write_coord.layer);
  imageStore(atlas_img, texel, vec4(radiance, 1.0));
}
