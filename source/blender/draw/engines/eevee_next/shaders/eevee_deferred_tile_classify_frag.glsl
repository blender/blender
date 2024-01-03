/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass load Gbuffer data and output a mask of tiles to process.
 * This mask is then processed by the compaction phase.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  ivec2 tile_co = texel >> closure_tile_size_shift;

  GBufferReader gbuf = gbuffer_read_header(in_gbuffer_header);

  if (gbuf.has_diffuse) {
    imageStore(tile_mask_img, ivec3(tile_co, 0), uvec4(1u));
  }
  if (gbuf.has_reflection) {
    imageStore(tile_mask_img, ivec3(tile_co, 1), uvec4(1u));
  }
  if (gbuf.has_refraction) {
    imageStore(tile_mask_img, ivec3(tile_co, 2), uvec4(1u));
  }
  if (gbuf.has_translucent) {
    imageStore(tile_mask_img, ivec3(tile_co, 3), uvec4(1u));
  }
  /* TODO(fclem): For now, override SSS if we have translucency. */
  else if (gbuf.has_sss) {
    imageStore(tile_mask_img, ivec3(tile_co, 3), uvec4(1u));
  }
}
