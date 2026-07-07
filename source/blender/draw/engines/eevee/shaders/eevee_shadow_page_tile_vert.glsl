/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual Shadow map tile shader.
 *
 * See fragment shader for more infos.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_shadow_page_tile_clear)

#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  int tile_id = gl_VertexID / 6;
  int vertex_id = gl_VertexID % 6;
  /* Generate Quad with 2 triangle with same winding.
   * This way the can be merged on some hardware. */
  int v = (vertex_id > 2) ? (3 - (vertex_id - 3)) : vertex_id;
  float2 tile_corner = float2(v & 1, v >> 1);

#ifdef PASS_DEPTH_STORE
  /* Load where fragment should write the tile data. */
  uint3 dst_page_co = shadow_page_unpack(dst_coord_buf[tile_id]);
  /* Interpolate output texel. */
  interp_noperspective.out_texel_xy = (float2(dst_page_co.xy) + tile_corner) *
                                      float2(SHADOW_PAGE_RES);
  interp_flat.out_page_z = dst_page_co.z;
#endif

  /* Load where the quad should be positioned. */
  uint3 src_page_co = unpackUvec4x8(src_coord_buf[tile_id]).xyz;

  float2 uv_pos = (tile_corner + float2(src_page_co.xy)) / float(SHADOW_TILEMAP_RES);
  float2 ndc_pos = uv_pos * 2.0f - 1.0f;
  /* We initially clear depth to 1.0 only for update fragments.
   * Non-updated tile depth will remain at 0.0 to ensure fragments are discarded. */
  gl_Position = float4(ndc_pos.x, ndc_pos.y, 1.0f, 1.0f);
  gpu_Layer = int(src_page_co.z);
  /* Assumes last viewport will always cover the whole frame-buffer. */
  gpu_ViewportIndex = 15;
}
