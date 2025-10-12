/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_lightprobe_types.h"

#include "BKE_global.hh"
#include "BKE_lightprobe.h"

#include "GPU_capabilities.hh"

#include "GPU_debug.hh"

#include "eevee_debug_shared.hh"
#include "eevee_instance.hh"
#include "eevee_lightprobe_volume.hh"

#include <cstdio>

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Interface
 * \{ */

void VolumeProbeModule::init()
{
  display_grids_enabled_ = inst_.draw_overlays;

  /* This might become an option in the future. */
  bool use_l2_band = false;
  int sh_coef_len = use_l2_band ? 9 : 4;
  BLI_assert(gpu::TextureFormat::VOLUME_PROBE_FORMAT == gpu::TextureFormat::SFLOAT_16_16_16_16);
  int texel_byte_size = 8; /* Assumes gpu::TextureFormat::SFLOAT_16_16_16_16. */
  uint atlas_col_count = 0;
  uint atlas_row_count = 0;

  if (assign_if_different(irradiance_pool_size_,
                          uint(inst_.scene->eevee.gi_irradiance_pool_size)) ||
      !irradiance_atlas_tx_.is_valid())
  {
    irradiance_atlas_tx_.free();
    /* Find highest pool size within device limits. */
    for (uint irradiance_pool_size = irradiance_pool_size_;
         irradiance_pool_size >= 16 && !irradiance_atlas_tx_.is_valid();
         irradiance_pool_size >>= 1)
    {
      int atlas_byte_size = 1024 * 1024 * irradiance_pool_size;
      /* Reshape texture to improve grid occupancy within device limits. */
      constexpr uint atlas_col_count_min = 16;
      constexpr uint atlas_col_count_max = 16384;
      for (uint atlas_col_count_try = atlas_col_count_min;
           atlas_col_count_try <= atlas_col_count_max && !irradiance_atlas_tx_.is_valid();
           atlas_col_count_try <<= 1)
      {
        int3 atlas_extent(IRRADIANCE_GRID_BRICK_SIZE);
        atlas_extent.z *= sh_coef_len;
        /* Add space for validity bits. */
        atlas_extent.z += IRRADIANCE_GRID_BRICK_SIZE / 4;
        atlas_extent.x *= atlas_col_count_try;

        /* Determine the row count depending on the scene settings. */
        int row_byte_size = math::reduce_mul(atlas_extent) * texel_byte_size;
        atlas_row_count = divide_ceil_u(atlas_byte_size, row_byte_size);
        atlas_extent.y *= atlas_row_count;

        constexpr eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE |
                                           GPU_TEXTURE_USAGE_SHADER_READ |
                                           GPU_TEXTURE_USAGE_ATTACHMENT;
        irradiance_atlas_tx_.ensure_3d(
            gpu::TextureFormat::VOLUME_PROBE_FORMAT, atlas_extent, usage);
        if (irradiance_atlas_tx_.is_valid()) {
          do_full_update_ = true;
          irradiance_pool_size_alloc_ = irradiance_pool_size;
          atlas_col_count = atlas_col_count_try;
        }
      }
    }
  }
  if (irradiance_pool_size_alloc_ != irradiance_pool_size_) {
    inst_.info_append_i18n(
        "Warning: Could not allocate light probes volume pool of {} MB, using {} MB instead.",
        irradiance_pool_size_,
        irradiance_pool_size_alloc_);
  }

  if (do_full_update_) {
    do_full_update_ = false;
    do_update_world_ = true;

    /* Delete all references to existing bricks. */
    for (VolumeProbe &grid : inst_.light_probes.volume_map_.values()) {
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
  }

  if (irradiance_atlas_tx_.is_valid() == false) {
    inst_.info_append_i18n("Irradiance Atlas texture could not be created");
  }
}

void VolumeProbeModule::sync()
{
  if (inst_.is_baking()) {
    bake.sync();
  }
}

Vector<IrradianceBrickPacked> VolumeProbeModule::bricks_alloc(int brick_len)
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

void VolumeProbeModule::bricks_free(Vector<IrradianceBrickPacked> &bricks)
{
  brick_pool_.extend(bricks.as_span());
  bricks.clear();
}

void VolumeProbeModule::set_view(View & /*view*/)
{
  Vector<VolumeProbe *> grid_loaded;

  bool any_update = false;
  /* First allocate the needed bricks and populate the brick buffer. */
  bricks_infos_buf_.clear();
  for (VolumeProbe &grid : inst_.light_probes.volume_map_.values()) {
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
      inst_.info_append_i18n("Error: Malformed irradiance grid data");
      continue;
    }

    /* TODO frustum cull and only load visible grids. */

    /* Note that we reserve 1 slot for the world irradiance. */
    if (grid_loaded.size() >= IRRADIANCE_GRID_MAX - 1) {
      inst_.info_append_i18n("Error: Too many irradiance grids in the scene");
      /* TODO frustum cull and only load visible grids. */
      // inst_.info_append_i18n("Error: Too many grid visible");
      continue;
    }

    int3 grid_size_with_padding = grid_size + 2;
    if (grid.bricks.is_empty()) {
      int3 grid_size_in_bricks = math::divide_ceil(grid_size_with_padding,
                                                   int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
      int brick_len = grid_size_in_bricks.x * grid_size_in_bricks.y * grid_size_in_bricks.z;
      grid.bricks = bricks_alloc(brick_len);

      if (grid.bricks.is_empty()) {
        inst_.info_append_i18n("Error: Irradiance grid allocation failed");
        continue;
      }
      grid.do_update = true;
    }

    if (do_update_world_) {
      /* Update grid composition if world changed. */
      grid.do_update = true;
    }

    any_update = any_update || grid.do_update;

    grid.brick_offset = bricks_infos_buf_.size();
    bricks_infos_buf_.extend(grid.bricks);

    float4x4 grid_to_world = grid.object_to_world * math::from_location<float4x4>(float3(-1.0f)) *
                             math::from_scale<float4x4>(
                                 float3(2.0f / float3(grid_size_with_padding - 1))) *
                             math::from_location<float4x4>(float3(-0.5f));

    grid.world_to_grid_transposed = float3x4(math::transpose(math::invert(grid_to_world)));
    grid.grid_size_padded = grid_size_with_padding;
    grid_loaded.append(&grid);
  }

  /* TODO: This is greedy update detection. We should check if a change can influence each grid
   * before tagging update. But this is a bit too complex and update is quite cheap. So we update
   * everything if there is any update on any grid. */
  if (any_update) {
    for (VolumeProbe *grid : grid_loaded) {
      grid->do_update = true;
    }
  }

  /* Then create brick & grid infos UBOs content. */
  int world_grid_index = 0;
  {
    /* Stable sorting of grids. */
    std::sort(
        grid_loaded.begin(), grid_loaded.end(), [](const VolumeProbe *a, const VolumeProbe *b) {
          float volume_a = math::determinant(float3x3(a->object_to_world));
          float volume_b = math::determinant(float3x3(b->object_to_world));
          if (volume_a != volume_b) {
            /* Smallest first. */
            return volume_a < volume_b;
          }
          /* Volumes are identical. Any arbitrary criteria can be used to sort them.
           * Use position to avoid unstable result caused by depsgraph non deterministic eval
           * order. This could also become a priority parameter. */
          float3 _a = a->object_to_world.location();
          float3 _b = b->object_to_world.location();
          if (_a.x != _b.x) {
            return _a.x < _b.x;
          }
          if (_a.y != _b.y) {
            return _a.y < _b.y;
          }
          if (_a.z != _b.z) {
            return _a.z < _b.z;
          }
          /* Fallback to memory address, since there's no good alternative. */
          return a < b;
        });

    /* Insert grids in UBO in sorted order. */
    int grids_len = 0;
    for (VolumeProbe *grid : grid_loaded) {
      grid->grid_index = grids_len;
      grids_infos_buf_[grids_len++] = *grid;
    }

    /* Insert world grid last. */
    world_grid_index = grids_len++;

    VolumeProbeData grid;
    grid.world_to_grid_transposed = float3x4::identity();
    grid.grid_size_padded = int3(1);
    grid.brick_offset = bricks_infos_buf_.size();
    grid.normal_bias = 0.0f;
    grid.view_bias = 0.0f;
    grid.facing_bias = 0.0f;
    grids_infos_buf_[world_grid_index] = grid;

    bricks_infos_buf_.append(world_brick_index_);

    if (grids_len < IRRADIANCE_GRID_MAX) {
      /* Tag last grid as invalid to stop the iteration. */
      grids_infos_buf_[grids_len].grid_size_padded = int3(-1);
    }

    bricks_infos_buf_.push_update();
    grids_infos_buf_.push_update();
  }

  /* Upload data for world. */
  if (do_update_world_) {
    grid_upload_ps_.init();
    grid_upload_ps_.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_WORLD));
    grid_upload_ps_.bind_resources(inst_.uniform_data);
    grid_upload_ps_.bind_ssbo("harmonic_buf", &inst_.sphere_probes.spherical_harmonics_buf());
    grid_upload_ps_.bind_ubo("grids_infos_buf", &grids_infos_buf_);
    grid_upload_ps_.bind_ssbo("bricks_infos_buf", &bricks_infos_buf_);
    grid_upload_ps_.push_constant("grid_index", world_grid_index);
    grid_upload_ps_.bind_image("irradiance_atlas_img", &irradiance_atlas_tx_);
    /* Sync with extraction. */
    grid_upload_ps_.barrier(GPU_BARRIER_SHADER_STORAGE);
    /* Only upload one brick. */
    grid_upload_ps_.dispatch(int3(1));
    /* Sync with next load. */
    grid_upload_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);

    inst_.manager->submit(grid_upload_ps_);
  }

  /* Upload data for each grid that need to be inserted in the atlas.
   * Upload by order of dependency. */
  /* Start at world index to not load any other grid (+1 because we decrement at loop start). */
  int grid_start_index = grid_loaded.size() + 1;
  for (auto it = grid_loaded.rbegin(); it != grid_loaded.rend(); ++it) {
    grid_start_index--;

    VolumeProbe *grid = *it;
    if (!grid->do_update) {
      continue;
    }

    grid->do_update = false;

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
      irradiance_a_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L0);
      irradiance_b_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_a);
      irradiance_c_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_b);
      irradiance_d_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_c);
      validity_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, cache->baking.validity);
      if (cache->baking.validity == nullptr) {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else if (cache->irradiance.L0) {
      irradiance_a_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L0);
      irradiance_b_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_a);
      irradiance_c_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_b);
      irradiance_d_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_c);
      validity_tx.ensure_3d(gpu::TextureFormat::UNORM_8, grid_size, usage);
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
      inst_.info_append_i18n("Error: Could not allocate irradiance staging texture");
      /* Avoid undefined behavior with uninitialized values. Still load a clear texture. */
      const float4 zero(0.0f);
      irradiance_a_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16, int3(1), usage, zero);
      irradiance_b_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16, int3(1), usage, zero);
      irradiance_c_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16, int3(1), usage, zero);
      irradiance_d_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16, int3(1), usage, zero);
      validity_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16, int3(1), usage, zero);
    }

    bool visibility_available = cache->visibility.L0 != nullptr;
    bool is_baking = cache->irradiance.L0 == nullptr;

    draw::Texture visibility_a_tx = {"visibility_a_tx"};
    draw::Texture visibility_b_tx = {"visibility_b_tx"};
    draw::Texture visibility_c_tx = {"visibility_c_tx"};
    draw::Texture visibility_d_tx = {"visibility_d_tx"};
    if (visibility_available) {
      visibility_a_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, (const float *)cache->visibility.L0);
      visibility_b_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, (const float *)cache->visibility.L1_a);
      visibility_c_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, (const float *)cache->visibility.L1_b);
      visibility_d_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, (const float *)cache->visibility.L1_c);

      GPU_texture_swizzle_set(visibility_a_tx, "111r");
      GPU_texture_swizzle_set(visibility_b_tx, "111r");
      GPU_texture_swizzle_set(visibility_c_tx, "111r");
      GPU_texture_swizzle_set(visibility_d_tx, "111r");
    }
    else if (!is_baking) {
      /* Missing visibility. Load default visibility L0 = 1, L1 = (0, 0, 0). */
      GPU_texture_swizzle_set(irradiance_a_tx, "rgb1");
      GPU_texture_swizzle_set(irradiance_b_tx, "rgb0");
      GPU_texture_swizzle_set(irradiance_c_tx, "rgb0");
      GPU_texture_swizzle_set(irradiance_d_tx, "rgb0");
    }

    grid_upload_ps_.init();
    grid_upload_ps_.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_LOAD));

    grid_upload_ps_.bind_resources(inst_.uniform_data);
    grid_upload_ps_.push_constant("validity_threshold", grid->validity_threshold);
    grid_upload_ps_.push_constant("dilation_threshold", grid->dilation_threshold);
    grid_upload_ps_.push_constant("dilation_radius", grid->dilation_radius);
    grid_upload_ps_.push_constant("grid_index", grid->grid_index);
    grid_upload_ps_.push_constant("grid_start_index", grid_start_index);
    grid_upload_ps_.push_constant("grid_local_to_world", grid->object_to_world);
    grid_upload_ps_.push_constant("grid_intensity_factor", grid->intensity);
    grid_upload_ps_.bind_ubo("grids_infos_buf", &grids_infos_buf_);
    grid_upload_ps_.bind_ssbo("bricks_infos_buf", &bricks_infos_buf_);
    grid_upload_ps_.bind_texture("irradiance_a_tx", &irradiance_a_tx);
    grid_upload_ps_.bind_texture("irradiance_b_tx", &irradiance_b_tx);
    grid_upload_ps_.bind_texture("irradiance_c_tx", &irradiance_c_tx);
    grid_upload_ps_.bind_texture("irradiance_d_tx", &irradiance_d_tx);
    grid_upload_ps_.bind_texture("validity_tx", &validity_tx);
    grid_upload_ps_.bind_image("irradiance_atlas_img", &irradiance_atlas_tx_);
    /* NOTE: We are read and writing the same texture that we are sampling from. If that causes an
     * issue, we should revert to manual trilinear interpolation. */
    grid_upload_ps_.bind_texture("irradiance_atlas_tx", &irradiance_atlas_tx_);
    /* If visibility is invalid, either it is still baking and visibility is stored with
     * irradiance, or it is missing and we sample a completely uniform visibility. */
    bool use_vis = visibility_available;
    grid_upload_ps_.bind_texture("visibility_a_tx", use_vis ? &visibility_a_tx : &irradiance_a_tx);
    grid_upload_ps_.bind_texture("visibility_b_tx", use_vis ? &visibility_b_tx : &irradiance_b_tx);
    grid_upload_ps_.bind_texture("visibility_c_tx", use_vis ? &visibility_c_tx : &irradiance_c_tx);
    grid_upload_ps_.bind_texture("visibility_d_tx", use_vis ? &visibility_d_tx : &irradiance_d_tx);

    /* Runtime grid is padded for blending with surrounding probes. */
    int3 grid_size_with_padding = grid_size + 2;
    /* Note that we take into account the padding border of each brick. */
    int3 grid_size_in_bricks = math::divide_ceil(grid_size_with_padding,
                                                 int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
    grid_upload_ps_.dispatch(grid_size_in_bricks);
    /* Sync with next load. */
    grid_upload_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);

    inst_.manager->submit(grid_upload_ps_);

    irradiance_a_tx.free();
    irradiance_b_tx.free();
    irradiance_c_tx.free();
    irradiance_d_tx.free();
  }

  do_update_world_ = false;
}

