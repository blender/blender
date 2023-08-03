/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_lightprobe_types.h"

#include "BKE_lightprobe.h"

#include "GPU_capabilities.h"
#include "GPU_debug.h"

#include "BLI_math_rotation.hh"

#include "eevee_instance.hh"

#include "eevee_irradiance_cache.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Interface
 * \{ */

void IrradianceCache::init()
{
  display_grids_enabled_ = DRW_state_draw_support() &&
                           (inst_.scene->eevee.flag & SCE_EEVEE_SHOW_IRRADIANCE);

  /* TODO option. */
  int atlas_byte_size = 1024 * 1024 * 16;
  /* This might become an option in the future. */
  bool use_l2_band = false;
  int sh_coef_len = use_l2_band ? 9 : 4;
  int texel_byte_size = 8; /* Assumes GPU_RGBA16F. */
  int3 atlas_extent(IRRADIANCE_GRID_BRICK_SIZE);
  atlas_extent.z *= sh_coef_len;
  int atlas_col_count = 256;
  atlas_extent.x *= atlas_col_count;
  /* Determine the row count depending on the scene settings. */
  int row_byte_size = atlas_extent.x * atlas_extent.y * atlas_extent.z * texel_byte_size;
  int atlas_row_count = divide_ceil_u(atlas_byte_size, row_byte_size);
  atlas_extent.y *= atlas_row_count;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ |
                           GPU_TEXTURE_USAGE_ATTACHMENT;
  do_full_update_ = irradiance_atlas_tx_.ensure_3d(GPU_RGBA16F, atlas_extent, usage);

  if (do_full_update_) {
    /* Delete all references to existing bricks. */
    for (IrradianceGrid &grid : inst_.light_probes.grid_map_.values()) {
      grid.bricks.clear();
    }
    brick_pool_.clear();
    /* Fill with all the available bricks. */
    for (auto i : IndexRange(atlas_row_count * atlas_col_count)) {
      if (i == 0) {
        /* Reserve one brick for the world. */
        world_brick_index_ = 0;
      }
      else {
        IrradianceBrick brick;
        brick.atlas_coord = uint2(i % atlas_col_count, i / atlas_col_count) *
                            IRRADIANCE_GRID_BRICK_SIZE;
        brick_pool_.append(irradiance_brick_pack(brick));
      }
    }

    if (irradiance_atlas_tx_.is_valid()) {
      /* Clear the pool to avoid any interpolation to undefined values. */
      irradiance_atlas_tx_.clear(float4(0.0f));
    }

    inst_.reflection_probes.do_world_update_irradiance_set(true);
  }

  if (irradiance_atlas_tx_.is_valid() == false) {
    inst_.info = "Irradiance Atlas texture could not be created";
  }
}

void IrradianceCache::sync()
{
  if (inst_.is_baking()) {
    bake.sync();
  }
}

Vector<IrradianceBrickPacked> IrradianceCache::bricks_alloc(int brick_len)
{
  if (brick_pool_.size() < brick_len) {
    /* Fail allocation. Not enough brick in the atlas. */
    return {};
  }
  Vector<IrradianceBrickPacked> allocated(brick_len);
  /* Copy bricks to return vector. */
  allocated.as_mutable_span().copy_from(brick_pool_.as_span().take_back(brick_len));
  /* Remove bricks from the pool. */
  brick_pool_.resize(brick_pool_.size() - brick_len);

  return allocated;
}

void IrradianceCache::bricks_free(Vector<IrradianceBrickPacked> &bricks)
{
  brick_pool_.extend(bricks.as_span());
  bricks.clear();
}

void IrradianceCache::set_view(View & /*view*/)
{
  Vector<IrradianceGrid *> grid_updates;
  Vector<IrradianceGrid *> grid_loaded;

  /* First allocate the needed bricks and populate the brick buffer. */
  bricks_infos_buf_.clear();
  for (IrradianceGrid &grid : inst_.light_probes.grid_map_.values()) {
    LightProbeGridCacheFrame *cache = grid.cache ? grid.cache->grid_static_cache : nullptr;
    if (cache == nullptr) {
      continue;
    }

    if (cache->baking.L0 == nullptr && cache->irradiance.L0 == nullptr) {
      /* No data. */
      continue;
    }

    int3 grid_size = int3(cache->size);
    if (grid_size.x <= 0 || grid_size.y <= 0 || grid_size.z <= 0) {
      inst_.info = "Error: Malformed irradiance grid data";
      continue;
    }

    /* TODO frustum cull and only load visible grids. */

    /* Note that we reserve 1 slot for the world irradiance. */
    if (grid_loaded.size() >= IRRADIANCE_GRID_MAX - 1) {
      inst_.info = "Error: Too many grid visible";
      continue;
    }

    if (grid.bricks.is_empty()) {
      int3 grid_size_in_bricks = math::divide_ceil(grid_size,
                                                   int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
      int brick_len = grid_size_in_bricks.x * grid_size_in_bricks.y * grid_size_in_bricks.z;
      grid.bricks = bricks_alloc(brick_len);

      if (grid.bricks.is_empty()) {
        inst_.info = "Error: Irradiance grid allocation failed";
        continue;
      }

      grid_updates.append(&grid);
    }

    grid.brick_offset = bricks_infos_buf_.size();
    bricks_infos_buf_.extend(grid.bricks);

    if (grid_size.x <= 0 || grid_size.y <= 0 || grid_size.z <= 0) {
      inst_.info = "Error: Malformed irradiance grid data";
      continue;
    }

    float4x4 grid_to_world = grid.object_to_world * math::from_location<float4x4>(float3(-1.0f)) *
                             math::from_scale<float4x4>(float3(2.0f / float3(grid_size))) *
                             math::from_location<float4x4>(float3(0.0f));

    grid.world_to_grid_transposed = float3x4(math::transpose(math::invert(grid_to_world)));
    grid.grid_size = grid_size;
    grid_loaded.append(&grid);
  }

  /* Then create brick & grid infos UBOs content. */
  {
    /* Stable sorting of grids. */
    std::sort(grid_loaded.begin(),
              grid_loaded.end(),
              [](const IrradianceGrid *a, const IrradianceGrid *b) {
                float volume_a = math::determinant(float3x3(a->world_to_grid_transposed));
                float volume_b = math::determinant(float3x3(b->world_to_grid_transposed));
                if (volume_a != volume_b) {
                  /* Smallest first. */
                  return volume_a > volume_b;
                }
                /* Volumes are identical. Any arbitrary criteria can be used to sort them.
                 * Use position to avoid unstable result caused by depsgraph non deterministic eval
                 * order. This could also become a priority parameter. */
                return a->world_to_grid_transposed[0][0] < b->world_to_grid_transposed[0][0] ||
                       a->world_to_grid_transposed[0][1] < b->world_to_grid_transposed[0][1] ||
                       a->world_to_grid_transposed[0][2] < b->world_to_grid_transposed[0][2];
              });

    /* Insert grids in UBO in sorted order. */
    int grids_len = 0;
    for (IrradianceGrid *grid : grid_loaded) {
      grid->grid_index = grids_len;
      grids_infos_buf_[grids_len++] = *grid;
    }

    /* Insert world grid last. */
    IrradianceGridData grid;
    grid.world_to_grid_transposed = float3x4::identity();
    grid.grid_size = int3(1);
    grid.brick_offset = bricks_infos_buf_.size();
    grid.normal_bias = 0.0f;
    grid.view_bias = 0.0f;
    grid.facing_bias = 0.0f;
    grids_infos_buf_[grids_len++] = grid;
    bricks_infos_buf_.append(world_brick_index_);

    if (grids_len < IRRADIANCE_GRID_MAX) {
      /* Tag last grid as invalid to stop the iteration. */
      grids_infos_buf_[grids_len].grid_size = int3(-1);
    }

    bricks_infos_buf_.push_update();
    grids_infos_buf_.push_update();
  }

  /* Upload data for each grid that need to be inserted in the atlas. */
  for (IrradianceGrid *grid : grid_updates) {
    LightProbeGridCacheFrame *cache = grid->cache->grid_static_cache;

    /* Staging textures are recreated for each light grid to avoid increasing VRAM usage. */
    draw::Texture irradiance_a_tx = {"irradiance_a_tx"};
    draw::Texture irradiance_b_tx = {"irradiance_b_tx"};
    draw::Texture irradiance_c_tx = {"irradiance_c_tx"};
    draw::Texture irradiance_d_tx = {"irradiance_d_tx"};
    draw::Texture validity_tx = {"validity_tx"};

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    int3 grid_size = int3(cache->size);
    if (cache->baking.L0) {
      irradiance_a_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L0);
      irradiance_b_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_a);
      irradiance_c_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_b);
      irradiance_d_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_c);
      validity_tx.ensure_3d(GPU_R16F, grid_size, usage, cache->baking.validity);
      if (cache->baking.validity == nullptr) {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else if (cache->irradiance.L0) {
      irradiance_a_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L0);
      irradiance_b_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_a);
      irradiance_c_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_b);
      irradiance_d_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_c);
      validity_tx.ensure_3d(GPU_R8, grid_size, usage);
      if (cache->connectivity.validity) {
        /* TODO(fclem): Make texture creation API work with different data types. */
        GPU_texture_update_sub(validity_tx,
                               GPU_DATA_UBYTE,
                               cache->connectivity.validity,
                               0,
                               0,
                               0,
                               UNPACK3(grid_size));
      }
      else {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else {
      continue;
    }

    if (irradiance_a_tx.is_valid() == false) {
      inst_.info = "Error: Could not allocate irradiance staging texture";
      /* Avoid undefined behavior with uninitialized values. Still load a clear texture. */
      float4 zero(0.0f);
      irradiance_a_tx.ensure_3d(GPU_RGB16F, int3(1), usage, zero);
      irradiance_b_tx.ensure_3d(GPU_RGB16F, int3(1), usage, zero);
      irradiance_c_tx.ensure_3d(GPU_RGB16F, int3(1), usage, zero);
      irradiance_d_tx.ensure_3d(GPU_RGB16F, int3(1), usage, zero);
      validity_tx.ensure_3d(GPU_R16F, int3(1), usage, zero);
    }

    grid_upload_ps_.init();
    grid_upload_ps_.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_LOAD));

    grid_upload_ps_.push_constant("dilation_threshold", grid->dilation_threshold);
    grid_upload_ps_.push_constant("dilation_radius", grid->dilation_radius);
    grid_upload_ps_.push_constant("grid_index", grid->grid_index);
    grid_upload_ps_.bind_ubo("grids_infos_buf", &grids_infos_buf_);
    grid_upload_ps_.bind_ssbo("bricks_infos_buf", &bricks_infos_buf_);
    grid_upload_ps_.bind_texture("irradiance_a_tx", &irradiance_a_tx);
    grid_upload_ps_.bind_texture("irradiance_b_tx", &irradiance_b_tx);
    grid_upload_ps_.bind_texture("irradiance_c_tx", &irradiance_c_tx);
    grid_upload_ps_.bind_texture("irradiance_d_tx", &irradiance_d_tx);
    grid_upload_ps_.bind_texture("validity_tx", &validity_tx);
    grid_upload_ps_.bind_image("irradiance_atlas_img", &irradiance_atlas_tx_);

    /* Note that we take into account the padding border of each brick. */
    int3 grid_size_in_bricks = math::divide_ceil(grid_size, int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
    grid_upload_ps_.dispatch(grid_size_in_bricks);

    inst_.manager->submit(grid_upload_ps_);

    irradiance_a_tx.free();
    irradiance_b_tx.free();
    irradiance_c_tx.free();
    irradiance_d_tx.free();
  }

  do_full_update_ = false;
}

