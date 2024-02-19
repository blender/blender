/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

SphereProbePixelArea reinterpret_as_write_coord(ivec4 packed_coord)
{
  SphereProbePixelArea unpacked;
  unpacked.offset = packed_coord.xy;
  unpacked.extent = packed_coord.z;
  unpacked.layer = packed_coord.w;
  return unpacked;
}

void main()
{
  SphereProbePixelArea write_coord = reinterpret_as_write_coord(write_coord_packed);

  /* Texel in probe. */
  ivec2 local_texel = ivec2(gl_GlobalInvocationID.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(local_texel, ivec2(write_coord.extent)))) {
    return;
  }

  ivec2 texel_out = write_coord.offset + local_texel;
  vec4 color = vec4(0.0);
  color += imageLoad(in_atlas_mip_img, ivec3(texel_out * 2 + ivec2(0, 0), write_coord.layer));
  color += imageLoad(in_atlas_mip_img, ivec3(texel_out * 2 + ivec2(1, 0), write_coord.layer));
  color += imageLoad(in_atlas_mip_img, ivec3(texel_out * 2 + ivec2(0, 1), write_coord.layer));
  color += imageLoad(in_atlas_mip_img, ivec3(texel_out * 2 + ivec2(1, 1), write_coord.layer));
  color *= 0.25;

  imageStore(out_atlas_mip_img, ivec3(texel_out, write_coord.layer), color);
}
