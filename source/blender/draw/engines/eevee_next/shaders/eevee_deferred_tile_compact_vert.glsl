/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Convert the tile classification texture into streams of tiles of each types.
 * Dispatched with 1 vertex (thread) per tile.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)

void main()
{
  /* Doesn't matter. Doesn't get rasterized.
   * gl_PointSize set for Metal/Vulkan backend compatibility. */
  gl_Position = vec4(0.0);
  gl_PointSize = 0.0;

  int tile_per_row = textureSize(tile_mask_tx, 0).x;
  ivec2 tile_coord = ivec2(gl_VertexID % tile_per_row, gl_VertexID / tile_per_row);

  if (gl_VertexID == 0) {
    closure_double_draw_buf.instance_len = 1u;
    closure_single_draw_buf.instance_len = 1u;
    closure_triple_draw_buf.instance_len = 1u;
  }

  if (!in_texture_range(tile_coord, tile_mask_tx)) {
    return;
  }

  uint closure_count = texelFetch(tile_mask_tx, ivec3(tile_coord, 0), 0).r +
                       texelFetch(tile_mask_tx, ivec3(tile_coord, 1), 0).r +
                       texelFetch(tile_mask_tx, ivec3(tile_coord, 2), 0).r +
                       texelFetch(tile_mask_tx, ivec3(tile_coord, 3), 0).r;

  if (closure_count == 3) {
    uint tile_index = atomicAdd(closure_triple_draw_buf.vertex_len, 6u) / 6u;
    closure_triple_tile_buf[tile_index] = packUvec2x16(uvec2(tile_coord));
  }
  else if (closure_count == 2) {
    uint tile_index = atomicAdd(closure_double_draw_buf.vertex_len, 6u) / 6u;
    closure_double_tile_buf[tile_index] = packUvec2x16(uvec2(tile_coord));
  }
  else if (closure_count == 1) {
    uint tile_index = atomicAdd(closure_single_draw_buf.vertex_len, 6u) / 6u;
    closure_single_tile_buf[tile_index] = packUvec2x16(uvec2(tile_coord));
  }
}