void IrradianceCache::viewport_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (!inst_.is_baking()) {
    debug_pass_draw(view, view_fb);
    display_pass_draw(view, view_fb);
  }
}

void IrradianceCache::debug_pass_draw(View &view, GPUFrameBuffer *view_fb)
{
  switch (inst_.debug_mode) {
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      inst_.info = "Debug Mode: Surfels Normal";
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE:
      inst_.info = "Debug Mode: Surfels Irradiance";
      break;
    default:
      /* Nothing to display. */
      return;
  }

  for (const IrradianceGrid &grid : inst_.light_probes.grid_map_.values()) {
    if (grid.cache == nullptr) {
      continue;
    }

    LightProbeGridCacheFrame *cache = grid.cache->grid_static_cache;

    if (cache->surfels == nullptr || cache->surfels_len == 0) {
      continue;
    }

    debug_surfels_ps_.init();
    debug_surfels_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                DRW_STATE_DEPTH_LESS_EQUAL);
    display_grids_ps_.framebuffer_set(&view_fb);
    debug_surfels_ps_.shader_set(inst_.shaders.static_shader_get(DEBUG_SURFELS));
    debug_surfels_ps_.push_constant("surfel_radius", 1.5f / 4.0f);
    debug_surfels_ps_.push_constant("debug_mode", int(inst_.debug_mode));

    debug_surfels_buf_.resize(cache->surfels_len);
    /* TODO(fclem): Cleanup: Could have a function in draw::StorageArrayBuffer that takes an input
     * data. */
    Span<Surfel> grid_surfels(static_cast<Surfel *>(cache->surfels), cache->surfels_len);
    MutableSpan<Surfel>(debug_surfels_buf_.data(), cache->surfels_len).copy_from(grid_surfels);
    debug_surfels_buf_.push_update();

    debug_surfels_ps_.bind_ssbo("surfels_buf", debug_surfels_buf_);
    debug_surfels_ps_.draw_procedural(GPU_PRIM_TRI_STRIP, cache->surfels_len, 4);

    inst_.manager->submit(debug_surfels_ps_, view);
  }
}

