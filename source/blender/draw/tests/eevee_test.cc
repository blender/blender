/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"

#include "RNA_define.hh"

#include "GPU_batch.hh"
#include "draw_shader.hh"
#include "draw_testing.hh"
#include "engines/eevee_next/eevee_instance.hh"
#include "engines/eevee_next/eevee_precompute.hh"

namespace blender::draw {

using namespace blender::eevee;

/* Replace with template version that is not GPU only. */
using ShadowPageCacheBuf = draw::StorageArrayBuffer<uint2, SHADOW_MAX_PAGE, false>;
using ShadowTileDataBuf = draw::StorageArrayBuffer<ShadowTileDataPacked, SHADOW_MAX_TILE, false>;

static void test_eevee_shadow_shift_clear()
{
  GPU_render_begin();
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  ShadowTileMapClipBuf tilemaps_clip = {"tilemaps_clip"};
  ShadowPageCacheBuf pages_cached_data_ = {"pages_cached_data_"};

  int tiles_index = 1;
  int tile_lod0 = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 5;
  int tile_lod1 = tile_lod0 + square_i(SHADOW_TILEMAP_RES);

  {
    ShadowTileMapData tilemap = {};
    tilemap.tiles_index = tiles_index * SHADOW_TILEDATA_PER_TILEMAP;
    tilemap.grid_shift = int2(SHADOW_TILEMAP_RES);
    tilemap.projection_type = SHADOW_PROJECTION_CUBEFACE;

    tilemaps_data.append(tilemap);

    tilemaps_data.push_update();
  }
  {
    ShadowTileData tile = {};

    tile.page = uint3(1, 2, 0);
    tile.is_used = true;
    tile.do_update = true;
    tiles_data[tile_lod0] = shadow_tile_pack(tile);

    tile.page = uint3(3, 2, 4);
    tile.is_used = false;
    tile.do_update = false;
    tiles_data[tile_lod1] = shadow_tile_pack(tile);

    tiles_data.push_update();
  }

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_tilemap_init");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tilemaps_clip_buf", tilemaps_clip);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.bind_ssbo("pages_cached_buf", pages_cached_data_);
  pass.dispatch(int3(1, 1, tilemaps_data.size()));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

  Manager manager;
  manager.submit(pass);

  tilemaps_data.read();
  tiles_data.read();

  EXPECT_EQ(tilemaps_data[0].grid_offset, int2(0));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod0]).page, uint3(1, 2, 0));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod0]).is_used, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod0]).do_update, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod1]).page, uint3(3, 2, 4));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod1]).is_used, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_lod1]).do_update, true);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_shift_clear)

static void test_eevee_shadow_shift()
{
  GPU_render_begin();
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  StorageArrayBuffer<ShadowTileMapClip, SHADOW_MAX_TILEMAP> tilemaps_clip = {"tilemaps_clip"};
  ShadowPageCacheBuf pages_cached_data = {"pages_cached_data"};

  auto tile_co_to_page = [](int2 co) {
    int page = co.x + co.y * SHADOW_TILEMAP_RES;
    return uint3((page % SHADOW_PAGE_PER_ROW),
                 (page / SHADOW_PAGE_PER_ROW) % SHADOW_PAGE_PER_COL,
                 (page / SHADOW_PAGE_PER_LAYER));
  };

  {
    ShadowTileMapClip clip = {};
    clip.clip_near_stored = 0.0;
    clip.clip_far_stored = 1.0;
    clip.clip_near = 0x00000000; /* floatBitsToOrderedInt(0.0) */
    clip.clip_far = 0x3F800000;  /* floatBitsToOrderedInt(1.0) */

    tilemaps_clip[0] = clip;

    tilemaps_clip.push_update();
  }
  {
    ShadowTileMapData tilemap = {};
    tilemap.tiles_index = 0;
    tilemap.clip_data_index = 0;
    tilemap.grid_shift = int2(-1, 2);
    tilemap.projection_type = SHADOW_PROJECTION_CLIPMAP;

    tilemaps_data.append(tilemap);

    tilemaps_data.push_update();
  }
  {
    ShadowTileData tile = {};

    for (auto x : IndexRange(SHADOW_TILEMAP_RES)) {
      for (auto y : IndexRange(SHADOW_TILEMAP_RES)) {
        tile.is_allocated = true;
        tile.is_rendered = true;
        tile.do_update = true;
        tile.page = tile_co_to_page(int2(x, y));
        tiles_data[x + y * SHADOW_TILEMAP_RES] = shadow_tile_pack(tile);
      }
    }

    tiles_data.push_update();
  }

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_tilemap_init");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tilemaps_clip_buf", tilemaps_clip);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.bind_ssbo("pages_cached_buf", pages_cached_data);
  pass.dispatch(int3(1, 1, tilemaps_data.size()));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

  Manager manager;
  manager.submit(pass);

  tilemaps_data.read();
  tiles_data.read();

  EXPECT_EQ(tilemaps_data[0].grid_offset, int2(0));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0]).page,
            tile_co_to_page(int2(SHADOW_TILEMAP_RES - 1, 2)));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0]).do_update, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0]).is_rendered, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0]).is_allocated, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1]).page, tile_co_to_page(int2(0, 2)));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1]).do_update, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1]).is_rendered, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1]).is_allocated, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0 + SHADOW_TILEMAP_RES * 2]).page,
            tile_co_to_page(int2(SHADOW_TILEMAP_RES - 1, 4)));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0 + SHADOW_TILEMAP_RES * 2]).do_update, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0 + SHADOW_TILEMAP_RES * 2]).is_rendered, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[0 + SHADOW_TILEMAP_RES * 2]).is_allocated, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1 + SHADOW_TILEMAP_RES * 2]).page,
            tile_co_to_page(int2(0, 4)));
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1 + SHADOW_TILEMAP_RES * 2]).do_update, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1 + SHADOW_TILEMAP_RES * 2]).is_rendered, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[1 + SHADOW_TILEMAP_RES * 2]).is_allocated, true);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_shift)