void VolumeProbeModule::viewport_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (!inst_.is_baking()) {
    debug_pass_draw(view, view_fb);
    display_pass_draw(view, view_fb);
  }
}

void VolumeProbeModule::debug_pass_draw(View &view, gpu::FrameBuffer *view_fb)
{
  switch (inst_.debug_mode) {
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      inst_.info_append("Debug Mode: Surfels Normal");
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER:
      inst_.info_append("Debug Mode: Surfels Cluster");
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE:
      inst_.info_append("Debug Mode: Surfels Irradiance");
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY:
      inst_.info_append("Debug Mode: Surfels Visibility");
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_VALIDITY:
      inst_.info_append("Debug Mode: Irradiance Validity");
      break;
    case eDebugMode::DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET:
      inst_.info_append("Debug Mode: Virtual Offset");
      break;
    default:
      /* Nothing to display. */
      return;
  }

  for (const VolumeProbe &grid : inst_.light_probes.volume_map_.values()) {
    if (grid.cache == nullptr) {
      continue;
    }

    LightProbeGridCacheFrame *cache = grid.cache->grid_static_cache;

    if (cache == nullptr) {
      continue;
    }

    switch (inst_.debug_mode) {
      case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER:
      case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY:
      case eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE: {
        if (cache->surfels == nullptr || cache->surfels_len == 0) {
          continue;
        }
        float max_axis_len = math::reduce_max(math::to_scale(grid.object_to_world));
        debug_ps_.init();
        debug_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                            DRW_STATE_CLIP_CONTROL_UNIT_RANGE | inst_.film.depth.test_state);
        debug_ps_.framebuffer_set(&view_fb);
        debug_ps_.shader_set(inst_.shaders.static_shader_get(DEBUG_SURFELS));
        debug_ps_.push_constant("debug_surfel_radius", 0.5f * max_axis_len / grid.surfel_density);
        debug_ps_.push_constant("debug_mode", int(inst_.debug_mode));

        debug_surfels_buf_.resize(cache->surfels_len);
        /* TODO(fclem): Cleanup: Could have a function in draw::StorageArrayBuffer that takes an
         * input data. */
        Span<Surfel> grid_surfels(static_cast<Surfel *>(cache->surfels), cache->surfels_len);
        MutableSpan<Surfel>(debug_surfels_buf_.data(), cache->surfels_len).copy_from(grid_surfels);
        debug_surfels_buf_.push_update();

        debug_ps_.bind_ssbo("surfels_buf", debug_surfels_buf_);
        debug_ps_.draw_procedural(GPU_PRIM_TRI_STRIP, cache->surfels_len, 4);

        inst_.manager->submit(debug_ps_, view);
        break;
      }

      case eDebugMode::DEBUG_IRRADIANCE_CACHE_VALIDITY:
      case eDebugMode::DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET: {
        int3 grid_size = int3(cache->size);
        debug_ps_.init();
        debug_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                            DRW_STATE_CLIP_CONTROL_UNIT_RANGE | inst_.film.depth.test_state);
        debug_ps_.framebuffer_set(&view_fb);
        debug_ps_.shader_set(inst_.shaders.static_shader_get(DEBUG_IRRADIANCE_GRID));
        debug_ps_.push_constant("debug_mode", int(inst_.debug_mode));
        debug_ps_.push_constant("grid_mat", grid.object_to_world);

        eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
        Texture debug_data_tx = {"debug_data_tx"};

        if (inst_.debug_mode == eDebugMode::DEBUG_IRRADIANCE_CACHE_VALIDITY) {
          const float *data;
          if (cache->baking.validity) {
            data = cache->baking.validity;
            debug_data_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16, grid_size, usage, data);
          }
          else if (cache->connectivity.validity) {
            debug_data_tx.ensure_3d(gpu::TextureFormat::UNORM_8, grid_size, usage);
            /* TODO(fclem): Make texture creation API work with different data types. */
            GPU_texture_update_sub(debug_data_tx,
                                   GPU_DATA_UBYTE,
                                   cache->connectivity.validity,
                                   0,
                                   0,
                                   0,
                                   UNPACK3(grid_size));
          }
          else {
            continue;
          }
          debug_ps_.push_constant("debug_value", grid.validity_threshold);
          debug_ps_.bind_texture("debug_data_tx", debug_data_tx);
          debug_ps_.draw_procedural(GPU_PRIM_POINTS, 1, grid_size.x * grid_size.y * grid_size.z);
        }
        else {
          if (cache->baking.virtual_offset) {
            const float *data = (const float *)cache->baking.virtual_offset;
            debug_data_tx.ensure_3d(
                gpu::TextureFormat::SFLOAT_16_16_16_16, grid_size, usage, data);
          }
          else {
            continue;
          }
          debug_ps_.bind_texture("debug_data_tx", debug_data_tx);
          debug_ps_.draw_procedural(
              GPU_PRIM_LINES, 1, grid_size.x * grid_size.y * grid_size.z * 2);
        }

        inst_.manager->submit(debug_ps_, view);
        break;
      }

      default:
        break;
    }
  }
}

