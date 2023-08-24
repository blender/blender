/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual Shadow map output.
 *
 * Meshes are rasterize onto an empty framebuffer. Each generated fragment then checks which
 * virtual page it is supposed to go and load the physical page address.
 * If a physical page exists, we then use atomicMin to mimic a less-than depth test and write to
 * the destination texel.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_transparency_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

void main()
{
#ifdef MAT_TRANSPARENT
  init_globals();

  nodetree_surface();

  float noise_offset = sampling_rng_1D_get(SAMPLING_TRANSPARENCY);
  float random_threshold = transparency_hashed_alpha_threshold(1.0, noise_offset, g_data.P);

  float transparency = avg(g_transmittance);
  if (transparency > random_threshold) {
    discard;
    return;
  }
#endif

#ifdef USE_ATOMIC
  ivec2 texel_co = ivec2(gl_FragCoord.xy);

  /* Using bitwise ops is way faster than integer ops. */
  const int page_shift = SHADOW_PAGE_LOD;
  const int page_mask = ~(0xFFFFFFFF << SHADOW_PAGE_LOD);

  ivec2 tile_co = texel_co >> page_shift;
  ivec2 texel_page = texel_co & page_mask;

  int view_index = shadow_view_id_get();

  int render_page_index = shadow_render_page_index_get(view_index, tile_co);
  uint page_packed = render_map_buf[render_page_index];

  ivec3 page = ivec3(shadow_page_unpack(page_packed));
  ivec3 out_texel = ivec3((page.xy << page_shift) | texel_page, page.z);
  uint u_depth = floatBitsToUint(gl_FragCoord.z + fwidth(gl_FragCoord.z));
  /* Quantization bias. Equivalent to `nextafter()` in C without all the safety. */
  u_depth += 2;
  imageAtomicMin(shadow_atlas_img, out_texel, u_depth);
#endif
}
