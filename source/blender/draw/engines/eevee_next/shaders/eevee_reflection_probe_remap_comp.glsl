/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

ReflectionProbeCoordinate reinterpret_as_atlas_coord(ivec4 packed_coord)
{
  ReflectionProbeCoordinate unpacked;
  unpacked.offset = intBitsToFloat(packed_coord.xy);
  unpacked.scale = intBitsToFloat(packed_coord.z);
  unpacked.layer = intBitsToFloat(packed_coord.w);
  return unpacked;
}

ReflectionProbeWriteCoordinate reinterpret_as_write_coord(ivec4 packed_coord)
{
  ReflectionProbeWriteCoordinate unpacked;
  unpacked.offset = packed_coord.xy;
  unpacked.extent = packed_coord.z;
  unpacked.layer = packed_coord.w;
  return unpacked;
}

void main()
{
  ReflectionProbeCoordinate sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  ReflectionProbeWriteCoordinate write_coord = reinterpret_as_write_coord(write_coord_packed);
  ReflectionProbeWriteCoordinate world_coord = reinterpret_as_write_coord(world_coord_packed);

  /* Texel in probe. */
  ivec2 local_texel = ivec2(gl_GlobalInvocationID.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    return;
  }

  /* Texel in probe atlas. */
  ivec2 texel = local_texel + write_coord.offset;
  /* UV in probe atlas. */
  vec2 atlas_uv = (vec2(texel) + 0.5) / vec2(imageSize(atlas_dst_mip_img).xy);
  /* UV in sampling area. */
  vec2 sampling_uv = (atlas_uv - sample_coord.offset) / sample_coord.scale;
  /* Direction in world space. */
  vec3 direction = octahedral_uv_to_direction(sampling_uv);
  vec4 col = textureLod(cubemap_tx, direction, float(mip_level));

  /* Convert transmittance to transparency. */
  col.a = 1.0 - col.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(write_coord_packed, world_coord_packed));
  if (!is_world && col.a != 1.0) {
    ivec2 world_texel = local_texel + world_coord.offset;
    vec4 world_col = imageLoad(atlas_src_mip_img, ivec3(world_texel, world_coord.layer));
    col.rgb = mix(world_col.rgb, col.rgb, col.a);
  }

  imageStore(atlas_dst_mip_img, ivec3(texel, write_coord.layer), col);
}