void VolumeProbeModule::display_pass_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (!display_grids_enabled_) {
    return;
  }

  for (const VolumeProbe &grid : inst_.light_probes.volume_map_.values()) {
    if (!grid.viewport_display || grid.viewport_display_size == 0.0f || !grid.cache ||
        !grid.cache->grid_static_cache)
    {
      continue;
    }

    LightProbeGridCacheFrame *cache = grid.cache->grid_static_cache;

    /* Display texture. Updated for each individual light grid to avoid increasing VRAM usage. */
    draw::Texture irradiance_a_tx = {"irradiance_a_tx"};
    draw::Texture irradiance_b_tx = {"irradiance_b_tx"};
    draw::Texture irradiance_c_tx = {"irradiance_c_tx"};
    draw::Texture irradiance_d_tx = {"irradiance_d_tx"};
    draw::Texture validity_tx = {"validity_tx"};

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    int3 grid_size = int3(cache->size);
    if (cache->baking.L0) {
      irradiance_a_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L0);
      irradiance_b_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_a);
      irradiance_c_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_b);
      irradiance_d_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->baking.L1_c);
      validity_tx.ensure_3d(
          gpu::TextureFormat::SFLOAT_16, grid_size, usage, (const float *)cache->baking.validity);
      if (cache->baking.validity == nullptr) {
        /* Avoid displaying garbage data. */
        validity_tx.clear(float4(0.0));
      }
    }
    else if (cache->irradiance.L0) {
      irradiance_a_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L0);
      irradiance_b_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_a);
      irradiance_c_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_b);
      irradiance_d_tx.ensure_3d(gpu::TextureFormat::SFLOAT_16_16_16,
                                grid_size,
                                usage,
                                (const float *)cache->irradiance.L1_c);
      validity_tx.ensure_3d(gpu::TextureFormat::UNORM_8, grid_size, usage);
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
                                DRW_STATE_CLIP_CONTROL_UNIT_RANGE | inst_.film.depth.test_state |
                                DRW_STATE_CULL_BACK);
    display_grids_ps_.framebuffer_set(&view_fb);
    display_grids_ps_.shader_set(inst_.shaders.static_shader_get(DISPLAY_PROBE_VOLUME));

    display_grids_ps_.push_constant("sphere_radius", grid.viewport_display_size);
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
  float max_axis_len = math::reduce_max(math::to_scale(probe_object.object_to_world()));

  const ::LightProbe &lightprobe = DRW_object_get_data_for_drawing<::LightProbe>(probe_object);
  surfel_density_ = lightprobe.grid_surfel_density / max_axis_len;
  min_distance_to_surface_ = lightprobe.grid_surface_bias;
  max_virtual_offset_ = lightprobe.grid_escape_bias;
  clip_distance_ = lightprobe.clipend;
  capture_world_ = (lightprobe.grid_flag & LIGHTPROBE_GRID_CAPTURE_WORLD);
  capture_indirect_ = (lightprobe.grid_flag & LIGHTPROBE_GRID_CAPTURE_INDIRECT);
  capture_emission_ = (lightprobe.grid_flag & LIGHTPROBE_GRID_CAPTURE_EMISSION);

  /* Initialize views data, since they're used by other modules. */
  surfel_raster_views_sync(float3(0.0f), float3(1.0f), float4x4::identity());
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
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.lights);
    pass.bind_resources(inst_.shadows);
    /* Sync with the surfel creation stage. */
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
    pass.dispatch(&dispatch_per_surfel_);
  }
  {
    PassSimple &pass = surfel_cluster_build_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(SURFEL_CLUSTER_BUILD));
    pass.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
    pass.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
    pass.bind_image("cluster_list_img", &cluster_list_tx_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    pass.dispatch(&dispatch_per_surfel_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    PassSimple &pass = surfel_ray_build_ps_;
    pass.init();
    {
      /* Count number of surfel per list. */
      PassSimple::Sub &sub = pass.sub("ListPrepare");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_PREPARE));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_counter_buf", &list_counter_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_surfel_);
    }
    {
      /* Prefix sum of list sizes. Outputs an IndexRange per list. */
      PassSimple::Sub &sub = pass.sub("ListPrefix");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_PREFIX));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_counter_buf", &list_counter_buf_);
      sub.bind_ssbo("list_range_buf", &list_range_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_list_);
    }
    {
      /* Copy surfel list sorting data into a flat array.
       * All lists data are contiguous in memory using the IndexRange from previous pass. */
      PassSimple::Sub &sub = pass.sub("ListFlatten");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_FLATTEN));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_counter_buf", &list_counter_buf_);
      sub.bind_ssbo("list_range_buf", &list_range_buf_);
      sub.bind_ssbo("list_item_distance_buf", &list_item_distance_buf_);
      sub.bind_ssbo("list_item_surfel_id_buf", &list_item_surfel_id_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_surfel_);
    }
    {
      /* Radix sort of the list. Output surfel index in the sorted list. */
      PassSimple::Sub &sub = pass.sub("ListSort");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_SORT));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_range_buf", &list_range_buf_);
      sub.bind_ssbo("list_item_surfel_id_buf", &list_item_surfel_id_buf_);
      sub.bind_ssbo("list_item_distance_buf", &list_item_distance_buf_);
      sub.bind_ssbo("sorted_surfel_id_buf", &sorted_surfel_id_buf_);
      sub.bind_ssbo("list_info_buf", &list_info_buf_);
      sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      sub.dispatch(&dispatch_per_surfel_);
    }
    {
      /* Take the sorted lists array and copy adjacent surfel indices back to the Surfels.
       * Also relink coplanar surfels to avoid over shadowing. */
      PassSimple::Sub &sub = pass.sub("ListBuild");
      sub.shader_set(inst_.shaders.static_shader_get(SURFEL_LIST_BUILD));
      sub.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
      sub.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
      sub.bind_ssbo("list_start_buf", &list_start_buf_);
      sub.bind_ssbo("list_range_buf", &list_range_buf_);
      sub.bind_ssbo("sorted_surfel_id_buf", &sorted_surfel_id_buf_);
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
      sub.bind_resources(inst_.sphere_probes);
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
    pass.bind_resources(inst_.sphere_probes);
    pass.bind_ssbo("list_start_buf", &list_start_buf_);
    pass.bind_ssbo("list_info_buf", &list_info_buf_);
    pass.push_constant("radiance_src", &radiance_src_);
    pass.bind_image("irradiance_L0_img", &irradiance_L0_tx_);
    pass.bind_image("irradiance_L1_a_img", &irradiance_L1_a_tx_);
    pass.bind_image("irradiance_L1_b_img", &irradiance_L1_b_tx_);
    pass.bind_image("irradiance_L1_c_img", &irradiance_L1_c_tx_);
    pass.bind_image("validity_img", &validity_tx_);
    pass.bind_image("virtual_offset_img", &virtual_offset_tx_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.dispatch(&dispatch_per_grid_sample_);
  }
  {
    PassSimple &pass = irradiance_offset_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(LIGHTPROBE_IRRADIANCE_OFFSET));
    pass.bind_ssbo(SURFEL_BUF_SLOT, &surfels_buf_);
    pass.bind_ssbo(CAPTURE_BUF_SLOT, &capture_info_buf_);
    pass.bind_ssbo("list_start_buf", &list_start_buf_);
    pass.bind_ssbo("list_info_buf", &list_info_buf_);
    pass.bind_image("cluster_list_img", &cluster_list_tx_);
    pass.bind_image("virtual_offset_img", &virtual_offset_tx_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.dispatch(&dispatch_per_grid_sample_);
  }
}