static void test_eevee_shadow_tag_update()
{
  GPU_render_begin();
  using namespace blender::math;
  StorageVectorBuffer<uint, 128> past_casters_updated = {"PastCastersUpdated"};
  StorageVectorBuffer<uint, 128> curr_casters_updated = {"CurrCastersUpdated"};

  Manager manager;
  {
    /* Simulate 1 object moving and 1 object static with changing resource index. */
    float4x4 obmat = float4x4::identity();
    float4x4 obmat2 = from_loc_rot_scale<float4x4>(
        float3(1.0f), Quaternion::identity(), float3(0.5f));
    float3 half_extent = float3(0.24f, 0.249f, 0.001f);

    {
      manager.begin_sync();
      ResourceHandle hdl = manager.resource_handle(obmat, float3(0.5f, 0.5f, -1.0f), half_extent);
      manager.resource_handle(obmat2);
      manager.end_sync();
      past_casters_updated.append(hdl.resource_index());
      past_casters_updated.push_update();
    }
    {
      manager.begin_sync();
      manager.resource_handle(obmat2);
      ResourceHandle hdl = manager.resource_handle(obmat, float3(-1.0f, 0.5f, -1.0f), half_extent);
      manager.end_sync();
      curr_casters_updated.append(hdl.resource_index());
      curr_casters_updated.push_update();
    }
  }

  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  tiles_data.clear_to_zero();

  {
    ShadowTileMap tilemap(0 * SHADOW_TILEDATA_PER_TILEMAP);
    tilemap.sync_cubeface(float4x4::identity(), 0.01f, 1.0f, 0.01f, 0.0f, Z_NEG, 0.0f);
    tilemaps_data.append(tilemap);
  }
  {
    ShadowTileMap tilemap(1 * SHADOW_TILEDATA_PER_TILEMAP);
    tilemap.sync_orthographic(float4x4::identity(), int2(0), 1, 0.0f, SHADOW_PROJECTION_CLIPMAP);
    tilemaps_data.append(tilemap);
  }

  tilemaps_data.push_update();

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_tag_update");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.bind_ssbo("bounds_buf", &manager.bounds_buf.previous());
  pass.bind_ssbo("resource_ids_buf", past_casters_updated);
  pass.dispatch(int3(past_casters_updated.size(), 1, tilemaps_data.size()));
  pass.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
  pass.bind_ssbo("resource_ids_buf", curr_casters_updated);
  pass.dispatch(int3(curr_casters_updated.size(), 1, tilemaps_data.size()));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

  manager.submit(pass);

  tiles_data.read();

  /** The layout of these expected strings is Y down. */
  StringRefNull expected_lod0 =
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "xxxx----------------xxxxxxxx----"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------";
  StringRefNull expected_lod1 =
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "----------------"
      "xx--------xxxx--"
      "xx--------xxxx--"
      "xx--------xxxx--"
      "xx--------xxxx--"
      "----------------"
      "----------------";
  StringRefNull expected_lod2 =
      "--------"
      "--------"
      "--------"
      "--------"
      "--------"
      "x----xx-"
      "x----xx-"
      "--------";
  StringRefNull expected_lod3 =
      "----"
      "----"
      "x-xx"
      "x-xx";
  StringRefNull expected_lod4 =
      "--"
      "xx";
  StringRefNull expected_lod5 = "x";
  const uint lod0_len = SHADOW_TILEMAP_LOD0_LEN;
  const uint lod1_len = SHADOW_TILEMAP_LOD1_LEN;
  const uint lod2_len = SHADOW_TILEMAP_LOD2_LEN;
  const uint lod3_len = SHADOW_TILEMAP_LOD3_LEN;
  const uint lod4_len = SHADOW_TILEMAP_LOD4_LEN;
  const uint lod5_len = SHADOW_TILEMAP_LOD5_LEN;

  auto stringify_result = [&](uint start, uint len) -> std::string {
    std::string result = "";
    for (auto i : IndexRange(start, len)) {
      result += (shadow_tile_unpack(tiles_data[i]).do_update) ? "x" : "-";
    }
    return result;
  };

  EXPECT_EQ(stringify_result(0, lod0_len), expected_lod0);
  EXPECT_EQ(stringify_result(lod0_len, lod1_len), expected_lod1);
  EXPECT_EQ(stringify_result(lod0_len + lod1_len, lod2_len), expected_lod2);
  EXPECT_EQ(stringify_result(lod0_len + lod1_len + lod2_len, lod3_len), expected_lod3);
  EXPECT_EQ(stringify_result(lod0_len + lod1_len + lod2_len + lod3_len, lod4_len), expected_lod4);
  EXPECT_EQ(stringify_result(lod0_len + lod1_len + lod2_len + lod3_len + lod4_len, lod5_len),
            expected_lod5);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_tag_update)

