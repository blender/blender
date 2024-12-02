/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "IMB_imbuf_types.hh"
#include "IMB_interp.hh"

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "image_batches.hh"
#include "image_private.hh"

#include "DNA_windowmanager_types.h"

namespace blender::image_engine {
class Instance;

constexpr float EPSILON_UV_BOUNDS = 0.00001f;

class BaseTextureMethod {
 protected:
  State *instance_data;

 protected:
  BaseTextureMethod(State *instance_data) : instance_data(instance_data) {}

 public:
  /**
   * \brief Ensure enough texture infos are allocated in `instance_data`.
   */
  virtual void ensure_texture_infos() = 0;

  /**
   * \brief Update the uv and region bounds of all texture_infos of instance_data.
   */
  virtual void update_bounds(const ARegion *region) = 0;

  virtual void ensure_gpu_textures_allocation() = 0;
};

/**
 * Uses a single texture that covers the area. Every zoom/pan change requires a full
 * update of the texture.
 */
class OneTexture : public BaseTextureMethod {
 public:
  OneTexture(State *instance_data) : BaseTextureMethod(instance_data) {}
  void ensure_texture_infos() override
  {
    instance_data->texture_infos.resize(1);
  }

  void update_bounds(const ARegion *region) override
  {
    float4x4 mat = math::invert(float4x4(instance_data->ss_to_texture));
    float2 region_uv_min = math::transform_point(mat, float3(0.0f, 0.0f, 0.0f)).xy();
    float2 region_uv_max = math::transform_point(mat, float3(1.0f, 1.0f, 0.0f)).xy();

    TextureInfo &texture_info = instance_data->texture_infos[0];
    texture_info.tile_id = int2(0);
    texture_info.need_full_update = false;
    rctf new_clipping_uv_bounds;
    BLI_rctf_init(&new_clipping_uv_bounds,
                  region_uv_min.x,
                  region_uv_max.x,
                  region_uv_min.y,
                  region_uv_max.y);

    if (memcmp(&new_clipping_uv_bounds, &texture_info.clipping_uv_bounds, sizeof(rctf))) {
      texture_info.clipping_uv_bounds = new_clipping_uv_bounds;
      texture_info.need_full_update = true;
    }

    rcti new_clipping_bounds;
    BLI_rcti_init(&new_clipping_bounds, 0, region->winx, 0, region->winy);
    if (memcmp(&new_clipping_bounds, &texture_info.clipping_bounds, sizeof(rcti))) {
      texture_info.clipping_bounds = new_clipping_bounds;
      texture_info.need_full_update = true;
    }
  }