void IrradianceBake::surfel_raster_views_sync(const float3 &scene_min,
                                              const float3 &scene_max,
                                              const float4x4 &probe_to_world)
{
  using namespace blender::math;

  float3 location, scale;
  Quaternion rotation;
  to_loc_rot_scale(probe_to_world, location, rotation, scale);
  /* Remove scale from view matrix. */
  float4x4 viewinv = from_loc_rot_scale<float4x4>(location, rotation, float3(1.0f));
  float4x4 viewmat = invert(viewinv);

  /* Compute the intersection between the grid and the scene extents. */
  float3 extent_min = float3(FLT_MAX);
  float3 extent_max = float3(-FLT_MAX);
  for (int x : {0, 1}) {
    for (int y : {0, 1}) {
      for (int z : {0, 1}) {
        float3 ws_corner = scene_min + ((scene_max - scene_min) * float3(x, y, z));
        float3 ls_corner = transform_point(viewmat, ws_corner);
        extent_min = min(extent_min, ls_corner);
        extent_max = max(extent_max, ls_corner);
      }
    }
  }
  /* Clip distance is added to every axis in both directions, not just Z. */
  float3 target_extent = scale + clip_distance_;
  extent_min = max(extent_min, -target_extent);
  extent_max = min(extent_max, target_extent);

  grid_pixel_extent_ = max(int3(1), int3(surfel_density_ * (extent_max - extent_min)));
  grid_pixel_extent_ = min(grid_pixel_extent_, int3(16384));

  float3 ls_midpoint = midpoint(extent_min, extent_max);
  scene_bound_sphere_ = float4(transform_point(viewinv, ls_midpoint),
                               distance(extent_min, extent_max) / 2.0f);

  /* We could use multi-view rendering here to avoid multiple submissions but it is unlikely to
   * make any difference. The bottleneck is still the light propagation loop. */
  auto sync_view = [&](View &view, CartesianBasis basis) {
    float4x4 capture_viewinv = viewinv * from_rotation<float4x4>(basis);

    float3 capture_extent_min = transform_point(invert(basis), extent_min);
    float3 capture_extent_max = transform_point(invert(basis), extent_max);

    float4x4 capture_winmat = projection::orthographic(capture_extent_min.x,
                                                       capture_extent_max.x,
                                                       capture_extent_min.y,
                                                       capture_extent_max.y,
                                                       -capture_extent_min.z,
                                                       -capture_extent_max.z);

    view.visibility_test(false);
    view.sync(invert(capture_viewinv), capture_winmat);
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

  const ::LightProbe &lightprobe = DRW_object_get_data_for_drawing<::LightProbe>(probe_object);

  int3 grid_resolution = int3(&lightprobe.grid_resolution_x);
  float4x4 grid_local_to_world = invert(probe_object.world_to_object());
  float3 grid_scale = math::to_scale(probe_object.object_to_world());

  /* TODO(fclem): Options. */
  capture_info_buf_.capture_world_direct = capture_world_;
  capture_info_buf_.capture_world_indirect = capture_world_ && capture_indirect_;
  capture_info_buf_.capture_visibility_direct = !capture_world_;
  capture_info_buf_.capture_visibility_indirect = !(capture_world_ && capture_indirect_);
  capture_info_buf_.capture_indirect = capture_indirect_;
  capture_info_buf_.capture_emission = capture_emission_;

  LightProbeModule &light_probes = inst_.light_probes;
  SphereProbeData &world_data = *static_cast<SphereProbeData *>(&light_probes.world_sphere_);
  capture_info_buf_.world_atlas_coord = world_data.atlas_coord;

  dispatch_per_grid_sample_ = math::divide_ceil(grid_resolution, int3(IRRADIANCE_GRID_GROUP_SIZE));
  capture_info_buf_.irradiance_grid_size = grid_resolution;
  capture_info_buf_.irradiance_grid_local_to_world = grid_local_to_world;
  capture_info_buf_.irradiance_grid_world_to_local = probe_object.world_to_object();
  capture_info_buf_.irradiance_grid_world_to_local_rotation = float4x4(
      invert(normalize(float3x3(grid_local_to_world))));

  capture_info_buf_.min_distance_to_surface = min_distance_to_surface_;
  capture_info_buf_.max_virtual_offset = max_virtual_offset_;
  capture_info_buf_.surfel_radius = 0.5f / surfel_density_;
  /* Make virtual offset distances scale relative. */
  float min_distance_between_grid_samples = math::reduce_min(grid_scale / float3(grid_resolution));
  capture_info_buf_.min_distance_to_surface *= min_distance_between_grid_samples;
  capture_info_buf_.max_virtual_offset *= min_distance_between_grid_samples;
  capture_info_buf_.clamp_direct = (lightprobe.grid_clamp_direct > 0.0) ?
                                       lightprobe.grid_clamp_direct :
                                       1e20f;
  capture_info_buf_.clamp_indirect = (lightprobe.grid_clamp_indirect > 0.0) ?
                                         lightprobe.grid_clamp_indirect :
                                         1e20f;

  eGPUTextureUsage texture_usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                                   GPU_TEXTURE_USAGE_HOST_READ;

  /* 32bit float is needed here otherwise we loose too much energy from rounding error during the
   * accumulation when the sample count is above 500. */
  irradiance_L0_tx_.ensure_3d(
      gpu::TextureFormat::SFLOAT_32_32_32_32, grid_resolution, texture_usage);
  irradiance_L1_a_tx_.ensure_3d(
      gpu::TextureFormat::SFLOAT_32_32_32_32, grid_resolution, texture_usage);
  irradiance_L1_b_tx_.ensure_3d(
      gpu::TextureFormat::SFLOAT_32_32_32_32, grid_resolution, texture_usage);
  irradiance_L1_c_tx_.ensure_3d(
      gpu::TextureFormat::SFLOAT_32_32_32_32, grid_resolution, texture_usage);
  validity_tx_.ensure_3d(gpu::TextureFormat::SFLOAT_32, grid_resolution, texture_usage);
  virtual_offset_tx_.ensure_3d(
      gpu::TextureFormat::SFLOAT_16_16_16_16, grid_resolution, texture_usage);

  if (!irradiance_L0_tx_.is_valid() || !irradiance_L1_a_tx_.is_valid() ||
      !irradiance_L1_b_tx_.is_valid() || !irradiance_L1_c_tx_.is_valid() ||
      !validity_tx_.is_valid() || !virtual_offset_tx_.is_valid())
  {
    inst_.info_append_i18n("Error: Not enough memory to bake {}.", probe_object.id.name);
    do_break_ = true;
    return;
  }

  irradiance_L0_tx_.clear(float4(0.0f));
  irradiance_L1_a_tx_.clear(float4(0.0f));
  irradiance_L1_b_tx_.clear(float4(0.0f));
  irradiance_L1_c_tx_.clear(float4(0.0f));
  validity_tx_.clear(float4(0.0f));
  virtual_offset_tx_.clear(float4(0.0f));

  GPU_debug_group_begin("IrradianceBake.SceneBounds");

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

  const int neg_flt_max = int(0xFF7FFFFFu ^ 0x7FFFFFFFu); /* floatBitsToOrderedInt(-FLT_MAX) */
  const int pos_flt_max = 0x7F7FFFFF;                     /* floatBitsToOrderedInt(FLT_MAX) */
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

  if (capture_info_buf_.scene_bound_x_min == pos_flt_max) {
    /* No valid object has been found. */
    do_break_ = true;
    return;
  }

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
  surfel_raster_views_sync(scene_min, scene_max, probe_object.object_to_world());

  GPU_debug_group_end();

  /* WORKAROUND: Sync camera with correct bounds for light culling. */
  inst_.camera.sync();
  /* WORKAROUND: Sync shadows tile-maps count again with new camera bounds. Fixes issues with sun
   * lights. */
  inst_.shadows.end_sync();
  inst_.lights.end_sync();

  GPU_debug_group_begin("IrradianceBake.SurfelsCount");

  /* Raster the scene to query the number of surfel needed. */
  capture_info_buf_.do_surfel_count = true;
  capture_info_buf_.do_surfel_output = false;
  capture_info_buf_.surfel_len = 0u;
  capture_info_buf_.push_update();

  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_x_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_x_);
  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_y_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_y_);
  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_z_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_z_);

  GPU_debug_group_end();

  /* Allocate surfel pool. */
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  capture_info_buf_.read();
  if (capture_info_buf_.surfel_len == 0) {
    /* No surfel to allocate. */
    return;
  }

  if (capture_info_buf_.surfel_len > surfels_buf_.size()) {
    CLOG_INFO(
        &Instance::log, "IrradianceBake: Allocating %u surfels.", capture_info_buf_.surfel_len);

    size_t max_size = GPU_max_storage_buffer_size();
    if (GPU_mem_stats_supported()) {
      int total_mem_kb, free_mem_kb;
      GPU_mem_stats_get(&total_mem_kb, &free_mem_kb);
      /* Leave at least 128MByte for OS and stuffs.
       * Try to avoid crashes because of OUT_OF_MEMORY errors. */
      size_t max_alloc = (size_t(total_mem_kb) - 128 * 1024) * 1024;
      /* Cap to 95% of available memory. */
      size_t max_free = size_t((size_t(free_mem_kb) * 1024) * 0.95f);

      max_size = min(max_size, min(max_alloc, max_free));
    }

    size_t required_mem = sizeof(Surfel) * (capture_info_buf_.surfel_len - surfels_buf_.size());
    if (required_mem > max_size) {
      const bool is_ssbo_bound = (max_size == GPU_max_storage_buffer_size());
      const uint req_mb = required_mem / (1024 * 1024);
      const uint max_mb = max_size / (1024 * 1024);

      if (is_ssbo_bound) {
        inst_.info_append_i18n(
            "Cannot allocate enough video memory to bake \"{}\" ({} / {} MBytes).\n"
            "Try reducing surfel resolution or capture distance to lower the size of the "
            "allocation.",
            probe_object.id.name,
            req_mb,
            max_mb);
      }
      else {
        inst_.info_append_i18n(
            "Not enough available video memory to bake \"{}\" ({} / {} MBytes).\n"
            "Try reducing surfel resolution or capture distance to lower the size of the "
            "allocation.",
            probe_object.id.name,
            req_mb,
            max_mb);
      }

      if (G.background) {
        /* Print something in background mode instead of failing silently. */
        fprintf(stderr, "%s", inst_.info_get());
      }

      do_break_ = true;
      return;
    }
  }

  surfels_buf_.resize(capture_info_buf_.surfel_len);
  surfels_buf_.clear_to_zero();

  dispatch_per_surfel_.x = divide_ceil_u(surfels_buf_.size(), SURFEL_GROUP_SIZE);

  GPU_debug_group_begin("IrradianceBake.SurfelsCreate");

  /* Raster the scene to generate the surfels. */
  capture_info_buf_.do_surfel_count = true;
  capture_info_buf_.do_surfel_output = true;
  capture_info_buf_.surfel_len = 0u;
  capture_info_buf_.push_update();

  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_x_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_x_);
  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_y_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_y_);
  empty_raster_fb_.ensure(math::abs(transform_point(invert(basis_z_), grid_pixel_extent_).xy()));
  inst_.pipelines.capture.render(view_z_);

  /* Sync with any other following pass using the surfel buffer. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  /* Read back so that following push_update will contain correct surfel count. */
  capture_info_buf_.read();

  GPU_debug_group_end();
}

