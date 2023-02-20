
/**
 * Virtual Shadow map output.
 *
 * Meshes are rasterize onto an empty framebuffer. Each generated fragment then checks which
 * virtual page it is supposed to go and load the physical page adress.
 * If a physical page exists, we then use atomicMin to mimic a less-than depth test and write to
 * the destination texel.
 **/

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_transparency_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

void write_depth(ivec2 texel_co, const int lod, ivec2 tile_co, float depth)
{
  ivec2 texel_co_lod = texel_co >> lod;

  ivec2 lod_corner_in_lod0 = texel_co_lod << lod;
  /* Add half of the lod to get the top right pixel nearest to the lod pixel.
   * This way we never get more than half a LOD0 pixel of offset from the center of any LOD.
   * This offset is taken into account during sampling. */
  const int lod_half_stride_in_lod0 = (1 << lod) / 2;
  ivec2 closest_lod0_texel = lod_corner_in_lod0 + lod_half_stride_in_lod0;

  if (!all(equal(closest_lod0_texel, texel_co))) {
    return;
  }

  ivec3 render_map_coord = ivec3(tile_co >> lod, shadow_interp.view_id);
  uint page_packed = texelFetch(shadow_render_map_tx, render_map_coord, lod).r;
  /* Return if no valid page. */
  if (page_packed == 0xFFFFFFFFu) {
    return;
  }
  ivec2 page = ivec2(unpackUvec2x16(page_packed));
  ivec2 texel_in_page = texel_co_lod % pages_infos_buf.page_size;
  ivec2 out_texel = page * pages_infos_buf.page_size + texel_in_page;

  uint u_depth = floatBitsToUint(depth);
  /* Quantization bias. Equivalent to nextafter in C without all the safety. 1 is not enough. */
  u_depth += 2;

  imageAtomicMin(shadow_atlas_img, out_texel, u_depth);
}

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

  drw_view_id = shadow_interp.view_id;

  ivec2 texel_co = ivec2(gl_FragCoord.xy);
  ivec2 tile_co = texel_co / pages_infos_buf.page_size;

  float depth = gl_FragCoord.z;
  float slope_bias = fwidth(depth);
  write_depth(texel_co, 0, tile_co, depth + slope_bias);

  /* Only needed for local lights. */
  bool is_persp = (drw_view.winmat[3][3] == 0.0);
  if (is_persp) {
    /* Note that even if texel center is offset, we store unmodified depth.
     * We increase bias instead at sampling time. */
#if SHADOW_TILEMAP_LOD != 5
#  error This needs to be adjusted
#endif
    write_depth(texel_co, 1, tile_co, depth + slope_bias * 2.0);
    write_depth(texel_co, 2, tile_co, depth + slope_bias * 4.0);
    write_depth(texel_co, 3, tile_co, depth + slope_bias * 8.0);
    write_depth(texel_co, 4, tile_co, depth + slope_bias * 16.0);
    write_depth(texel_co, 5, tile_co, depth + slope_bias * 32.0);
  }
}