void IrradianceCache::display_pass_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (!display_grids_enabled_) {
    return;
  }

  for (const IrradianceGrid &grid : inst_.light_probes.grid_map_.values()) {
    if (grid.cache == nullptr) {
      continue;
    }

    LightProbeGridCacheFrame *cache = grid.cache->grid_static_cache;

    if (cache == nullptr) {
      continue;
    }

    /* Display texture. Updated for each individual light grid to avoid increasing VRAM usage. */
    draw::Texture irradiance_a_tx = {"irradiance_a_tx"};
    draw::Texture irradiance_b_tx = {"irradiance_b_tx"};
    draw::Texture irradiance_c_tx = {"irradiance_c_tx"};
    draw::Texture irradiance_d_tx = {"irradiance_d_tx"};
    draw::Texture validity_tx = {"validity_tx"};

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    int3 grid_size = int3(cache->size);
    if (cache->baking.L0) {
      irradiance_a_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L0);
      irradiance_b_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_a);
      irradiance_c_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_b);
      irradiance_d_tx.ensure_3d(GPU_RGBA16F, grid_size, usage, (float *)cache->baking.L1_c);
      validity_tx.ensure_3d(GPU_R16F, grid_size, usage, (float *)cache->baking.validity);
      if (cache->baking.validity == nullptr) {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else if (cache->irradiance.L0) {
      irradiance_a_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L0);
      irradiance_b_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_a);
      irradiance_c_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_b);
      irradiance_d_tx.ensure_3d(GPU_RGB16F, grid_size, usage, (float *)cache->irradiance.L1_c);
      validity_tx.ensure_3d(GPU_R8, grid_size, usage);
      if (cache->connectivity.validity) {
        /* TODO(fclem): Make texture creation API work with different data types. */
        GPU_texture_update_sub(validity_tx,
                               GPU_DATA_UBYTE,
                               cache->connectivity.validity,
                               0,
                               0,
                               0,
                               UNPACK3(grid_size));
      }
      else {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else {
      continue;
    }

    display_grids_ps_.init();
    display_grids_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK);
    display_grids_ps_.framebuffer_set(&view_fb);
    display_grids_ps_.shader_set(inst_.shaders.static_shader_get(DISPLAY_PROBE_GRID));

    display_grids_ps_.push_constant("sphere_radius", inst_.scene->eevee.gi_irradiance_draw_size);
    display_grids_ps_.push_constant("grid_resolution", grid_size);
    display_grids_ps_.push_constant("grid_to_world", grid.object_to_world);
    display_grids_ps_.push_constant("world_to_grid", grid.world_to_object);
    /* TODO(fclem): Make it an option when display options are moved to probe DNA. */
    display_grids_ps_.push_constant("display_validity", false);

    display_grids_ps_.bind_texture("irradiance_a_tx", &irradiance_a_tx);
    display_grids_ps_.bind_texture("irradiance_b_tx", &irradiance_b_tx);
    display_grids_ps_.bind_texture("irradiance_c_tx", &irradiance_c_tx);
    display_grids_ps_.bind_texture("irradiance_d_tx", &irradiance_d_tx);
    display_grids_ps_.bind_texture("validity_tx", &validity_tx);

    int sample_count = int(BKE_lightprobe_grid_cache_frame_sample_count(cache));
    int triangle_count = sample_count * 2;
    display_grids_ps_.draw_procedural(GPU_PRIM_TRIS, 1, triangle_count * 3);

    inst_.manager->submit(display_grids_ps_, view);

    irradiance_a_tx.free();
    irradiance_b_tx.free();
    irradiance_c_tx.free();
    irradiance_d_tx.free();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Baking
 * \{ */

void IrradianceBake::init(const Object &probe_object)
{
  const ::LightProbe *lightprobe = static_cast<::LightProbe *>(probe_object.data);
  surfel_density_ = lightprobe->surfel_density;
}

void IrradianceBake::sync()
{
  {
    PassSimple &pass = surfel_light_eval_ps_;
    pass.init();
    /* Apply lights contribution to scene surfel representation. */
    pass.shader_set(inst_.shaders.static_shader_get(SURFEL_LIGHT));
    pass.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
    pass.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    inst_.lights.bind_resources(&pass);
    inst_.shadows.bind_resources(&pass);
    /* Sync with the surfel creation stage. */
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
    pass.dispatch(&dispatch_per_surfel_);
  }
  {
    PassSimple &pass = surfel_ray_build_ps_;
    pass.init();
    {
      PassSimple::Sub &sub = pass.sub("ListBuild");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_BUILD));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_start_buf", &list_start_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_surfel_);
    }
    {
      PassSimple::Sub &sub = pass.sub("ListSort");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_SORT));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_start_buf", &list_start_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_list_);
    }
  }
  {
    PassSimple &pass = surfel_light_propagate_ps_;
    pass.init();
    {
      PassSimple::Sub &sub = pass.sub("RayEval");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_RAY));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      inst_.reflection_probes.bind_resources(&sub);
      sub.push_constant("radiance_src", &radiance_src_);
      sub.push_constant("radiance_dst", &radiance_dst_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_surfel_);
    }
  }
  {
    PassSimple &pass = irradiance_capture_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_RAY));
    pass.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
    pass.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
    inst_.reflection_probes.bind_resources(&pass);
    pass.bind_ssbo("list_start_buf", &list_start_buf_);
    pass.bind_ssbo("list_info_buf", &list_info_buf_);
    pass.push_constant("radiance_src", &radiance_src_);
    pass.bind_image("irradiance_L0_img", &irradiance_L0_tx_);
    pass.bind_image("irradiance_L1_a_img", &irradiance_L1_a_tx_);
    pass.bind_image("irradiance_L1_b_img", &irradiance_L1_b_tx_);
    pass.bind_image("irradiance_L1_c_img", &irradiance_L1_c_tx_);
    pass.bind_image("validity_img", &validity_tx_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.dispatch(&dispatch_per_grid_sample_);
  }
}