void IrradianceBake::surfels_lights_eval()
{
  /* Use the last setup view. This should work since the view is orthographic. */
  /* TODO(fclem): Remove this. It is only present to avoid crash inside `shadows.set_view` */
  inst_.render_buffers.acquire(int2(1));
  inst_.hiz_buffer.set_source(&inst_.render_buffers.depth_tx);
  inst_.lights.set_view(view_z_, grid_pixel_extent_.xy());
  inst_.shadows.set_view(view_z_, grid_pixel_extent_.xy());
  if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY)) {
    /* There seems to be a synchronization issue with shadow rendering pass. If not waiting, the
     * surfels are lit without shadows. Waiting for sync here shouldn't be a huge bottleneck
     * anyway. */
    GPU_finish();
  }
  inst_.render_buffers.release();

  inst_.manager->submit(surfel_light_eval_ps_, view_z_);
}

void IrradianceBake::clusters_build()
{
  if (max_virtual_offset_ == 0.0f) {
    return;
  }
  eGPUTextureUsage texture_usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                                   GPU_TEXTURE_USAGE_ATOMIC;

  cluster_list_tx_.ensure_3d(
      gpu::TextureFormat::SINT_32, capture_info_buf_.irradiance_grid_size, texture_usage);
  cluster_list_tx_.clear(int4(-1));
  /* View is not important here. It is only for validation. */
  inst_.manager->submit(surfel_cluster_build_ps_, view_z_);
}

