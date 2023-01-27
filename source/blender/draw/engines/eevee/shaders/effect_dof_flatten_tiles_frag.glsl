
/**
 * Tile flatten pass: Takes the halfres CoC buffer and converts it to 8x8 tiles.
 *
 * Output min and max values for each tile and for both foreground & background.
 * Also outputs min intersectable CoC for the background, which is the minimum CoC
 * that comes from the background pixels.
 */

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

#define halfres_tile_divisor (DOF_TILE_DIVISOR / 2)

void main()
{
  ivec2 halfres_bounds = textureSize(halfResCocBuffer, 0).xy - 1;
  ivec2 tile_co = ivec2(gl_FragCoord.xy);

  CocTile tile = dof_coc_tile_init();

  for (int x = 0; x < halfres_tile_divisor; x++) {
    /* OPTI: Could be done in separate passes. */
    for (int y = 0; y < halfres_tile_divisor; y++) {
      ivec2 sample_texel = tile_co * halfres_tile_divisor + ivec2(x, y);
      vec2 sample_data = texelFetch(halfResCocBuffer, min(sample_texel, halfres_bounds), 0).rg;
      float sample_coc = sample_data.x;
      float sample_slight_focus_coc = sample_data.y;

      float fg_coc = min(sample_coc, 0.0);
      tile.fg_min_coc = min(tile.fg_min_coc, fg_coc);
      tile.fg_max_coc = max(tile.fg_max_coc, fg_coc);

      float bg_coc = max(sample_coc, 0.0);
      tile.bg_min_coc = min(tile.bg_min_coc, bg_coc);
      tile.bg_max_coc = max(tile.bg_max_coc, bg_coc);

      if (sample_coc > 0.0) {
        tile.bg_min_intersectable_coc = min(tile.bg_min_intersectable_coc, bg_coc);
      }
      if (sample_coc < 0.0) {
        tile.fg_max_intersectable_coc = max(tile.fg_max_intersectable_coc, fg_coc);
      }

      tile.fg_slight_focus_max_coc = dof_coc_max_slight_focus(tile.fg_slight_focus_max_coc,
                                                              sample_slight_focus_coc);
    }
  }

  dof_coc_tile_store(tile, outFgCoc, outBgCoc);
}
