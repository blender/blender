/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shadow pipeline
 * \{ */

/* NOTE(Metal): As this is implemented using a fundamental data type, this needs to be specified
 * explicitly as uint for code generation, as the MSLShaderGenerator needs to be able to
 * distinguish between classes and fundamental types during code generation. */
#define SHADOW_TILE_DATA_PACKED "uint"
#define SHADOW_PAGE_PACKED "uint"

GPU_SHADER_CREATE_INFO(eevee_shadow_clipmap_clear)
    .do_static_compilation(true)
    .local_group_size(SHADOW_CLIPMAP_GROUP_SIZE)
    .storage_buf(0, Qualifier::WRITE, "ShadowTileMapClip", "tilemaps_clip_buf[]")
    .push_constant(Type::INT, "tilemaps_clip_buf_len")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_clipmap_clear_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tilemap_bounds)
    .do_static_compilation(true)
    .local_group_size(SHADOW_BOUNDS_GROUP_SIZE)
    .storage_buf(LIGHT_BUF_SLOT, Qualifier::READ_WRITE, "LightData", "light_buf[]")
    .storage_buf(LIGHT_CULL_BUF_SLOT, Qualifier::READ, "LightCullingData", "light_cull_buf")
    .storage_buf(4, Qualifier::READ, "uint", "casters_id_buf[]")
    .storage_buf(5, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(6, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .storage_buf(7, Qualifier::READ_WRITE, "ShadowTileMapClip", "tilemaps_clip_buf[]")
    .push_constant(Type::INT, "resource_len")
    .typedef_source("draw_shader_shared.hh")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_tilemap_bounds_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tilemap_init)
    .do_static_compilation(true)
    .local_group_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)
    .storage_buf(0, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ShadowTileMapClip", "tilemaps_clip_buf[]")
    .storage_buf(4, Qualifier::READ_WRITE, "uvec2", "pages_cached_buf[]")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_tilemap_init_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tag_update)
    .do_static_compilation(true)
    .local_group_size(1, 1, 1)
    .storage_buf(0, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(5, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .storage_buf(6, Qualifier::READ, "uint", "resource_ids_buf[]")
    .additional_info("eevee_shared", "draw_view", "draw_view_culling")
    .compute_source("eevee_shadow_tag_update_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tag_usage_opaque)
    .do_static_compilation(true)
    .local_group_size(SHADOW_DEPTH_SCAN_GROUP_SIZE, SHADOW_DEPTH_SCAN_GROUP_SIZE)
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .storage_buf(5, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(6, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .push_constant(Type::FLOAT, "tilemap_proj_ratio")
    .additional_info("eevee_shared", "draw_view", "draw_view_culling", "eevee_light_data")
    .compute_source("eevee_shadow_tag_usage_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tag_usage_surfels)
    .do_static_compilation(true)
    .local_group_size(SURFEL_GROUP_SIZE)
    .storage_buf(6, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    /* ShadowTileDataPacked is uint. But MSL translation need the real type. */
    .storage_buf(7, Qualifier::READ_WRITE, "uint", "tiles_buf[]")
    .push_constant(Type::INT, "directional_level")
    .push_constant(Type::FLOAT, "tilemap_proj_ratio")
    .additional_info("eevee_shared",
                     "draw_view",
                     "draw_view_culling",
                     "eevee_light_data",
                     "eevee_surfel_common")
    .compute_source("eevee_shadow_tag_usage_surfels_comp.glsl");

GPU_SHADER_INTERFACE_INFO(eevee_shadow_tag_transparent_iface, "interp")
    .smooth(Type::VEC3, "P")
    .smooth(Type::VEC3, "vP");
GPU_SHADER_INTERFACE_INFO(eevee_shadow_tag_transparent_flat_iface, "interp_flat")
    .flat(Type::VEC3, "ls_aabb_min")
    .flat(Type::VEC3, "ls_aabb_max");

GPU_SHADER_CREATE_INFO(eevee_shadow_tag_usage_transparent)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .storage_buf(4, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .storage_buf(5, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(6, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .push_constant(Type::FLOAT, "tilemap_proj_ratio")
    .push_constant(Type::FLOAT, "pixel_world_radius")
    .push_constant(Type::IVEC2, "fb_resolution")
    .push_constant(Type::INT, "fb_lod")
    .vertex_out(eevee_shadow_tag_transparent_iface)
    .vertex_out(eevee_shadow_tag_transparent_flat_iface)
    .additional_info("eevee_shared",
                     "draw_resource_id_varying",
                     "draw_view",
                     "draw_view_culling",
                     "draw_modelmat_new",
                     "eevee_hiz_data",
                     "eevee_light_data")
    .vertex_source("eevee_shadow_tag_usage_vert.glsl")
    .fragment_source("eevee_shadow_tag_usage_frag.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tag_usage_volume)
    .do_static_compilation(true)
    .local_group_size(VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE)
    .storage_buf(4, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(5, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .push_constant(Type::FLOAT, "tilemap_proj_ratio")
    .additional_info("eevee_volume_properties_data",
                     "eevee_shared",
                     "draw_view",
                     "draw_view_culling",
                     "eevee_hiz_data",
                     "eevee_light_data",
                     "eevee_sampling_data")
    .compute_source("eevee_shadow_tag_usage_volume_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_page_mask)
    .do_static_compilation(true)
    .local_group_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)
    .storage_buf(0, Qualifier::READ, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_page_mask_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_page_free)
    .do_static_compilation(true)
    .local_group_size(SHADOW_TILEMAP_LOD0_LEN)
    .storage_buf(0, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ShadowPagesInfoData", "pages_infos_buf")
    .storage_buf(3, Qualifier::READ_WRITE, "uint", "pages_free_buf[]")
    .storage_buf(4, Qualifier::READ_WRITE, "uvec2", "pages_cached_buf[]")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_page_free_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_page_defrag)
    .do_static_compilation(true)
    .local_group_size(1)
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ShadowPagesInfoData", "pages_infos_buf")
    .storage_buf(3, Qualifier::READ_WRITE, "uint", "pages_free_buf[]")
    .storage_buf(4, Qualifier::READ_WRITE, "uvec2", "pages_cached_buf[]")
    .storage_buf(5, Qualifier::WRITE, "DispatchCommand", "clear_dispatch_buf")
    .storage_buf(6, Qualifier::WRITE, "DrawCommand", "tile_draw_buf")
    .storage_buf(7, Qualifier::READ_WRITE, "ShadowStatistics", "statistics_buf")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_page_defrag_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_page_allocate)
    .do_static_compilation(true)
    .local_group_size(SHADOW_TILEMAP_LOD0_LEN)
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(0, Qualifier::READ_WRITE, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ShadowPagesInfoData", "pages_infos_buf")
    .storage_buf(3, Qualifier::READ_WRITE, "uint", "pages_free_buf[]")
    .storage_buf(4, Qualifier::READ_WRITE, "uvec2", "pages_cached_buf[]")
    .storage_buf(6, Qualifier::READ_WRITE, "ShadowStatistics", "statistics_buf")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_page_allocate_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_shadow_tilemap_finalize)
    .do_static_compilation(true)
    .typedef_source("draw_shader_shared.hh")
    .local_group_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)
    .storage_buf(0, Qualifier::READ, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ShadowPagesInfoData", "pages_infos_buf")
    .storage_buf(3, Qualifier::WRITE, "ViewMatrices", "view_infos_buf[SHADOW_VIEW_MAX]")
    .storage_buf(4, Qualifier::READ_WRITE, "ShadowStatistics", "statistics_buf")
    .storage_buf(5, Qualifier::READ_WRITE, "DispatchCommand", "clear_dispatch_buf")
    .storage_buf(6, Qualifier::READ_WRITE, "DrawCommand", "tile_draw_buf")
    .storage_buf(7, Qualifier::WRITE, SHADOW_PAGE_PACKED, "dst_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .storage_buf(8, Qualifier::WRITE, SHADOW_PAGE_PACKED, "src_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .storage_buf(9, Qualifier::WRITE, SHADOW_PAGE_PACKED, "render_map_buf[SHADOW_RENDER_MAP_SIZE]")
    .storage_buf(10, Qualifier::WRITE, "uint", "viewport_index_buf[SHADOW_VIEW_MAX]")
    .storage_buf(11, Qualifier::READ, "ShadowTileMapClip", "tilemaps_clip_buf[]")
    /* 12 is the minimum number of storage buf we require. Do not go above this limit. */
    .image(0, GPU_R32UI, Qualifier::WRITE, ImageType::UINT_2D, "tilemaps_img")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_tilemap_finalize_comp.glsl");

/* AtomicMin clear implementation. */
GPU_SHADER_CREATE_INFO(eevee_shadow_page_clear)
    .do_static_compilation(true)
    .local_group_size(SHADOW_PAGE_CLEAR_GROUP_SIZE, SHADOW_PAGE_CLEAR_GROUP_SIZE)
    .storage_buf(2, Qualifier::READ, "ShadowPagesInfoData", "pages_infos_buf")
    .storage_buf(6, Qualifier::READ, SHADOW_PAGE_PACKED, "dst_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .additional_info("eevee_shared")
    .compute_source("eevee_shadow_page_clear_comp.glsl")
    .image(SHADOW_ATLAS_IMG_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D_ARRAY_ATOMIC,
           "shadow_atlas_img");

/* TBDR clear implementation. */
GPU_SHADER_CREATE_INFO(eevee_shadow_page_tile_clear)
    .do_static_compilation(true)
    .define("PASS_CLEAR")
    .additional_info("eevee_shared")
    .builtins(BuiltinBits::VIEWPORT_INDEX | BuiltinBits::LAYER)
    .storage_buf(8, Qualifier::READ, SHADOW_PAGE_PACKED, "src_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .vertex_source("eevee_shadow_page_tile_vert.glsl")
    .fragment_source("eevee_shadow_page_tile_frag.glsl")
    .fragment_out(0, Type::FLOAT, "out_tile_depth", DualBlend::NONE, SHADOW_ROG_ID);

#ifdef APPLE
/* Metal supports USHORT which saves a bit of performance here. */
#  define PAGE_Z_TYPE Type::USHORT
#else
#  define PAGE_Z_TYPE Type::UINT
#endif

/* Interface for passing precalculated values in accumulation vertex to frag. */
GPU_SHADER_INTERFACE_INFO(eevee_shadow_page_tile_store_noperspective_iface, "interp_noperspective")
    .no_perspective(Type::VEC2, "out_texel_xy");
GPU_SHADER_INTERFACE_INFO(eevee_shadow_page_tile_store_flat_iface, "interp_flat")
    .flat(PAGE_Z_TYPE, "out_page_z");

#undef PAGE_Z_TYPE

/* 2nd tile pass to store shadow depths in atlas. */
GPU_SHADER_CREATE_INFO(eevee_shadow_page_tile_store)
    .do_static_compilation(true)
    .define("PASS_DEPTH_STORE")
    .additional_info("eevee_shared")
    .builtins(BuiltinBits::VIEWPORT_INDEX | BuiltinBits::LAYER)
    .storage_buf(7, Qualifier::READ, SHADOW_PAGE_PACKED, "dst_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .storage_buf(8, Qualifier::READ, SHADOW_PAGE_PACKED, "src_coord_buf[SHADOW_RENDER_MAP_SIZE]")
    .subpass_in(0, Type::FLOAT, "in_tile_depth", SHADOW_ROG_ID)
    .image(SHADOW_ATLAS_IMG_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D_ARRAY,
           "shadow_atlas_img")
    .vertex_out(eevee_shadow_page_tile_store_noperspective_iface)
    .vertex_out(eevee_shadow_page_tile_store_flat_iface)
    .vertex_source("eevee_shadow_page_tile_vert.glsl")
    .fragment_source("eevee_shadow_page_tile_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow resources
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_shadow_data)
    /* SHADOW_READ_ATOMIC macro indicating shadow functions should use `usampler2DArrayAtomic` as
     * the atlas type. */
    .define("SHADOW_READ_ATOMIC")
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    .sampler(SHADOW_ATLAS_TEX_SLOT, ImageType::UINT_2D_ARRAY_ATOMIC, "shadow_atlas_tx")
    .sampler(SHADOW_TILEMAPS_TEX_SLOT, ImageType::UINT_2D, "shadow_tilemaps_tx");

GPU_SHADER_CREATE_INFO(eevee_shadow_data_non_atomic)
    .sampler(SHADOW_ATLAS_TEX_SLOT, ImageType::UINT_2D_ARRAY, "shadow_atlas_tx")
    .sampler(SHADOW_TILEMAPS_TEX_SLOT, ImageType::UINT_2D, "shadow_tilemaps_tx");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_shadow_debug)
    .do_static_compilation(true)
    .additional_info("eevee_shared")
    .storage_buf(5, Qualifier::READ, "ShadowTileMapData", "tilemaps_buf[]")
    .storage_buf(6, Qualifier::READ, SHADOW_TILE_DATA_PACKED, "tiles_buf[]")
    .fragment_out(0, Type::VEC4, "out_color_add", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_color_mul", DualBlend::SRC_1)
    .push_constant(Type::INT, "debug_mode")
    .push_constant(Type::INT, "debug_tilemap_index")
    .depth_write(DepthWrite::ANY)
    .fragment_source("eevee_shadow_debug_frag.glsl")
    .additional_info(
        "draw_fullscreen", "draw_view", "eevee_hiz_data", "eevee_light_data", "eevee_shadow_data");

/** \} */