void IrradianceBake::surfel_raster_views_sync(const float3 &scene_min, const float3 &scene_max)
{
  using namespace blender::math;

  grid_pixel_extent_ = max(int3(1), int3(surfel_density_ * (scene_max - scene_min)));

  grid_pixel_extent_ = min(grid_pixel_extent_, int3(16384));

  /* We could use multi-view rendering here to avoid multiple submissions but it is unlikely to
   * make any difference. The bottleneck is still the light propagation loop. */
  auto sync_view = [&](View &view, CartesianBasis basis) {
    float3 extent_min = transform_point(invert(basis), scene_min);
    float3 extent_max = transform_point(invert(basis), scene_max);
    float4x4 winmat = projection::orthographic(
        extent_min.x, extent_max.x, extent_min.y, extent_max.y, -extent_min.z, -extent_max.z);
    float4x4 viewinv = from_rotation<float4x4>(to_quaternion<float>(basis));
    view.visibility_test(false);
    view.sync(invert(viewinv), winmat);
  };

  sync_view(view_x_, basis_x_);
  sync_view(view_y_, basis_y_);
  sync_view(view_z_, basis_z_);
}

void IrradianceBake::surfels_create(const Object &probe_object)
{
  /**
   * We rasterize the scene along the 3 axes. Each generated fragment will write a surface element
   * so raster grid density need to match the desired surfel density. We do a first pass to know
   * how much surfel to allocate then render again to create the surfels.
   */
  using namespace blender::math;

  const ::LightProbe *lightprobe = static_cast<::LightProbe *>(probe_object.data);

  int3 grid_resolution = int3(&lightprobe->grid_resolution_x);
  float4x4 grid_local_to_world = invert(float4x4(probe_object.world_to_object));

  dispatch_per_grid_sample_ = math::divide_ceil(grid_resolution, int3(IRRADIANCE_GRID_GROUP_SIZE));
  capture_info_buf_.irradiance_grid_size = grid_resolution;
  capture_info_buf_.irradiance_grid_local_to_world = grid_local_to_world;
  capture_info_buf_.irradiance_grid_world_to_local_rotation = float4x4(
      invert(normalize(float3x3(grid_local_to_world))));

  eGPUTextureUsage texture_usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                                   GPU_TEXTURE_USAGE_HOST_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  /* 32bit float is needed here otherwise we loose too much energy from rounding error during the
   * accumulation when the sample count is above 500. */
  irradiance_L0_tx_.ensure_3d(GPU_RGBA32F, grid_resolution, texture_usage);
  irradiance_L1_a_tx_.ensure_3d(GPU_RGBA32F, grid_resolution, texture_usage);
  irradiance_L1_b_tx_.ensure_3d(GPU_RGBA32F, grid_resolution, texture_usage);
  irradiance_L1_c_tx_.ensure_3d(GPU_RGBA32F, grid_resolution, texture_usage);
  validity_tx_.ensure_3d(GPU_R32F, grid_resolution, texture_usage);
  irradiance_L0_tx_.clear(float4(0.0f));
  irradiance_L1_a_tx_.clear(float4(0.0f));
  irradiance_L1_b_tx_.clear(float4(0.0f));
  irradiance_L1_c_tx_.clear(float4(0.0f));
  validity_tx_.clear(float4(0.0f));

  DRW_stats_group_start("IrradianceBake.SceneBounds");

  {
    draw::Manager &manager = *inst_.manager;
    PassSimple &pass = irradiance_bounds_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_BOUNDS));
    pass.bind_ssbo("capture_info_buf", &capture_info_buf_);
    pass.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
    pass.push_constant("resource_len", int(manager.resource_handle_count()));
    pass.dispatch(
        int3(divide_ceil_u(manager.resource_handle_count(), IRRADIANCE_BOUNDS_GROUP_SIZE), 1, 1));
  }

  /* Raster the scene to query the number of surfel needed. */
  capture_info_buf_.do_surfel_count = false;
  capture_info_buf_.do_surfel_output = false;

  int neg_flt_max = int(0xFF7FFFFFu ^ 0x7FFFFFFFu); /* floatBitsToOrderedInt(-FLT_MAX) */
  int pos_flt_max = 0x7F7FFFFF;                     /* floatBitsToOrderedInt(FLT_MAX) */
  capture_info_buf_.scene_bound_x_min = pos_flt_max;
  capture_info_buf_.scene_bound_y_min = pos_flt_max;
  capture_info_buf_.scene_bound_z_min = pos_flt_max;
  capture_info_buf_.scene_bound_x_max = neg_flt_max;
  capture_info_buf_.scene_bound_y_max = neg_flt_max;
  capture_info_buf_.scene_bound_z_max = neg_flt_max;

  capture_info_buf_.push_update();

  inst_.manager->submit(irradiance_bounds_ps_);

  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  capture_info_buf_.read();

  auto ordered_int_bits_to_float = [](int32_t int_value) -> float {
    int32_t float_bits = (int_value < 0) ? (int_value ^ 0x7FFFFFFF) : int_value;
    return *reinterpret_cast<float *>(&float_bits);
  };

  float3 scene_min = float3(ordered_int_bits_to_float(capture_info_buf_.scene_bound_x_min),
                            ordered_int_bits_to_float(capture_info_buf_.scene_bound_y_min),
                            ordered_int_bits_to_float(capture_info_buf_.scene_bound_z_min));
  float3 scene_max = float3(ordered_int_bits_to_float(capture_info_buf_.scene_bound_x_max),
                            ordered_int_bits_to_float(capture_info_buf_.scene_bound_y_max),
                            ordered_int_bits_to_float(capture_info_buf_.scene_bound_z_max));
  /* To avoid loosing any surface to the clipping planes, add some padding. */
  float epsilon = 1.0f / surfel_density_;
  scene_min -= epsilon;
  scene_max += epsilon;
  surfel_raster_views_sync(scene_min, scene_max);

  scene_bound_sphere_ = float4(midpoint(scene_max, scene_min),
                               distance(scene_max, scene_min) / 2.0f);

  DRW_stats_group_end();

  /* WORKAROUND: Sync camera with correct bounds for light culling. */
  inst_.camera.sync();

  DRW_stats_group_start("IrradianceBake.SurfelsCount");

  /* Raster the scene to query the number of surfel needed. */
  capture_info_buf_.do_surfel_count = true;
  capture_info_buf_.do_surfel_output = false;
  capture_info_buf_.surfel_len = 0u;
  capture_info_buf_.push_update();

  empty_raster_fb_.ensure(transform_point(invert(basis_x_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_x_);
  empty_raster_fb_.ensure(transform_point(invert(basis_y_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_y_);
  empty_raster_fb_.ensure(transform_point(invert(basis_z_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_z_);

  DRW_stats_group_end();

  /* Allocate surfel pool. */
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  capture_info_buf_.read();
  if (capture_info_buf_.surfel_len == 0) {
    /* No surfel to allocated. */
    return;
  }

  /* TODO(fclem): Check for GL limit and abort if the surfel cache doesn't fit the GPU memory. */
  surfels_buf_.resize(capture_info_buf_.surfel_len);
  surfels_buf_.clear_to_zero();

  dispatch_per_surfel_.x = divide_ceil_u(surfels_buf_.size(), SURFEL_GROUP_SIZE);

  DRW_stats_group_start("IrradianceBake.SurfelsCreate");

  /* Raster the scene to generate the surfels. */
  capture_info_buf_.do_surfel_count = true;
  capture_info_buf_.do_surfel_output = true;
  capture_info_buf_.surfel_len = 0u;
  capture_info_buf_.push_update();

  empty_raster_fb_.ensure(transform_point(invert(basis_x_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_x_);
  empty_raster_fb_.ensure(transform_point(invert(basis_y_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_y_);
  empty_raster_fb_.ensure(transform_point(invert(basis_z_), grid_pixel_extent_).xy());
  inst_.pipelines.capture.render(view_z_);

  /* Sync with any other following pass using the surfel buffer. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  /* Read back so that following push_update will contain correct surfel count. */
  capture_info_buf_.read();

  DRW_stats_group_end();
}

void IrradianceBake::surfels_lights_eval()
{
  /* Use the last setup view. This should work since the view is orthographic. */
  /* TODO(fclem): Remove this. It is only present to avoid crash inside `shadows.set_view` */
  inst_.render_buffers.acquire(int2(1));
  inst_.lights.set_view(view_z_, grid_pixel_extent_.xy());
  inst_.shadows.set_view(view_z_);
  inst_.render_buffers.release();

  inst_.manager->submit(surfel_light_eval_ps_, view_z_);
}

void IrradianceBake::raylists_build()
{
  using namespace blender::math;

  float2 rand_uv = inst_.sampling.rng_2d_get(eSamplingDimension::SAMPLING_LENS_U);
  const float3 ray_direction = inst_.sampling.sample_sphere(rand_uv);
  const float3 up = ray_direction;
  const float3 forward = cross(up, normalize(orthogonal(up)));
  const float4x4 viewinv = from_orthonormal_axes<float4x4>(float3(0.0f), forward, up);
  const float4x4 viewmat = invert(viewinv);

  /* Compute projection bounds. */
  float2 min, max;
  min = max = transform_point(viewmat, scene_bound_sphere_.xyz()).xy();
  min -= scene_bound_sphere_.w;
  max += scene_bound_sphere_.w;

  /* This avoid light leaking by making sure that for one surface there will always be at least 1
   * surfel capture inside a ray list. Since the surface with the maximum distance (after
   * projection) between adjacent surfels is a slope that goes through 3 corners of a cube,
   * the distance the grid needs to cover is the diagonal of a cube face.
   *
   * The lower the number the more surfels it clumps together in the same surfel-list.
   * Biasing the grid_density like that will create many invalid link between coplanar surfels.
   * These are dealt with during the list sorting pass.
   *
   * This has a side effect of inflating shadows and emissive surfaces.
   *
   * We add an extra epsilon just in case. We really need this step to be leak free. */
  const float max_distance_between_neighbor_surfels_inv = M_SQRT1_2 - 1e-4;
  /* Surfel list per unit distance. */
  const float ray_grid_density = surfel_density_ * max_distance_between_neighbor_surfels_inv;
  /* Surfel list size in unit distance. */
  const float pixel_size = 1.0f / ray_grid_density;
  list_info_buf_.ray_grid_size = math::max(int2(1), int2(ray_grid_density * (max - min)));

  /* Add a 2 pixels margin to have empty lists for irradiance grid samples to fall into (as they
   * are not considered by the scene bounds). The first pixel margin is because we are jittering
   * the grid position. */
  list_info_buf_.ray_grid_size += int2(4);
  min -= pixel_size * 2.0f;
  max += pixel_size * 2.0f;

  /* Randomize grid center to avoid uneven inflating of corners in some directions. */
  const float2 aa_rand = inst_.sampling.rng_2d_get(eSamplingDimension::SAMPLING_FILTER_U);
  /* Offset in surfel list "pixel". */
  const float2 aa_offset = (aa_rand - 0.5f) * 0.499f;
  min += pixel_size * aa_offset;

  list_info_buf_.list_max = list_info_buf_.ray_grid_size.x * list_info_buf_.ray_grid_size.y;
  list_info_buf_.push_update();

  /* NOTE: Z values do not really matter since we are not doing any rasterization. */
  const float4x4 winmat = projection::orthographic<float>(min.x, max.x, min.y, max.y, 0, 1);

  ray_view_.sync(viewmat, winmat);

  dispatch_per_list_.x = divide_ceil_u(list_info_buf_.list_max, SURFEL_LIST_GROUP_SIZE);

  list_start_buf_.resize(ceil_to_multiple_u(list_info_buf_.list_max, 4));

  GPU_storagebuf_clear(list_start_buf_, -1);
  inst_.manager->submit(surfel_ray_build_ps_, ray_view_);
}

void IrradianceBake::propagate_light()
{
  /* NOTE: Subtract 1 because after `sampling.step()`. */
  capture_info_buf_.sample_index = inst_.sampling.sample_index() - 1;
  capture_info_buf_.sample_count = inst_.sampling.sample_count();
  capture_info_buf_.push_update();

  inst_.manager->submit(surfel_light_propagate_ps_, ray_view_);

  std::swap(radiance_src_, radiance_dst_);
}

void IrradianceBake::irradiance_capture()
{
  inst_.manager->submit(irradiance_capture_ps_, ray_view_);
}

void IrradianceBake::read_surfels(LightProbeGridCacheFrame *cache_frame)
{
  if (!ELEM(inst_.debug_mode,
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL,
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE))
  {
    return;
  }

  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  capture_info_buf_.read();
  surfels_buf_.read();

  cache_frame->surfels_len = capture_info_buf_.surfel_len;
  cache_frame->surfels = MEM_malloc_arrayN(cache_frame->surfels_len, sizeof(Surfel), __func__);

  MutableSpan<Surfel> surfels_dst((Surfel *)cache_frame->surfels, cache_frame->surfels_len);
  Span<Surfel> surfels_src(surfels_buf_.data(), cache_frame->surfels_len);
  surfels_dst.copy_from(surfels_src);
}

LightProbeGridCacheFrame *IrradianceBake::read_result_unpacked()
{
  LightProbeGridCacheFrame *cache_frame = BKE_lightprobe_grid_cache_frame_create();

  read_surfels(cache_frame);

  cache_frame->size[0] = irradiance_L0_tx_.width();
  cache_frame->size[1] = irradiance_L0_tx_.height();
  cache_frame->size[2] = irradiance_L0_tx_.depth();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  cache_frame->baking.L0 = (float(*)[4])irradiance_L0_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_a = (float(*)[4])irradiance_L1_a_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_b = (float(*)[4])irradiance_L1_b_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_c = (float(*)[4])irradiance_L1_c_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.validity = (float *)validity_tx_.read<float>(GPU_DATA_FLOAT);

  return cache_frame;
}

LightProbeGridCacheFrame *IrradianceBake::read_result_packed()
{
  LightProbeGridCacheFrame *cache_frame = BKE_lightprobe_grid_cache_frame_create();

  read_surfels(cache_frame);

  cache_frame->size[0] = irradiance_L0_tx_.width();
  cache_frame->size[1] = irradiance_L0_tx_.height();
  cache_frame->size[2] = irradiance_L0_tx_.depth();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  cache_frame->baking.L0 = (float(*)[4])irradiance_L0_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_a = (float(*)[4])irradiance_L1_a_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_b = (float(*)[4])irradiance_L1_b_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_c = (float(*)[4])irradiance_L1_c_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.validity = (float *)validity_tx_.read<float>(GPU_DATA_FLOAT);

  int64_t sample_count = irradiance_L0_tx_.width() * irradiance_L0_tx_.height() *
                         irradiance_L0_tx_.depth();
  size_t coefficient_texture_size = sizeof(*cache_frame->irradiance.L0) * sample_count;
  size_t validity_texture_size = sizeof(*cache_frame->connectivity.validity) * sample_count;
  cache_frame->irradiance.L0 = (float(*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_a = (float(*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_b = (float(*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_c = (float(*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->connectivity.validity = (uint8_t *)MEM_mallocN(validity_texture_size, __func__);

  /* TODO(fclem): This could be done on GPU if that's faster. */
  for (auto i : IndexRange(sample_count)) {
    copy_v3_v3(cache_frame->irradiance.L0[i], cache_frame->baking.L0[i]);
    copy_v3_v3(cache_frame->irradiance.L1_a[i], cache_frame->baking.L1_a[i]);
    copy_v3_v3(cache_frame->irradiance.L1_b[i], cache_frame->baking.L1_b[i]);
    copy_v3_v3(cache_frame->irradiance.L1_c[i], cache_frame->baking.L1_c[i]);
    cache_frame->connectivity.validity[i] = unit_float_to_uchar_clamp(
        cache_frame->baking.validity[i]);
  }

  MEM_SAFE_FREE(cache_frame->baking.L0);
  MEM_SAFE_FREE(cache_frame->baking.L1_a);
  MEM_SAFE_FREE(cache_frame->baking.L1_b);
  MEM_SAFE_FREE(cache_frame->baking.L1_c);
  MEM_SAFE_FREE(cache_frame->baking.validity);

  // cache_frame->visibility.L0 = irradiance_only_L0_tx_.read<uint8_t>(GPU_DATA_UBYTE);
  // cache_frame->visibility.L1_a = irradiance_only_L1_a_tx_.read<uint8_t>(GPU_DATA_UBYTE);
  // cache_frame->visibility.L1_b = irradiance_only_L1_b_tx_.read<uint8_t>(GPU_DATA_UBYTE);
  // cache_frame->visibility.L1_c = irradiance_only_L1_c_tx_.read<uint8_t>(GPU_DATA_UBYTE);
  // cache_frame->connectivity.validity = validity_packed_.read<uint8_t>(GPU_DATA_FLOAT);

  return cache_frame;
}

/** \} */

}  // namespace blender::eevee