static void test_eevee_shadow_free()
{
  GPU_render_begin();
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  ShadowPageHeapBuf pages_free_data = {"PagesFreeBuf"};
  ShadowPageCacheBuf pages_cached_data = {"PagesCachedBuf"};
  ShadowPagesInfoDataBuf pages_infos_data = {"PagesInfosBuf"};

  int tiles_index = 1;
  int tile_orphaned_cached = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 5;
  int tile_orphaned_allocated = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 6;
  int tile_used_cached = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 260;
  int tile_used_allocated = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 32;
  int tile_used_unallocated = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 64;
  int tile_unused_cached = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 9;
  int tile_unused_allocated = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 8;
  int page_free_count = SHADOW_MAX_PAGE - 6;

  for (uint i : IndexRange(2, page_free_count)) {
    uint3 page = uint3((i % SHADOW_PAGE_PER_ROW),
                       (i / SHADOW_PAGE_PER_ROW) % SHADOW_PAGE_PER_COL,
                       (i / SHADOW_PAGE_PER_LAYER));
    pages_free_data[i] = shadow_page_pack(page);
  }
  pages_free_data.push_update();

  pages_infos_data.page_free_count = page_free_count;
  pages_infos_data.page_alloc_count = 0;
  pages_infos_data.page_cached_next = 2u;
  pages_infos_data.page_cached_start = 0u;
  pages_infos_data.page_cached_end = 2u;
  pages_infos_data.push_update();

  for (uint i : IndexRange(pages_cached_data.size())) {
    pages_cached_data[i] = uint2(-1, -1);
  }
  pages_cached_data[0] = uint2(0, tile_orphaned_cached);
  pages_cached_data[1] = uint2(1, tile_used_cached);
  pages_cached_data.push_update();

  {
    ShadowTileData tile = {};

    tiles_data.clear_to_zero();
    tiles_data.read();

    /* is_orphaned = true */
    tile.is_used = false;
    tile.do_update = true;

    tile.is_cached = true;
    tile.is_allocated = false;
    tiles_data[tile_orphaned_cached] = shadow_tile_pack(tile);

    tile.is_cached = false;
    tile.is_allocated = true;
    tiles_data[tile_orphaned_allocated] = shadow_tile_pack(tile);

    /* is_orphaned = false */
    tile.do_update = false;
    tile.is_used = true;

    tile.is_cached = true;
    tile.is_allocated = false;
    tiles_data[tile_used_cached] = shadow_tile_pack(tile);

    tile.is_cached = false;
    tile.is_allocated = true;
    tiles_data[tile_used_allocated] = shadow_tile_pack(tile);

    tile.is_cached = false;
    tile.is_allocated = false;
    tiles_data[tile_used_unallocated] = shadow_tile_pack(tile);

    tile.is_used = false;
    tile.is_cached = true;
    tile.is_allocated = false;
    tiles_data[tile_unused_cached] = shadow_tile_pack(tile);

    tile.is_cached = false;
    tile.is_allocated = true;
    tiles_data[tile_unused_allocated] = shadow_tile_pack(tile);

    tiles_data.push_update();
  }
  {
    ShadowTileMapData tilemap = {};
    tilemap.tiles_index = tiles_index * SHADOW_TILEDATA_PER_TILEMAP;
    tilemaps_data.append(tilemap);
    tilemaps_data.push_update();
  }

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_page_free");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.bind_ssbo("pages_infos_buf", pages_infos_data);
  pass.bind_ssbo("pages_free_buf", pages_free_data);
  pass.bind_ssbo("pages_cached_buf", pages_cached_data);
  pass.dispatch(int3(1, 1, tilemaps_data.size()));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

  Manager manager;
  manager.submit(pass);

  tiles_data.read();
  pages_infos_data.read();

  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_orphaned_cached]).is_cached, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_orphaned_cached]).is_allocated, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_orphaned_allocated]).is_cached, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_orphaned_allocated]).is_allocated, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_cached]).is_cached, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_cached]).is_allocated, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_allocated]).is_cached, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_allocated]).is_allocated, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_unallocated]).is_cached, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_used_unallocated]).is_allocated, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_unused_cached]).is_cached, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_unused_cached]).is_allocated, false);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_unused_allocated]).is_cached, true);
  EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_unused_allocated]).is_allocated, false);
  EXPECT_EQ(pages_infos_data.page_alloc_count, 1);
  EXPECT_EQ(pages_infos_data.page_free_count, page_free_count + 2);
  EXPECT_EQ(pages_infos_data.page_cached_next, 3);
  EXPECT_EQ(pages_infos_data.page_cached_end, 2);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_free)

class TestDefrag {
 private:
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  ShadowPageHeapBuf pages_free_data = {"PagesFreeBuf"};
  ShadowPageCacheBuf pages_cached_data = {"PagesCachedBuf"};
  ShadowPagesInfoDataBuf pages_infos_data = {"PagesInfosBuf"};
  StorageBuffer<DispatchCommand> clear_dispatch_buf;
  StorageBuffer<DrawCommand> tile_draw_buf;
  ShadowStatisticsBuf statistics_buf = {"statistics_buf"};

 public:
  TestDefrag(int allocation_count,
             int descriptor_offset,
             StringRefNull descriptor,
             StringRefNull expect)
  {
    for (uint i : IndexRange(SHADOW_MAX_PAGE)) {
      uint2 page = {i % SHADOW_PAGE_PER_ROW, i / SHADOW_PAGE_PER_ROW};
      pages_free_data[i] = page.x | (page.y << 16u);
    }

    for (uint i : IndexRange(tiles_data.size())) {
      tiles_data[i] = 0;
    }

    int free_count = SHADOW_MAX_PAGE;
    int tile_index = 0;

    for (uint i : IndexRange(pages_cached_data.size())) {
      pages_cached_data[i] = uint2(-1, -1);
    }

    int cached_index = descriptor_offset;
    int hole_count = 0;
    int inserted_count = 0;
    ShadowTileData tile = {};
    tile.is_cached = true;
    for (char c : descriptor) {
      switch (c) {
        case 'c':
          tile.cache_index = cached_index++ % SHADOW_MAX_PAGE;
          pages_cached_data[tile.cache_index] = uint2(pages_free_data[--free_count], tile_index);
          tiles_data[tile_index++] = shadow_tile_pack(tile);
          break;
        case 'f':
          pages_cached_data[cached_index++ % SHADOW_MAX_PAGE] = uint2(-1, -1);
          hole_count++;
          break;
        case 'i':
          tile.cache_index = (cached_index + inserted_count++) % SHADOW_MAX_PAGE;
          pages_cached_data[tile.cache_index] = uint2(pages_free_data[--free_count], tile_index);
          tiles_data[tile_index++] = shadow_tile_pack(tile);
          break;
        default:
          break;
      }
    }

    pages_infos_data.page_alloc_count = allocation_count;
    pages_infos_data.page_cached_next = cached_index + inserted_count;
    pages_infos_data.page_free_count = free_count;
    pages_infos_data.page_cached_start = descriptor_offset;
    pages_infos_data.page_cached_end = cached_index;

    tiles_data.push_update();
    pages_infos_data.push_update();
    pages_free_data.push_update();
    pages_cached_data.push_update();

    GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_page_defrag");

    PassSimple pass("Test");
    pass.shader_set(sh);
    pass.bind_ssbo("tiles_buf", tiles_data);
    pass.bind_ssbo("pages_infos_buf", pages_infos_data);
    pass.bind_ssbo("pages_free_buf", pages_free_data);
    pass.bind_ssbo("pages_cached_buf", pages_cached_data);
    pass.bind_ssbo("clear_dispatch_buf", clear_dispatch_buf);
    pass.bind_ssbo("tile_draw_buf", tile_draw_buf);
    pass.bind_ssbo("statistics_buf", statistics_buf);
    pass.dispatch(int3(1, 1, 1));
    pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

    Manager manager;
    manager.submit(pass);

    tiles_data.read();
    pages_cached_data.read();
    pages_infos_data.read();

    std::string result = "";
    int expect_cached_len = 0;
    for (auto i : IndexRange(descriptor_offset, descriptor.size())) {
      if (pages_cached_data[i % SHADOW_MAX_PAGE].y != -1) {
        result += 'c';
        expect_cached_len++;
      }
      else {
        result += 'f';
      }
    }
    EXPECT_EQ(expect, result);

    allocation_count = min_ii(allocation_count, SHADOW_MAX_PAGE);

    int additional_pages = max_ii(0, allocation_count - free_count);
    int expected_free_count = max_ii(free_count, allocation_count);
    int expected_start = descriptor_offset + hole_count + additional_pages;
    int result_cached_len = pages_infos_data.page_cached_end - pages_infos_data.page_cached_start;

    if (expected_start > SHADOW_MAX_PAGE) {
      expected_start -= SHADOW_MAX_PAGE;
    }

    EXPECT_EQ(expected_free_count, pages_infos_data.page_free_count);
    EXPECT_EQ(expected_start, pages_infos_data.page_cached_start);
    EXPECT_EQ(expect_cached_len, result_cached_len);
    EXPECT_EQ(pages_infos_data.page_cached_end, pages_infos_data.page_cached_next);

    GPU_shader_free(sh);
    DRW_shaders_free();
  }
};