  void ensure_gpu_textures_allocation() override
  {
    TextureInfo &texture_info = instance_data->texture_infos[0];
    int2 texture_size = int2(BLI_rcti_size_x(&texture_info.clipping_bounds),
                             BLI_rcti_size_y(&texture_info.clipping_bounds));
    texture_info.ensure_gpu_texture(texture_size);
  }
};

/**
 * \brief Screen space method using a multiple textures covering the region.
 *
 * This method improves panning speed, but has some drawing artifacts and
 * therefore isn't selected.
 */
template<size_t Divisions> class ScreenTileTextures : public BaseTextureMethod {
 public:
  static const size_t TexturesPerDimension = Divisions + 1;
  static const size_t TexturesRequired = TexturesPerDimension * TexturesPerDimension;
  static const size_t VerticesPerDimension = TexturesPerDimension + 1;

 private:
  /**
   * \brief Helper struct to pair a texture info and a region in uv space of the area.
   */
  struct TextureInfoBounds {
    TextureInfo *info = nullptr;
    rctf uv_bounds;
    /* Offset of this tile to be drawn on the screen (number of tiles from bottom left corner). */
    int2 tile_id;
  };

 public:
  ScreenTileTextures(State *instance_data) : BaseTextureMethod(instance_data) {}

  /**
   * \brief Ensure enough texture infos are allocated in `instance_data`.
   */
  void ensure_texture_infos() override
  {
    instance_data->texture_infos.resize(TexturesRequired);
  }

  /**
   * \brief Update the uv and region bounds of all texture_infos of instance_data.
   */
  void update_bounds(const ARegion *region) override
  {
    /* determine uv_area of the region. */
    Vector<TextureInfo *> unassigned_textures;
    float4x4 mat = math::invert(float4x4(instance_data->ss_to_texture));
    float2 region_uv_min = math::transform_point(mat, float3(0.0f, 0.0f, 0.0f)).xy();
    float2 region_uv_max = math::transform_point(mat, float3(1.0f, 1.0f, 0.0f)).xy();
    float2 region_uv_span = region_uv_max - region_uv_min;

    /* Calculate uv coordinates of each vert in the grid of textures. */

    /* Construct the uv bounds of the 4 textures that are needed to fill the region. */
    Vector<TextureInfoBounds> info_bounds = create_uv_bounds(region_uv_span, region_uv_min);
    assign_texture_infos_by_uv_bounds(info_bounds, unassigned_textures);
    assign_unused_texture_infos(info_bounds, unassigned_textures);

    /* Calculate the region bounds from the uv bounds. */
    rctf region_uv_bounds;
    BLI_rctf_init(
        &region_uv_bounds, region_uv_min.x, region_uv_max.x, region_uv_min.y, region_uv_max.y);
    update_region_bounds_from_uv_bounds(region_uv_bounds, int2(region->winx, region->winy));
  }

  /**
   * Get the texture size of a single texture for the current settings.
   */
  int2 gpu_texture_size() const
  {
    float2 viewport_size = DRW_viewport_size_get();
    int2 texture_size(ceil(viewport_size.x / Divisions), ceil(viewport_size.y / Divisions));
    return texture_size;
  }

  void ensure_gpu_textures_allocation() override
  {
    int2 texture_size = gpu_texture_size();
    for (TextureInfo &info : instance_data->texture_infos) {
      info.ensure_gpu_texture(texture_size);
    }
  }

 private:
  Vector<TextureInfoBounds> create_uv_bounds(float2 region_uv_span, float2 region_uv_min)
  {
    float2 uv_coords[VerticesPerDimension][VerticesPerDimension];
    float2 region_tile_uv_span = region_uv_span / float2(float(Divisions));
    float2 onscreen_multiple = (blender::math::floor(region_uv_min / region_tile_uv_span) +
                                float2(1.0f)) *
                               region_tile_uv_span;
    for (int y = 0; y < VerticesPerDimension; y++) {
      for (int x = 0; x < VerticesPerDimension; x++) {
        uv_coords[x][y] = region_tile_uv_span * float2(float(x - 1), float(y - 1)) +
                          onscreen_multiple;
      }
    }

    Vector<TextureInfoBounds> info_bounds;
    for (int x = 0; x < TexturesPerDimension; x++) {
      for (int y = 0; y < TexturesPerDimension; y++) {
        TextureInfoBounds texture_info_bounds;
        texture_info_bounds.tile_id = int2(x, y);
        BLI_rctf_init(&texture_info_bounds.uv_bounds,
                      uv_coords[x][y].x,
                      uv_coords[x + 1][y + 1].x,
                      uv_coords[x][y].y,
                      uv_coords[x + 1][y + 1].y);
        info_bounds.append(texture_info_bounds);
      }
    }
    return info_bounds;
  }

  void assign_texture_infos_by_uv_bounds(Vector<TextureInfoBounds> &info_bounds,
                                         Vector<TextureInfo *> &r_unassigned_textures)
  {
    for (TextureInfo &info : instance_data->texture_infos) {
      bool assigned = false;
      for (TextureInfoBounds &info_bound : info_bounds) {
        if (info_bound.info == nullptr &&
            BLI_rctf_compare(&info_bound.uv_bounds, &info.clipping_uv_bounds, 0.001))
        {
          info_bound.info = &info;
          info.tile_id = info_bound.tile_id;
          assigned = true;
          break;
        }
      }
      if (!assigned) {
        r_unassigned_textures.append(&info);
      }
    }
  }

  void assign_unused_texture_infos(Vector<TextureInfoBounds> &info_bounds,
                                   Vector<TextureInfo *> &unassigned_textures)
  {
    for (TextureInfoBounds &info_bound : info_bounds) {
      if (info_bound.info == nullptr) {
        info_bound.info = unassigned_textures.pop_last();
        info_bound.info->tile_id = info_bound.tile_id;
        info_bound.info->need_full_update = true;
        info_bound.info->clipping_uv_bounds = info_bound.uv_bounds;
      }
    }
  }

  void update_region_bounds_from_uv_bounds(const rctf &region_uv_bounds, const int2 region_size)
  {
    rctf region_bounds;
    BLI_rctf_init(&region_bounds, 0.0, region_size.x, 0.0, region_size.y);
    float4x4 uv_to_screen;
    BLI_rctf_transform_calc_m4_pivot_min(&region_uv_bounds, &region_bounds, uv_to_screen.ptr());
    int2 tile_origin(0);
    for (const TextureInfo &info : instance_data->texture_infos) {
      if (info.tile_id == int2(0)) {
        tile_origin = int2(math::transform_point(
            uv_to_screen,
            float3(info.clipping_uv_bounds.xmin, info.clipping_uv_bounds.ymin, 0.0)));
        break;
      }
    }

    const int2 texture_size = gpu_texture_size();
    for (TextureInfo &info : instance_data->texture_infos) {
      int2 bottom_left = tile_origin + texture_size * info.tile_id;
      int2 top_right = bottom_left + texture_size;
      BLI_rcti_init(&info.clipping_bounds, bottom_left.x, top_right.x, bottom_left.y, top_right.y);
    }
  }
};

using namespace blender::bke::image::partial_update;
using namespace blender::bke::image;

class ScreenSpaceDrawingMode : public AbstractDrawingMode {
 private:
  Instance &instance_;

