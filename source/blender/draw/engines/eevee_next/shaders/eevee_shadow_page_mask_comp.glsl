
/**
 * Virtual shadowmapping: Usage un-tagging
 *
 * Remove used tag from masked tiles (LOD overlap).
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)

shared uint usage_grid[SHADOW_TILEMAP_RES / 2][SHADOW_TILEMAP_RES / 2];

void main()
{
  uint tilemap_index = gl_GlobalInvocationID.z;
  ShadowTileMapData tilemap = tilemaps_buf[tilemap_index];

  if (tilemap.projection_type == SHADOW_PROJECTION_CUBEFACE) {
    /* For each level collect the number of used (or masked) tile that are covering the tile from
     * the level underneath. If this adds up to 4 the underneath tile is flag unused as its data
     * is not needed for rendering.
     *
     * This is because 2 receivers can tag used the same area of the shadowmap but with different
     * LODs. */
    bool is_used = false;
    ivec2 tile_co = ivec2(gl_GlobalInvocationID.xy);
    uint lod_size = uint(SHADOW_TILEMAP_RES);
    for (int lod = 0; lod <= SHADOW_TILEMAP_LOD; lod++, lod_size >>= 1u) {
      bool thread_active = all(lessThan(tile_co, ivec2(lod_size)));

      barrier();

      ShadowTileData tile;
      if (thread_active) {
        int tile_offset = shadow_tile_offset(tile_co, tilemap.tiles_index, lod);
        tile = shadow_tile_unpack(tiles_buf[tile_offset]);
        if (lod > 0 && usage_grid[tile_co.y][tile_co.x] == 4u) {
          /* Remove the usage flag as this tile is completely covered by higher LOD tiles. */
          tiles_buf[tile_offset] &= ~SHADOW_IS_USED;
          /* Consider this tile occluding lower levels. */
          tile.is_used = true;
        }
        /* Reset count for next level. */
        usage_grid[tile_co.y][tile_co.x] = 0u;
      }

      barrier();

      if (thread_active) {
        if (tile.is_used) {
          atomicAdd(usage_grid[tile_co.y / 2][tile_co.x / 2], 1u);
        }
      }
    }
  }
}