static void test_eevee_shadow_defrag()
{
  TestDefrag(0, 0, "cfi", "fcc");
  TestDefrag(0, 0, "fci", "fcc");
  TestDefrag(0, 47, "ccfcffccfcfciiiii", "fffffcccccccccccc");
  TestDefrag(10, SHADOW_MAX_PAGE - 5, "ccfcffccfcfciiiii", "fffffcccccccccccc");
  TestDefrag(SHADOW_MAX_PAGE - 8, 30, "ccfcffccfcfciiiii", "fffffffffcccccccc");
  TestDefrag(SHADOW_MAX_PAGE - 4, 30, "ccfcffccfcfciiiii", "fffffffffffffcccc");
  /* Over allocation but should not crash. */
  TestDefrag(SHADOW_MAX_PAGE + 4, 30, "ccfcffccfcfciiiii", "fffffffffffffffff");
}
DRAW_TEST(eevee_shadow_defrag)

class TestAlloc {
 private:
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  ShadowPageHeapBuf pages_free_data = {"PagesFreeBuf"};
  ShadowPageCacheBuf pages_cached_data = {"PagesCachedBuf"};
  ShadowPagesInfoDataBuf pages_infos_data = {"PagesInfosBuf"};
  ShadowStatisticsBuf statistics_buf = {"statistics_buf"};

 public:
  TestAlloc(int page_free_count)
  {
    GPU_render_begin();
    int tiles_index = 1;

    for (int i : IndexRange(SHADOW_MAX_TILE)) {
      tiles_data[i] = 0;
    }

    for (uint i : IndexRange(0, page_free_count)) {
      uint2 page = {i % SHADOW_PAGE_PER_ROW, i / SHADOW_PAGE_PER_ROW};
      pages_free_data[i] = page.x | (page.y << 16u);
    }
    pages_free_data.push_update();
    pages_cached_data.push_update();

    pages_infos_data.page_free_count = page_free_count;
    pages_infos_data.page_alloc_count = 1;
    pages_infos_data.page_cached_next = 0u;
    pages_infos_data.page_cached_start = 0u;
    pages_infos_data.page_cached_end = 0u;
    pages_infos_data.push_update();

    statistics_buf.view_needed_count = 0;
    statistics_buf.push_update();

    int tile_allocated = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 5;
    int tile_free = tiles_index * SHADOW_TILEDATA_PER_TILEMAP + 6;

    {
      ShadowTileData tile = {};

      tile.is_used = true;
      tile.do_update = false;

      tile.is_cached = false;
      tile.is_allocated = false;
      tiles_data[tile_free] = shadow_tile_pack(tile);

      tile.is_cached = false;
      tile.is_allocated = true;
      tiles_data[tile_allocated] = shadow_tile_pack(tile);

      tiles_data.push_update();
    }
    {
      ShadowTileMapData tilemap = {};
      tilemap.tiles_index = tiles_index * SHADOW_TILEDATA_PER_TILEMAP;
      tilemaps_data.append(tilemap);
      tilemaps_data.push_update();
    }

    GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_page_allocate");

    PassSimple pass("Test");
    pass.shader_set(sh);
    pass.bind_ssbo("tilemaps_buf", tilemaps_data);
    pass.bind_ssbo("tiles_buf", tiles_data);
    pass.bind_ssbo("pages_infos_buf", pages_infos_data);
    pass.bind_ssbo("pages_free_buf", pages_free_data);
    pass.bind_ssbo("pages_cached_buf", pages_cached_data);
    pass.bind_ssbo("statistics_buf", statistics_buf);
    pass.dispatch(int3(1, 1, tilemaps_data.size()));
    pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

    Manager manager;
    manager.submit(pass);

    tiles_data.read();
    pages_infos_data.read();

    bool alloc_success = page_free_count >= 1;

    EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_free]).do_update, alloc_success);
    EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_free]).is_allocated, alloc_success);
    EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_allocated]).do_update, false);
    EXPECT_EQ(shadow_tile_unpack(tiles_data[tile_allocated]).is_allocated, true);
    EXPECT_EQ(pages_infos_data.page_free_count, page_free_count - 1);

    GPU_shader_free(sh);
    DRW_shaders_free();
    GPU_render_end();
  }
};