 public:
  ScreenSpaceDrawingMode(Instance &instance) : instance_(instance) {}

 private:
  void add_shgroups() const;

  /**
   * \brief add depth drawing calls.
   *
   * The depth is used to identify if the tile exist or transparent.
   */
  void add_depth_shgroups(::Image *image, ::ImageUser *image_user) const;

  /**
   * \brief Update GPUTextures for drawing the image.
   *
   * GPUTextures that are marked dirty are rebuild. GPUTextures that aren't marked dirty are
   * updated with changed region of the image.
   */
  void update_textures(::Image *image, ::ImageUser *image_user) const;

  /**
   * Update the float buffer in the region given by the partial update checker.
   */
  void do_partial_update_float_buffer(
      ImBuf *float_buffer, PartialUpdateChecker<ImageTileData>::CollectResult &iterator) const;
  void do_partial_update(PartialUpdateChecker<ImageTileData>::CollectResult &iterator) const;
  void do_full_update_for_dirty_textures(const ::ImageUser *image_user) const;
  void do_full_update_gpu_texture(TextureInfo &info, const ::ImageUser *image_user) const;
  /**
   * texture_buffer is the image buffer belonging to the texture_info.
   * tile_buffer is the image buffer of the tile.
   */
  void do_full_update_texture_slot(const TextureInfo &texture_info,
                                   ImBuf &texture_buffer,
                                   ImBuf &tile_buffer,
                                   const ImageTileWrapper &image_tile) const;

 public:
  void begin_sync() const override;
  void image_sync(::Image *image, ::ImageUser *iuser) const override;
  void draw_finish() const override;
  void draw_viewport() const override;
};

}  // namespace blender::image_engine