void IrradianceBake::irradiance_offset()
{
  if (max_virtual_offset_ == 0.0f) {
    /* NOTE: Virtual offset texture should already have been cleared to 0. */
    return;
  }

  inst_.manager->submit(irradiance_offset_ps_, view_z_);

  /* Not needed after this point. */
  cluster_list_tx_.free();
}

void IrradianceBake::raylists_build()
{
  using namespace blender::math;

  float2 rand_uv = inst_.sampling.rng_2d_get(eSamplingDimension::SAMPLING_LENS_U);
  const float3 ray_direction = Sampling::sample_sphere(rand_uv);
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
  list_counter_buf_.resize(ceil_to_multiple_u(list_info_buf_.list_max, 4));
  list_range_buf_.resize(ceil_to_multiple_u(list_info_buf_.list_max * 2, 4));

  list_item_distance_buf_.resize(ceil_to_multiple_u(max_ii(1, capture_info_buf_.surfel_len), 4));
  list_item_surfel_id_buf_.resize(ceil_to_multiple_u(max_ii(1, capture_info_buf_.surfel_len), 4));
  sorted_surfel_id_buf_.resize(ceil_to_multiple_u(max_ii(1, capture_info_buf_.surfel_len), 4));

  GPU_storagebuf_clear(list_counter_buf_, 0);
  /* Clear for the case where there are no list or no surfel.
   * Otherwise the irradiance_capture stage will have broken lists. */
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
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER,
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL,
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE,
            eDebugMode::DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY))
  {
    return;
  }

  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  capture_info_buf_.read();
  surfels_buf_.read();

  cache_frame->surfels_len = capture_info_buf_.surfel_len;
  cache_frame->surfels = MEM_malloc_arrayN<Surfel>(cache_frame->surfels_len, __func__);

  MutableSpan<Surfel> surfels_dst((Surfel *)cache_frame->surfels, cache_frame->surfels_len);
  Span<Surfel> surfels_src(surfels_buf_.data(), cache_frame->surfels_len);
  surfels_dst.copy_from(surfels_src);
}