static void test_eevee_shadow_alloc()
{
  TestAlloc(SHADOW_MAX_PAGE);
  TestAlloc(1);
  TestAlloc(0);
}
DRAW_TEST(eevee_shadow_alloc)

static void test_eevee_shadow_finalize()
{
  GPU_render_begin();
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  ShadowPageHeapBuf pages_free_data = {"PagesFreeBuf"};
  ShadowPageCacheBuf pages_cached_data = {"PagesCachedBuf"};
  ShadowPagesInfoDataBuf pages_infos_data = {"PagesInfosBuf"};
  ShadowStatisticsBuf statistics_buf = {"statistics_buf"};
  StorageArrayBuffer<ShadowTileMapClip, SHADOW_MAX_TILEMAP, false> tilemaps_clip = {
      "tilemaps_clip"};

  const uint lod0_len = SHADOW_TILEMAP_LOD0_LEN;
  const uint lod1_len = SHADOW_TILEMAP_LOD1_LEN;
  const uint lod2_len = SHADOW_TILEMAP_LOD2_LEN;
  const uint lod3_len = SHADOW_TILEMAP_LOD3_LEN;
  const uint lod4_len = SHADOW_TILEMAP_LOD4_LEN;

  const uint lod0_ofs = 0;
  const uint lod1_ofs = lod0_len;
  const uint lod2_ofs = lod1_ofs + lod1_len;
  const uint lod3_ofs = lod2_ofs + lod2_len;
  const uint lod4_ofs = lod3_ofs + lod3_len;
  const uint lod5_ofs = lod4_ofs + lod4_len;

  for (auto i : IndexRange(SHADOW_TILEDATA_PER_TILEMAP)) {
    tiles_data[i] = SHADOW_NO_DATA;
  }

  {
    ShadowTileData tile = {};
    tile.is_used = true;
    tile.is_allocated = true;

    tile.page = uint3(1, 0, 0);
    tile.do_update = false;
    tiles_data[lod0_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(2, 0, 0);
    tile.do_update = false;
    tiles_data[lod1_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(3, 0, 0);
    tile.do_update = true;
    tiles_data[lod2_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(0, 1, 0);
    tile.do_update = true;
    tiles_data[lod3_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(1, 1, 0);
    tile.do_update = true;
    tiles_data[lod4_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(2, 1, 0);
    tile.do_update = true;
    tiles_data[lod5_ofs] = shadow_tile_pack(tile);

    tile.page = uint3(3, 1, 0);
    tile.do_update = true;
    tiles_data[lod0_ofs + 31] = shadow_tile_pack(tile);

    tile.page = uint3(0, 2, 0);
    tile.do_update = true;
    tiles_data[lod3_ofs + 8] = shadow_tile_pack(tile);

    tile.page = uint3(1, 2, 0);
    tile.do_update = true;
    tiles_data[lod0_ofs + 32 * 16 - 8] = shadow_tile_pack(tile);

    tiles_data.push_update();
  }
  {
    ShadowTileMapData tilemap = {};
    tilemap.viewmat = float4x4::identity();
    tilemap.tiles_index = 0;
    tilemap.clip_data_index = 0;
    tilemap.clip_far = 10.0f;
    tilemap.clip_near = 1.0f;
    tilemap.half_size = 1.0f;
    tilemap.projection_type = SHADOW_PROJECTION_CUBEFACE;
    tilemaps_data.append(tilemap);

    tilemaps_data.push_update();
  }
  {
    ShadowTileMapClip clip = {};
    clip.clip_far_stored = 10.0f;
    clip.clip_near_stored = 1.0f;
    tilemaps_clip[0] = clip;
    tilemaps_clip.push_update();
  }
  {
    statistics_buf.view_needed_count = 0;
    statistics_buf.push_update();
  }
  {
    pages_infos_data.page_free_count = -5;
    pages_infos_data.page_alloc_count = 0;
    pages_infos_data.page_cached_next = 0u;
    pages_infos_data.page_cached_start = 0u;
    pages_infos_data.page_cached_end = 0u;
    pages_infos_data.push_update();
  }

  Texture tilemap_tx = {"tilemap_tx"};
  tilemap_tx.ensure_2d(GPU_R32UI,
                       int2(SHADOW_TILEMAP_RES),
                       GPU_TEXTURE_USAGE_HOST_READ | GPU_TEXTURE_USAGE_SHADER_READ |
                           GPU_TEXTURE_USAGE_SHADER_WRITE);
  tilemap_tx.clear(uint4(0));

  StorageArrayBuffer<ViewMatrices, DRW_VIEW_MAX> shadow_multi_view_buf = {"ShadowMultiView"};
  StorageBuffer<DispatchCommand> clear_dispatch_buf;
  StorageBuffer<DrawCommand> tile_draw_buf;
  StorageArrayBuffer<uint, SHADOW_MAX_PAGE> dst_coord_buf = {"dst_coord_buf"};
  StorageArrayBuffer<uint, SHADOW_MAX_PAGE> src_coord_buf = {"src_coord_buf"};
  StorageArrayBuffer<uint, SHADOW_RENDER_MAP_SIZE> render_map_buf = {"render_map_buf"};
  StorageArrayBuffer<uint, SHADOW_VIEW_MAX> viewport_index_buf = {"viewport_index_buf"};

  render_map_buf.clear_to_zero();

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_tilemap_finalize");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tilemaps_clip_buf", tilemaps_clip);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.bind_ssbo("view_infos_buf", shadow_multi_view_buf);
  pass.bind_ssbo("statistics_buf", statistics_buf);
  pass.bind_ssbo("clear_dispatch_buf", clear_dispatch_buf);
  pass.bind_ssbo("tile_draw_buf", tile_draw_buf);
  pass.bind_ssbo("dst_coord_buf", dst_coord_buf);
  pass.bind_ssbo("src_coord_buf", src_coord_buf);
  pass.bind_ssbo("render_map_buf", render_map_buf);
  pass.bind_ssbo("viewport_index_buf", viewport_index_buf);
  pass.bind_ssbo("pages_infos_buf", pages_infos_data);
  pass.bind_image("tilemaps_img", tilemap_tx);
  pass.dispatch(int3(1, 1, tilemaps_data.size()));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE | GPU_BARRIER_TEXTURE_UPDATE);

  Manager manager;
  manager.submit(pass);

  {
    /* Check output views. */
    shadow_multi_view_buf.read();

    for (auto i : IndexRange(5)) {
      EXPECT_EQ(shadow_multi_view_buf[i].viewmat, float4x4::identity());
      EXPECT_EQ(shadow_multi_view_buf[i].viewinv, float4x4::identity());
    }

    EXPECT_EQ(shadow_multi_view_buf[0].winmat,
              math::projection::perspective(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 10.0f));
    EXPECT_EQ(shadow_multi_view_buf[1].winmat,
              math::projection::perspective(-1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 10.0f));
    EXPECT_EQ(shadow_multi_view_buf[2].winmat,
              math::projection::perspective(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 10.0f));
    EXPECT_EQ(shadow_multi_view_buf[3].winmat,
              math::projection::perspective(-1.0f, -0.75f, -1.0f, -0.75f, 1.0f, 10.0f));
    EXPECT_EQ(shadow_multi_view_buf[4].winmat,
              math::projection::perspective(0.5f, 1.5f, -1.0f, 0.0f, 1.0f, 10.0f));
  }

  {
    uint *pixels = tilemap_tx.read<uint32_t>(GPU_DATA_UINT);

    std::string result = "";
    for (auto y : IndexRange(SHADOW_TILEMAP_RES)) {
      for (auto x : IndexRange(SHADOW_TILEMAP_RES)) {
        ShadowTileData tile = shadow_tile_unpack(pixels[y * SHADOW_TILEMAP_RES + x]);
        result += std::to_string(tile.page.x + tile.page.y * SHADOW_PAGE_PER_ROW);
      }
    }

    MEM_SAFE_FREE(pixels);

    /** The layout of these expected strings is Y down. */
    StringRefNull expected_pages =
        "12334444555555556666666666666667"
        "22334444555555556666666666666666"
        "33334444555555556666666666666666"
        "33334444555555556666666666666666"
        "44444444555555556666666666666666"
        "44444444555555556666666666666666"
        "44444444555555556666666666666666"
        "44444444555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666666666666"
        "55555555555555556666666696666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "88888888666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666"
        "66666666666666666666666666666666";

    EXPECT_EQ(expected_pages, result);
  }

  {
    auto stringify_view = [](Span<uint> data) -> std::string {
      std::string result = "";
      for (auto x : data) {
        result += (x == 0u) ? '-' : ((x == 0xFFFFFFFFu) ? 'x' : '0' + (x % 10));
      }
      return result;
    };

    /** The layout of these expected strings is Y down. */
    StringRefNull expected_view0 =
        "6-------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------";

    StringRefNull expected_view1 =
        "5-------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------";

    StringRefNull expected_view2 =
        "4xxx----------------------------"
        "xxxx----------------------------"
        "8xxx----------------------------"
        "xxxx----------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------";

    StringRefNull expected_view3 =
        "3-------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------";

    StringRefNull expected_view4 =
        "xxxxxxx7xxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "xxxxxxxxxxxxxxxx----------------"
        "9xxxxxxxxxxxxxxx----------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------"
        "--------------------------------";

    render_map_buf.read();

    EXPECT_EQ(stringify_view(Span<uint>(&render_map_buf[SHADOW_TILEMAP_LOD0_LEN * 0],
                                        SHADOW_TILEMAP_LOD0_LEN)),
              expected_view0);
    EXPECT_EQ(stringify_view(Span<uint>(&render_map_buf[SHADOW_TILEMAP_LOD0_LEN * 1],
                                        SHADOW_TILEMAP_LOD0_LEN)),
              expected_view1);
    EXPECT_EQ(stringify_view(Span<uint>(&render_map_buf[SHADOW_TILEMAP_LOD0_LEN * 2],
                                        SHADOW_TILEMAP_LOD0_LEN)),
              expected_view2);
    EXPECT_EQ(stringify_view(Span<uint>(&render_map_buf[SHADOW_TILEMAP_LOD0_LEN * 3],
                                        SHADOW_TILEMAP_LOD0_LEN)),
              expected_view3);
    EXPECT_EQ(stringify_view(Span<uint>(&render_map_buf[SHADOW_TILEMAP_LOD0_LEN * 4],
                                        SHADOW_TILEMAP_LOD0_LEN)),
              expected_view4);
  }

  pages_infos_data.read();
  EXPECT_EQ(pages_infos_data.page_free_count, 0);

  statistics_buf.read();
  EXPECT_EQ(statistics_buf.view_needed_count, 5);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_finalize)

static void test_eevee_shadow_page_mask()
{
  GPU_render_begin();
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  ShadowTileDataBuf tiles_data = {"tiles_data"};

  {
    ShadowTileMap tilemap(0);
    tilemap.sync_cubeface(float4x4::identity(), 0.01f, 1.0f, 0.01f, 0.0f, Z_NEG, 0.0f);
    tilemaps_data.append(tilemap);
  }

  const uint lod0_len = SHADOW_TILEMAP_LOD0_LEN;
  const uint lod1_len = SHADOW_TILEMAP_LOD1_LEN;
  const uint lod2_len = SHADOW_TILEMAP_LOD2_LEN;
  const uint lod3_len = SHADOW_TILEMAP_LOD3_LEN;
  const uint lod4_len = SHADOW_TILEMAP_LOD4_LEN;
  const uint lod5_len = SHADOW_TILEMAP_LOD5_LEN;

  const uint lod0_ofs = 0;
  const uint lod1_ofs = lod0_ofs + lod0_len;
  const uint lod2_ofs = lod1_ofs + lod1_len;
  const uint lod3_ofs = lod2_ofs + lod2_len;
  const uint lod4_ofs = lod3_ofs + lod3_len;
  const uint lod5_ofs = lod4_ofs + lod4_len;

  {
    ShadowTileData tile = {};
    /* Init all LOD to true. */
    for (auto i : IndexRange(SHADOW_TILEDATA_PER_TILEMAP)) {
      tile.is_used = true;
      tiles_data[i] = shadow_tile_pack(tile);
    }

    /* Init all of LOD0 to false. */
    for (auto i : IndexRange(square_i(SHADOW_TILEMAP_RES))) {
      tile.is_used = false;
      tiles_data[i] = shadow_tile_pack(tile);
    }

    /* Bottom Left of the LOD0 to true. */
    for (auto y : IndexRange((SHADOW_TILEMAP_RES / 2) + 1)) {
      for (auto x : IndexRange((SHADOW_TILEMAP_RES / 2) + 1)) {
        tile.is_used = true;
        tiles_data[x + y * SHADOW_TILEMAP_RES] = shadow_tile_pack(tile);
      }
    }

    /* All Bottom of the LOD0 to true. */
    for (auto x : IndexRange(SHADOW_TILEMAP_RES)) {
      tile.is_used = true;
      tiles_data[x] = shadow_tile_pack(tile);
    }

    /* Bottom Left of the LOD1 to false. */
    /* Should still cover bottom LODs since it is itself fully masked */
    for (auto y : IndexRange((SHADOW_TILEMAP_RES / 8))) {
      for (auto x : IndexRange((SHADOW_TILEMAP_RES / 8))) {
        tile.is_used = false;
        tiles_data[x + y * (SHADOW_TILEMAP_RES / 2) + lod0_len] = shadow_tile_pack(tile);
      }
    }

    /* Top right Center of the LOD1 to false. */
    /* Should un-cover 1 LOD2 tile. */
    {
      int x = SHADOW_TILEMAP_RES / 4;
      int y = SHADOW_TILEMAP_RES / 4;
      tile.is_used = false;
      tiles_data[x + y * (SHADOW_TILEMAP_RES / 2) + lod0_len] = shadow_tile_pack(tile);
    }

    tiles_data.push_update();
  }

  tilemaps_data.push_update();

  GPUShader *sh = GPU_shader_create_from_info_name("eevee_shadow_page_mask");

  PassSimple pass("Test");
  pass.shader_set(sh);
  pass.bind_ssbo("tilemaps_buf", tilemaps_data);
  pass.bind_ssbo("tiles_buf", tiles_data);
  pass.dispatch(int3(1, 1, tilemaps_data.size()));

  Manager manager;
  manager.submit(pass);
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);

  tiles_data.read();

  /** The layout of these expected strings is Y down. */
  StringRefNull expected_lod0 =
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "xxxxxxxxxxxxxxxxx---------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------"
      "--------------------------------";
  StringRefNull expected_lod1 =
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "--------xxxxxxxx"
      "xxxxxxxx-xxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx"
      "xxxxxxxxxxxxxxxx";
  StringRefNull expected_lod2 =
      "--------"
      "--------"
      "--------"
      "--------"
      "----x---"
      "--------"
      "--------"
      "--------";
  StringRefNull expected_lod3 =
      "----"
      "----"
      "----"
      "----";
  StringRefNull expected_lod4 =
      "--"
      "--";
  StringRefNull expected_lod5 = "-";

  auto stringify_result = [&](uint start, uint len) -> std::string {
    std::string result = "";
    for (auto i : IndexRange(start, len)) {
      result += (shadow_tile_unpack(tiles_data[i]).is_used) ? "x" : "-";
    }
    return result;
  };

  EXPECT_EQ(stringify_result(lod0_ofs, lod0_len), expected_lod0);
  EXPECT_EQ(stringify_result(lod1_ofs, lod1_len), expected_lod1);
  EXPECT_EQ(stringify_result(lod2_ofs, lod2_len), expected_lod2);
  EXPECT_EQ(stringify_result(lod3_ofs, lod3_len), expected_lod3);
  EXPECT_EQ(stringify_result(lod4_ofs, lod4_len), expected_lod4);
  EXPECT_EQ(stringify_result(lod5_ofs, lod5_len), expected_lod5);

  GPU_shader_free(sh);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_shadow_page_mask)

static void test_eevee_surfel_list()
{
  GPU_render_begin();
  StorageArrayBuffer<int> list_start_buf = {"list_start_buf"};
  StorageVectorBuffer<Surfel> surfel_buf = {"surfel_buf"};
  CaptureInfoBuf capture_info_buf = {"capture_info_buf"};
  SurfelListInfoBuf list_info_buf = {"list_info_buf"};

  /**
   * Simulate surfels on a 2x2 projection grid covering [0..2] on the Z axis.
   */
  {
    Surfel surfel;
    /* NOTE: Expected link assumes linear increasing processing order [0->5]. But this is
     * multithreaded and we can't know the execution order in advance. */
    /* 0: Project to (1, 0) = list 1. Unsorted Next = -1; Next = -1; Previous = 3. */
    surfel.position = {1.1f, 0.1f, 0.1f};
    surfel_buf.append(surfel);
    /* 1: Project to (1, 0) = list 1. Unsorted Next = 0; Next = 2; Previous = -1. */
    surfel.position = {1.1f, 0.2f, 0.5f};
    surfel_buf.append(surfel);
    /* 2: Project to (1, 0) = list 1. Unsorted Next = 1; Next = 3; Previous = 1. */
    surfel.position = {1.1f, 0.3f, 0.3f};
    surfel_buf.append(surfel);
    /* 3: Project to (1, 0) = list 1. Unsorted Next = 2; Next = 0; Previous = 2. */
    surfel.position = {1.2f, 0.4f, 0.2f};
    surfel_buf.append(surfel);
    /* 4: Project to (1, 1) = list 3. Unsorted Next = -1; Next = -1; Previous = -1. */
    surfel.position = {1.0f, 1.0f, 0.5f};
    surfel_buf.append(surfel);
    /* 5: Project to (0, 1) = list 2. Unsorted Next = -1; Next = -1; Previous = -1. */
    surfel.position = {0.1f, 1.1f, 0.5f};
    surfel_buf.append(surfel);

    surfel_buf.push_update();
  }
  {
    capture_info_buf.surfel_len = surfel_buf.size();
    capture_info_buf.push_update();
  }
  {
    list_info_buf.ray_grid_size = int2(2);
    list_info_buf.list_max = list_info_buf.ray_grid_size.x * list_info_buf.ray_grid_size.y;
    list_info_buf.push_update();
  }
  {
    list_start_buf.resize(ceil_to_multiple_u(list_info_buf.list_max, 4u));
    list_start_buf.push_update();
    GPU_storagebuf_clear(list_start_buf, -1);
  }

  /* Top-down view. */
  View view = {"RayProjectionView"};
  view.sync(float4x4::identity(), math::projection::orthographic<float>(0, 2, 0, 2, 0, 1));

  GPUShader *sh_build = GPU_shader_create_from_info_name("eevee_surfel_list_build");
  GPUShader *sh_sort = GPU_shader_create_from_info_name("eevee_surfel_list_sort");

  PassSimple pass("Build_and_Sort");
  pass.shader_set(sh_build);
  pass.bind_ssbo("list_start_buf", list_start_buf);
  pass.bind_ssbo("surfel_buf", surfel_buf);
  pass.bind_ssbo("capture_info_buf", capture_info_buf);
  pass.bind_ssbo("list_info_buf", list_info_buf);
  pass.dispatch(int3(1, 1, 1));
  pass.barrier(GPU_BARRIER_SHADER_STORAGE);

  pass.shader_set(sh_sort);
  pass.bind_ssbo("list_start_buf", list_start_buf);
  pass.bind_ssbo("surfel_buf", surfel_buf);
  pass.bind_ssbo("list_info_buf", list_info_buf);
  pass.dispatch(int3(1, 1, 1));
  pass.barrier(GPU_BARRIER_BUFFER_UPDATE);

  Manager manager;
  manager.submit(pass, view);

  list_start_buf.read();
  surfel_buf.read();

  /* Expect surfel list. */
  Vector<int> expect_link_next = {-1, +2, +3, +0, -1, -1};
  Vector<int> expect_link_prev = {+3, -1, +1, +2, -1, -1};

  Vector<int> link_next, link_prev;
  for (auto &surfel : Span<Surfel>(surfel_buf.data(), surfel_buf.size())) {
    link_next.append(surfel.next);
    link_prev.append(surfel.prev);
  }

#if 0 /* Useful for debugging */
  /* NOTE: All of these are unstable by definition (atomic + multi-thread).
   * But should be consistent since we only dispatch one thread-group. */
  /* Expect last added surfel index. It is the list start index before sorting. */
  Vector<int> expect_list_start = {-1, 1, 5, 4};
  // Span<int>(list_start_buf.data(), expect_list_start.size()).print_as_lines("list_start");
  // link_next.as_span().print_as_lines("link_next");
  // link_prev.as_span().print_as_lines("link_prev");
  EXPECT_EQ_ARRAY(expect_list_start.data(), list_start_buf.data(), expect_list_start.size());
#endif
  EXPECT_EQ_ARRAY(expect_link_next.data(), link_next.data(), expect_link_next.size());
  EXPECT_EQ_ARRAY(expect_link_prev.data(), link_prev.data(), expect_link_prev.size());

  GPU_shader_free(sh_build);
  GPU_shader_free(sh_sort);
  DRW_shaders_free();
  GPU_render_end();
}
DRAW_TEST(eevee_surfel_list)

static void test_eevee_lut_gen()
{
  GPU_render_begin();

  Manager manager;

  /* Check if LUT generation matches the header version. */
  auto brdf_ggx_gen = Precompute(manager, LUT_GGX_BRDF_SPLIT_SUM, {64, 64, 1}).data<float3>();
  auto btdf_ggx_gen = Precompute(manager, LUT_GGX_BTDF_IOR_GT_ONE, {64, 64, 16}).data<float1>();
  auto bsdf_ggx_gen = Precompute(manager, LUT_GGX_BSDF_SPLIT_SUM, {64, 64, 16}).data<float3>();
  auto burley_gen = Precompute(manager, LUT_BURLEY_SSS_PROFILE, {64, 1, 1}).data<float1>();
  auto rand_walk_gen = Precompute(manager, LUT_RANDOM_WALK_SSS_PROFILE, {64, 1, 1}).data<float1>();

  Span<float3> brdf_ggx_lut((const float3 *)&eevee::lut::brdf_ggx, 64 * 64);
  Span<float1> btdf_ggx_lut((const float1 *)&eevee::lut::btdf_ggx, 64 * 64 * 16);
  Span<float3> bsdf_ggx_lut((const float3 *)&eevee::lut::bsdf_ggx, 64 * 64 * 16);
  Span<float1> burley_sss_lut((const float1 *)&eevee::lut::burley_sss_profile, 64);
  Span<float1> rand_walk_lut((const float1 *)&eevee::lut::random_walk_sss_profile, 64);

  const float eps = 3e-3f;
  EXPECT_NEAR_ARRAY_ND(brdf_ggx_lut.data(), brdf_ggx_gen.data(), brdf_ggx_gen.size(), 3, eps);
  EXPECT_NEAR_ARRAY_ND(btdf_ggx_lut.data(), btdf_ggx_gen.data(), btdf_ggx_gen.size(), 1, eps);
  EXPECT_NEAR_ARRAY_ND(bsdf_ggx_lut.data(), bsdf_ggx_gen.data(), bsdf_ggx_gen.size(), 3, eps);
  EXPECT_NEAR_ARRAY_ND(burley_gen.data(), burley_sss_lut.data(), burley_sss_lut.size(), 1, eps);
  EXPECT_NEAR_ARRAY_ND(rand_walk_gen.data(), rand_walk_lut.data(), rand_walk_lut.size(), 1, eps);

  GPU_render_end();
}
DRAW_TEST(eevee_lut_gen)

}  // namespace blender::draw
