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

/* Mirror the UV if they are not on the diagonal or unit UV squares.
 * Doesn't extend outside of [-1..2] range. But this is fine since we use it only for borders. */
vec2 mirror_repeat_uv(vec2 uv)
{
  vec2 m = abs(uv - 0.5) + 0.5;
  vec2 f = floor(m);
  float x = f.x - f.y;
  if (x != 0.0) {
    uv.xy = 1.0 - uv.xy;
  }
  return fract(uv);
}

void main()
{
  ReflectionProbeCoordinate world_coord = reinterpret_as_atlas_coord(world_coord_packed);
  ReflectionProbeCoordinate sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  ReflectionProbeWriteCoordinate write_coord = reinterpret_as_write_coord(write_coord_packed);

  /* Texel in probe. */
  ivec2 local_texel = ivec2(gl_GlobalInvocationID.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    return;
  }

  /* Texel in probe atlas. */
  ivec2 texel = local_texel + write_coord.offset;
  /* UV in probe atlas. */
  vec2 atlas_uv = (vec2(texel) + 0.5) / vec2(imageSize(atlas_img).xy);
  /* UV in sampling area. */
  vec2 sampling_uv = (atlas_uv - sample_coord.offset) / sample_coord.scale;
  vec2 wrapped_uv = mirror_repeat_uv(sampling_uv);
  /* Direction in world space. */
  vec3 direction = octahedral_uv_to_direction(wrapped_uv);
  vec4 col = textureLod(cubemap_tx, direction, float(mip_level));

  /* Convert transmittance to transparency. */
  col.a = 1.0 - col.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(write_coord_packed, world_coord_packed));
  if (!is_world && col.a != 1.0) {
    vec2 world_uv = wrapped_uv * world_coord.scale + world_coord.offset;
    vec4 world_col = textureLod(atlas_tx, vec3(world_uv, world_coord.layer), 0.0);
    col.rgb = mix(world_col.rgb, col.rgb, col.a);
  }

  imageStore(atlas_img, ivec3(texel, write_coord.layer), col);
}