void IrradianceBake::read_virtual_offset(LightProbeGridCacheFrame *cache_frame)
{
  if (!ELEM(inst_.debug_mode, eDebugMode::DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET)) {
    return;
  }

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  cache_frame->baking.virtual_offset = (float (*)[4])virtual_offset_tx_.read<float4>(
      GPU_DATA_FLOAT);
}

LightProbeGridCacheFrame *IrradianceBake::read_result_unpacked()
{
  LightProbeGridCacheFrame *cache_frame = BKE_lightprobe_grid_cache_frame_create();

  read_surfels(cache_frame);
  read_virtual_offset(cache_frame);

  cache_frame->size[0] = irradiance_L0_tx_.width();
  cache_frame->size[1] = irradiance_L0_tx_.height();
  cache_frame->size[2] = irradiance_L0_tx_.depth();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  cache_frame->baking.L0 = (float (*)[4])irradiance_L0_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_a = (float (*)[4])irradiance_L1_a_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_b = (float (*)[4])irradiance_L1_b_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_c = (float (*)[4])irradiance_L1_c_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.validity = (float *)validity_tx_.read<float>(GPU_DATA_FLOAT);

  return cache_frame;
}

LightProbeGridCacheFrame *IrradianceBake::read_result_packed()
{
  LightProbeGridCacheFrame *cache_frame = BKE_lightprobe_grid_cache_frame_create();

  read_surfels(cache_frame);
  read_virtual_offset(cache_frame);

  cache_frame->size[0] = irradiance_L0_tx_.width();
  cache_frame->size[1] = irradiance_L0_tx_.height();
  cache_frame->size[2] = irradiance_L0_tx_.depth();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  cache_frame->baking.L0 = (float (*)[4])irradiance_L0_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_a = (float (*)[4])irradiance_L1_a_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_b = (float (*)[4])irradiance_L1_b_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.L1_c = (float (*)[4])irradiance_L1_c_tx_.read<float4>(GPU_DATA_FLOAT);
  cache_frame->baking.validity = (float *)validity_tx_.read<float>(GPU_DATA_FLOAT);

  int64_t sample_count = int64_t(irradiance_L0_tx_.width()) * irradiance_L0_tx_.height() *
                         irradiance_L0_tx_.depth();
  size_t coefficient_texture_size = sizeof(*cache_frame->irradiance.L0) * sample_count;
  size_t validity_texture_size = sizeof(*cache_frame->connectivity.validity) * sample_count;
  cache_frame->irradiance.L0 = (float (*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_a = (float (*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_b = (float (*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->irradiance.L1_c = (float (*)[3])MEM_mallocN(coefficient_texture_size, __func__);
  cache_frame->connectivity.validity = (uint8_t *)MEM_mallocN(validity_texture_size, __func__);

  size_t visibility_texture_size = sizeof(*cache_frame->irradiance.L0) * sample_count;
  cache_frame->visibility.L0 = (float *)MEM_mallocN(visibility_texture_size, __func__);
  cache_frame->visibility.L1_a = (float *)MEM_mallocN(visibility_texture_size, __func__);
  cache_frame->visibility.L1_b = (float *)MEM_mallocN(visibility_texture_size, __func__);
  cache_frame->visibility.L1_c = (float *)MEM_mallocN(visibility_texture_size, __func__);

  /* TODO(fclem): This could be done on GPU if that's faster. */
  for (auto i : IndexRange(sample_count)) {
    copy_v3_v3(cache_frame->irradiance.L0[i], cache_frame->baking.L0[i]);
    copy_v3_v3(cache_frame->irradiance.L1_a[i], cache_frame->baking.L1_a[i]);
    copy_v3_v3(cache_frame->irradiance.L1_b[i], cache_frame->baking.L1_b[i]);
    copy_v3_v3(cache_frame->irradiance.L1_c[i], cache_frame->baking.L1_c[i]);

    cache_frame->visibility.L0[i] = cache_frame->baking.L0[i][3];
    cache_frame->visibility.L1_a[i] = cache_frame->baking.L1_a[i][3];
    cache_frame->visibility.L1_b[i] = cache_frame->baking.L1_b[i][3];
    cache_frame->visibility.L1_c[i] = cache_frame->baking.L1_c[i][3];
    cache_frame->connectivity.validity[i] = unit_float_to_uchar_clamp(
        cache_frame->baking.validity[i]);
  }

  MEM_SAFE_FREE(cache_frame->baking.L0);
  MEM_SAFE_FREE(cache_frame->baking.L1_a);
  MEM_SAFE_FREE(cache_frame->baking.L1_b);
  MEM_SAFE_FREE(cache_frame->baking.L1_c);
  MEM_SAFE_FREE(cache_frame->baking.validity);

  return cache_frame;
}

/** \} */

}  // namespace blender::eevee
