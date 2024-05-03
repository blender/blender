/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The shadow module manages shadow update tagging & shadow rendering.
 */

#include "BKE_global.hh"
#include "BLI_math_rotation.h"
#include "BLI_rect.h"

#include "eevee_instance.hh"

#include "draw_debug.hh"
#include <iostream>

namespace blender::eevee {

ShadowTechnique ShadowModule::shadow_technique = ShadowTechnique::ATOMIC_RASTER;

/* -------------------------------------------------------------------- */
/** \name Tile map
 *
 * \{ */

void ShadowTileMap::sync_orthographic(const float4x4 &object_mat_,
                                      int2 origin_offset,
                                      int clipmap_level,
                                      float lod_bias_,
                                      float filter_radius_,
                                      eShadowProjectionType projection_type_)
{
  if ((projection_type != projection_type_) || (level != clipmap_level) ||
      (filter_radius != filter_radius_))
  {
    set_dirty();
  }
  projection_type = projection_type_;
  level = clipmap_level;
  light_type = eLightType::LIGHT_SUN;
  is_area_side = false;
  filter_radius = filter_radius_;

  if (grid_shift == int2(0)) {
    /* Only replace shift if it is not already dirty. */
    grid_shift = origin_offset - grid_offset;
  }
  grid_offset = origin_offset;

  if (!equals_m4m4(object_mat.ptr(), object_mat_.ptr())) {
    object_mat = object_mat_;
    set_dirty();
  }

  lod_bias = lod_bias_;

  float tile_size = ShadowDirectional::tile_size_get(level);

  /* object_mat is a rotation matrix. Reduce imprecision by taking the transpose which is also the
   * inverse in this particular case. */
  viewmat = math::transpose(object_mat);

  half_size = ShadowDirectional::coverage_get(level) / 2.0f;
  center_offset = float2(grid_offset) * tile_size;
  orthographic_m4(winmat.ptr(),
                  -half_size + center_offset.x,
                  half_size + center_offset.x,
                  -half_size + center_offset.y,
                  half_size + center_offset.y,
                  /* Near/far is computed on GPU using casters bounds. */
                  -1.0,
                  1.0);
}

void ShadowTileMap::sync_cubeface(eLightType light_type_,
                                  const float4x4 &object_mat_,
                                  float near_,
                                  float far_,
                                  float side_,
                                  float shift,
                                  eCubeFace face,
                                  float filter_radius_,
                                  float lod_bias_)
{
  if (projection_type != SHADOW_PROJECTION_CUBEFACE || (cubeface != face)) {
    set_dirty();
  }
  projection_type = SHADOW_PROJECTION_CUBEFACE;
  cubeface = face;
  grid_offset = int2(0);
  lod_bias = lod_bias_;
  light_type = light_type_;
  is_area_side = is_area_light(light_type) && (face != eCubeFace::Z_NEG);

  if ((clip_near != near_) || (filter_radius != filter_radius_) || (clip_far != far_) ||
      (half_size != side_))
  {
    set_dirty();
  }

  filter_radius = filter_radius_;
  clip_near = near_;
  clip_far = far_;
  area_shift = shift;
  half_size = side_;
  center_offset = float2(0.0f);

  if (!equals_m4m4(object_mat.ptr(), object_mat_.ptr())) {
    object_mat = object_mat_;
    set_dirty();
  }

  winmat = math::projection::perspective(
      -half_size, half_size, -half_size, half_size, clip_near, clip_far);
  viewmat = float4x4(shadow_face_mat[cubeface]) *
            math::from_location<float4x4>(float3(0.0f, 0.0f, -shift)) * math::invert(object_mat);

  /* Update corners. */
  float4x4 viewinv = object_mat;
  corners[0] = float4(viewinv.location(), 0.0f);
  corners[1] = float4(math::transform_point(viewinv, float3(-far_, -far_, -far_)), 0.0f);
  corners[2] = float4(math::transform_point(viewinv, float3(far_, -far_, -far_)), 0.0f);
  corners[3] = float4(math::transform_point(viewinv, float3(-far_, far_, -far_)), 0.0f);
  /* Store deltas. */
  corners[2] = (corners[2] - corners[1]) / float(SHADOW_TILEMAP_RES);
  corners[3] = (corners[3] - corners[1]) / float(SHADOW_TILEMAP_RES);
}

void ShadowTileMap::debug_draw() const
{
  /** Used for debug drawing. */
  const float4 debug_color[6] = {
      {1.0f, 0.1f, 0.1f, 1.0f},
      {0.1f, 1.0f, 0.1f, 1.0f},
      {0.0f, 0.2f, 1.0f, 1.0f},
      {1.0f, 1.0f, 0.3f, 1.0f},
      {0.1f, 0.1f, 0.1f, 1.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
  };
  float4 color =
      debug_color[((projection_type == SHADOW_PROJECTION_CUBEFACE ? cubeface : level) + 9999) % 6];

  float4x4 persinv = winmat * viewmat;
  drw_debug_matrix_as_bbox(math::invert(persinv), color);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tile map pool
 *
 * \{ */

ShadowTileMapPool::ShadowTileMapPool()
{
  free_indices.reserve(SHADOW_MAX_TILEMAP);
  /* Reverse order to help debugging (first allocated tile-map will get 0). */
  for (int i = SHADOW_MAX_TILEMAP - 1; i >= 0; i--) {
    free_indices.append(i * SHADOW_TILEDATA_PER_TILEMAP);
  }

  int2 extent;
  extent.x = min_ii(SHADOW_MAX_TILEMAP, maps_per_row) * ShadowTileMap::tile_map_resolution;
  extent.y = (SHADOW_MAX_TILEMAP / maps_per_row) * ShadowTileMap::tile_map_resolution;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                           GPU_TEXTURE_USAGE_ATTACHMENT;
  tilemap_tx.ensure_2d(GPU_R32UI, extent, usage);
  tilemap_tx.clear(uint4(0));
}

ShadowTileMap *ShadowTileMapPool::acquire()
{
  if (free_indices.is_empty()) {
    /* Grow the tile-map buffer. See `end_sync`. */
    for (auto i : IndexRange(free_indices.size(), SHADOW_MAX_TILEMAP)) {
      free_indices.append(i * SHADOW_TILEDATA_PER_TILEMAP);
    }
  }
  int index = free_indices.pop_last();
  return &tilemap_pool.construct(ShadowTileMap(index));
}

void ShadowTileMapPool::release(Span<ShadowTileMap *> free_list)
{
  for (ShadowTileMap *map : free_list) {
    free_indices.append(map->tiles_index);
    tilemap_pool.destruct(*map);
  }
}

void ShadowTileMapPool::end_sync(ShadowModule &module)
{
  tilemaps_data.push_update();

  uint needed_tilemap_capacity = (free_indices.size() + tilemap_pool.size());
  if (needed_tilemap_capacity != (tiles_data.size() / SHADOW_TILEDATA_PER_TILEMAP)) {
    tiles_data.resize(needed_tilemap_capacity * SHADOW_TILEDATA_PER_TILEMAP);
    tilemaps_clip.resize(needed_tilemap_capacity);
    /* We reallocated the tile-map buffer, discarding all the data it contained.
     * We need to re-initialize the page heaps. */
    module.do_full_update = true;
  }

  tilemaps_unused.clear();
  int64_t newly_unused_count = free_indices.size() - last_free_len;
  if (newly_unused_count > 0) {
    /* Upload tile-map indices which pages needs to be pushed back to the free page heap. */
    Span<uint> newly_unused_indices = free_indices.as_span().slice(last_free_len,
                                                                   newly_unused_count);
    for (uint index : newly_unused_indices) {
      /* Push a dummy tile-map to a unused tile-map buffer. It is then processed through the some
       * of the setup steps to release the pages. */
      ShadowTileMapData tilemap_data = {};
      tilemap_data.tiles_index = index;
      tilemap_data.clip_data_index = -1;
      tilemap_data.grid_shift = int2(SHADOW_TILEMAP_RES);
      tilemap_data.projection_type = SHADOW_PROJECTION_CUBEFACE;

      tilemaps_unused.append(tilemap_data);
    }
    tilemaps_unused.push_update();
  }

  last_free_len = free_indices.size();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Punctual
 *
 * \{ */

void ShadowPunctual::sync(eLightType light_type,
                          const float4x4 &object_mat,
                          float cone_aperture,
                          float light_shape_radius,
                          float max_distance,
                          float softness_factor,
                          float shadow_radius)
{
  if (is_spot_light(light_type)) {
    tilemaps_needed_ = (cone_aperture > DEG2RADF(90.0f)) ? 5 : 1;
  }
  else if (is_area_light(light_type)) {
    tilemaps_needed_ = 5;
  }
  else {
    tilemaps_needed_ = 6;
  }

  /* Clamp for near/far clip distance calculation. */
  max_distance_ = max_ff(max_distance, 4e-4f);
  light_radius_ = min_ff(light_shape_radius, max_distance_ - 1e-4f);
  light_type_ = light_type;

  position_ = float3(object_mat[3]);
  softness_factor_ = softness_factor;
  shadow_radius_ = shadow_radius;
}

void ShadowPunctual::release_excess_tilemaps()
{
  if (tilemaps_.size() <= tilemaps_needed_) {
    return;
  }
  auto span = tilemaps_.as_span();
  shadows_.tilemap_pool.release(span.drop_front(tilemaps_needed_));
  tilemaps_ = span.take_front(tilemaps_needed_);
}

void ShadowPunctual::compute_projection_boundaries(eLightType light_type,
                                                   float light_radius,
                                                   float shadow_radius,
                                                   float max_lit_distance,
                                                   float &near,
                                                   float &far,
                                                   float &side,
                                                   float &back_shift)
{
  /*
   * In order to make sure we can trace any ray in its entirety using a single tile-map, we have
   * to make sure that the tile-map cover all potential occluder that can intersect any ray shot
   * in this particular shadow quadrant.
   *
   * To this end, we inflate the tile-map perspective sides to make sure the
   * tile-map frustum starts where the rays cannot go.
   *
   * We are interesting in finding `I` the new origin and `n` the new near plane distances.
   *
   *                                              I .... Intersection between tangent and
   *                                             /|      projection center axis
   *                                            / |
   *                                           /  |
   *                                          /   |
   *                                         /    |
   *                                        /     |
   *                                       /      |
   *                                      /       |
   *                                     /        |
   *                                    /         |
   *                                   /      ....|
   *                                  /   ....    |
   *                                 / ...        |
   *                                /.            |
   *                               /              |
   *  Tangent to light shape .... T\--------------N
   *                             /  --\ Beta      |
   *                            /      -\         |
   *                           /         --\      |
   *                          /.            --\   |
   *                         / .               -\ |
   *                        /  .           Alpha -O .... Light center
   *                       /   .              /-/ |
   *  Inflated side       /    .        /--- -/   |
   *                 .   /      . /----  --/      |
   *                  . /   /---- .   --/         |
   *                   /-------------/------------X .... Desired near plane (inscribed cube)
   *              /----         --/ ..            |
   *         /----   /       --/      ...         |
   *    /----       /     --/            ....     |
   *               /    -/                    ....| .... Shadow radius
   *              /  --/                          |
   *             /--/                             |
   *            F .... Most distant shadow receiver possible.
   *
   * F: The most distant shadowed point at the edge of the 45° cube-face pyramid.
   * O: The light origin.
   * T: The tangent to the circle of radius `radius` centered at the origin and passing through F.
   * I: Intersection between tangent and the projection center axis.
   * N: The shifted near plane center.
   * X: Intersection between the near plane and the projection center axis.
   * Alpha: FOT angle.
   * Beta: OTN angle.
   *
   * NOTE: FTO, ONT and TNI are right angles.
   */
  float cos_alpha = shadow_radius / max_lit_distance;
  float sin_alpha = sqrt(1.0f - math::square(cos_alpha));
  float near_shift = M_SQRT2 * shadow_radius * 0.5f * (sin_alpha - cos_alpha);
  float side_shift = M_SQRT2 * shadow_radius * 0.5f * (sin_alpha + cos_alpha);
  float origin_shift = M_SQRT2 * shadow_radius / (sin_alpha - cos_alpha);

  float min_near = (max_lit_distance / 4000.0f) / M_SQRT3;

  if (is_area_light(light_type)) {
    /* Make near plane be inside the inscribed cube of the shadow sphere. */
    near = max_ff(shadow_radius / M_SQRT3, min_near);
    /* Subtract min_near to make the shadow center match the light center if there is no shadow
     * tracing required. This avoid light leaking issues near the light plane caused by the
     * shadow discard clipping. */
    back_shift = (near - min_near);
  }
  else {
    /* Make near plane be inside the inscribed cube of the light sphere. */
    near = max_ff(light_radius / M_SQRT3, min_near);
    back_shift = 0.0f;
  }

  far = max_lit_distance;
  if (shadow_radius > 1e-5f) {
    side = ((side_shift / (origin_shift - near_shift)) * (origin_shift + near));
  }
  else {
    side = near;
  }
}

void ShadowPunctual::end_sync(Light &light, float lod_bias)
{
  ShadowTileMapPool &tilemap_pool = shadows_.tilemap_pool;

  float side, near, far, shift;
  compute_projection_boundaries(
      light.type, light_radius_, shadow_radius_, max_distance_, near, far, side, shift);

  float4x4 obmat_tmp = light.object_to_world;

  /* Acquire missing tile-maps. */
  while (tilemaps_.size() < tilemaps_needed_) {
    tilemaps_.append(tilemap_pool.acquire());
  }

  tilemaps_[Z_NEG]->sync_cubeface(
      light.type, obmat_tmp, near, far, side, shift, Z_NEG, light.pcf_radius, lod_bias);
  if (tilemaps_needed_ >= 5) {
    tilemaps_[X_POS]->sync_cubeface(
        light.type, obmat_tmp, near, far, side, shift, X_POS, light.pcf_radius, lod_bias);
    tilemaps_[X_NEG]->sync_cubeface(
        light.type, obmat_tmp, near, far, side, shift, X_NEG, light.pcf_radius, lod_bias);
    tilemaps_[Y_POS]->sync_cubeface(
        light.type, obmat_tmp, near, far, side, shift, Y_POS, light.pcf_radius, lod_bias);
    tilemaps_[Y_NEG]->sync_cubeface(
        light.type, obmat_tmp, near, far, side, shift, Y_NEG, light.pcf_radius, lod_bias);
  }
  if (tilemaps_needed_ == 6) {
    tilemaps_[Z_POS]->sync_cubeface(
        light.type, obmat_tmp, near, far, side, shift, Z_POS, light.pcf_radius, lod_bias);
  }

  light.tilemap_index = tilemap_pool.tilemaps_data.size();

  light.local.tilemaps_count = tilemaps_needed_;
  /* TODO(fclem): `as_uint()`. */
  union {
    float f;
    int32_t i;
  } as_int;
  as_int.f = near;
  light.clip_near = as_int.i;
  as_int.f = far;
  light.clip_far = as_int.i;
  light.local.clip_side = side;
  light.local.shadow_projection_shift = shift;
  light.local.shadow_scale = softness_factor_;

  for (ShadowTileMap *tilemap : tilemaps_) {
    /* Add shadow tile-maps grouped by lights to the GPU buffer. */
    tilemap_pool.tilemaps_data.append(*tilemap);
    tilemap->set_updated();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Directional Shadow Maps
 *
 * In order to improve shadow map density, we switch between two tile-map distribution mode.
 * One is beater suited for large FOV (clip-map), the other for really small FOV or Orthographic
 * projections (cascade).
 *
 * Clip-map distribution centers a number of log2 sized tile-maps around the view position.
 * https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry
 *
 * Cascade distribution puts tile-maps along the frustum projection to the light space.
 * https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
 *
 * We choose to distribute cascades linearly to achieve uniform density and simplify lookup.
 * Using clip-map instead of cascades for perspective view also allows for better caching.
 * \{ */

eShadowProjectionType ShadowDirectional::directional_distribution_type_get(const Camera &camera)
{
  /* TODO(fclem): Enable the cascade projection if the FOV is tiny in perspective mode. */
  return camera.is_perspective() ? SHADOW_PROJECTION_CLIPMAP : SHADOW_PROJECTION_CASCADE;
}

/************************************************************************
 *                         Cascade Distribution                         *
 ************************************************************************/

void ShadowDirectional::cascade_tilemaps_distribution_near_far_points(const Camera &camera,
                                                                      float3 &near_point,
                                                                      float3 &far_point)
{
  const CameraData &cam_data = camera.data_get();
  /* Ideally we should only take the intersection with the scene bounds. */
  far_point = (camera.position() - camera.forward() * cam_data.clip_far) *
              float3x3(object_mat_.view<3, 3>());
  near_point = (camera.position() - camera.forward() * cam_data.clip_near) *
               float3x3(object_mat_.view<3, 3>());
}

/* \note All tile-maps are meant to have the same LOD but we still return a range starting at the
 * unique LOD. */
IndexRange ShadowDirectional::cascade_level_range(const Camera &camera, float lod_bias)
{
  using namespace blender::math;

  /* 16 is arbitrary. To avoid too much tile-map per directional lights. */
  const int max_tilemap_per_shadows = 16;
  const CameraData &cam_data = camera.data_get();

  float3 near_point, far_point;
  cascade_tilemaps_distribution_near_far_points(camera, near_point, far_point);

  /* This gives the maximum resolution in depth we can have with a fixed set of tile-maps. Gives
   * the best results when view direction is orthogonal to the light direction. */
  float depth_range_in_shadow_space = distance(far_point.xy(), near_point.xy());
  float min_depth_tilemap_size = 2 * (depth_range_in_shadow_space / max_tilemap_per_shadows);
  /* This allow coverage of the whole view with a single tile-map if camera forward is colinear
   * with the light direction. */
  float min_diagonal_tilemap_size = cam_data.screen_diagonal_length;

  if (camera.is_perspective()) {
    /* Use the far plane diagonal if using perspective. */
    min_diagonal_tilemap_size *= cam_data.clip_far / cam_data.clip_near;
  }

  /* Allow better tile-map usage without missing pages near end of view. */
  lod_bias += 0.5f;
  /* Level of detail (or size) of every tile-maps of this light. */
  int lod_level = ceil(log2(max_ff(min_depth_tilemap_size, min_diagonal_tilemap_size)) + lod_bias);

  /* Tile-maps "rotate" around the first one so their effective range is only half their size. */
  float per_tilemap_coverage = ShadowDirectional::coverage_get(lod_level) * 0.5f;
  /* Number of tile-maps needed to cover the whole view. */
  /* NOTE: floor + 0.5 to avoid 0 when parallel. */
  int tilemap_len = ceil(0.5f + depth_range_in_shadow_space / per_tilemap_coverage);
  return IndexRange(lod_level, tilemap_len);
}

/**
 * Distribute tile-maps in a linear pattern along camera forward vector instead of a clipmap
 * centered on camera position.
 */
void ShadowDirectional::cascade_tilemaps_distribution(Light &light, const Camera &camera)
{
  using namespace blender::math;

  /* All tile-maps use the first level size. */
  float half_size = ShadowDirectional::coverage_get(levels_range.first()) / 2.0f;
  float tile_size = ShadowDirectional::tile_size_get(levels_range.first());

  float3 near_point, far_point;
  cascade_tilemaps_distribution_near_far_points(camera, near_point, far_point);

  float2 local_view_direction = normalize(far_point.xy() - near_point.xy());
  float2 farthest_tilemap_center = local_view_direction * half_size * (levels_range.size() - 1);

  /* Offset for smooth level transitions. */
  light.object_to_world.x.w = near_point.x;
  light.object_to_world.y.w = near_point.y;
  light.object_to_world.z.w = near_point.z;

  /* Offset in tiles from the scene origin to the center of the first tile-maps. */
  int2 origin_offset = int2(round(float2(near_point) / tile_size));
  /* Offset in tiles between the first and the last tile-maps. */
  int2 offset_vector = int2(round(farthest_tilemap_center / tile_size));

  light.sun.clipmap_base_offset_pos = (offset_vector * (1 << 16)) /
                                      max_ii(levels_range.size() - 1, 1);

  /* \note cascade_level_range starts the range at the unique LOD to apply to all tile-maps. */
  int level = levels_range.first();
  for (int i : IndexRange(levels_range.size())) {
    ShadowTileMap *tilemap = tilemaps_[i];

    /* Equal spacing between cascades layers since we want uniform shadow density. */
    int2 level_offset = origin_offset +
                        shadow_cascade_grid_offset(light.sun.clipmap_base_offset_pos, i);
    tilemap->sync_orthographic(
        object_mat_, level_offset, level, 0.0f, light.pcf_radius, SHADOW_PROJECTION_CASCADE);

    /* Add shadow tile-maps grouped by lights to the GPU buffer. */
    shadows_.tilemap_pool.tilemaps_data.append(*tilemap);
    tilemap->set_updated();
  }

  light.sun.clipmap_origin = float2(origin_offset * tile_size);

  light.type = LIGHT_SUN_ORTHO;

  /* Not really clip-maps, but this is in order to make #light_tilemap_max_get() work and determine
   * the scaling. */
  light.sun.clipmap_lod_min = levels_range.first();
  light.sun.clipmap_lod_max = levels_range.last();

  /* The bias is applied in cascade_level_range().
   * Using clipmap_lod_min here simplify code in shadow_directional_level().
   * Minus 1 because of the ceil(). */
  light.lod_bias = light.sun.clipmap_lod_min - 1;
}

/************************************************************************
 *                         Clip-map Distribution                        *
 ************************************************************************/

IndexRange ShadowDirectional::clipmap_level_range(const Camera &camera)
{
  using namespace blender::math;

  /* 32 to be able to pack offset into two single int2. */
  const int max_tilemap_per_shadows = 32;

  int user_min_level = floorf(log2(min_resolution_));
  /* Covers the farthest points of the view. */
  int max_level = ceil(
      log2(camera.bound_radius() + distance(camera.bound_center(), camera.position())));
  /* We actually need to cover a bit more because of clipmap origin snapping. */
  max_level += 1;
  /* Covers the closest points of the view. */
  int min_level = floor(log2(abs(camera.data_get().clip_near)));
  min_level = clamp_i(user_min_level, min_level, max_level);

  IndexRange range(min_level, max_level - min_level + 1);
  /* The maximum level count is bounded by the mantissa of a 32bit float. Take top-most level to
   * still cover the whole view. */
  range = range.take_back(max_tilemap_per_shadows);

  return range;
}

void ShadowDirectional::clipmap_tilemaps_distribution(Light &light,
                                                      const Camera &camera,
                                                      float lod_bias)
{
  for (int lod : IndexRange(levels_range.size())) {
    ShadowTileMap *tilemap = tilemaps_[lod];

    int level = levels_range.first() + lod;
    /* Compute full offset from world origin to the smallest clipmap tile centered around the
     * camera position. The offset is computed in smallest tile unit. */
    float tile_size = ShadowDirectional::tile_size_get(level);
    /* Moving to light space by multiplying by the transpose (which is the inverse). */
    float2 light_space_camera_position = camera.position() * float2x3(object_mat_.view<2, 3>());
    int2 level_offset = int2(math::round(light_space_camera_position / tile_size));

    tilemap->sync_orthographic(
        object_mat_, level_offset, level, lod_bias, light.pcf_radius, SHADOW_PROJECTION_CLIPMAP);

    /* Add shadow tile-maps grouped by lights to the GPU buffer. */
    shadows_.tilemap_pool.tilemaps_data.append(*tilemap);
    tilemap->set_updated();
  }

  int2 pos_offset = int2(0);
  int2 neg_offset = int2(0);
  for (int lod : IndexRange(levels_range.size() - 1)) {
    /* Since offset can only differ by one tile from the higher level, we can compress that as a
     * single integer where one bit contains offset between 2 levels. Then a single bit shift in
     * the shader gives the number of tile to offset in the given tile-map space. However we need
     * also the sign of the offset for each level offset. To this end, we split the negative
     * offsets to a separate int. */
    int2 lvl_offset_next = tilemaps_[lod + 1]->grid_offset;
    int2 lvl_offset = tilemaps_[lod]->grid_offset;
    int2 lvl_delta = lvl_offset - (lvl_offset_next << 1);
    BLI_assert(math::abs(lvl_delta.x) <= 1 && math::abs(lvl_delta.y) <= 1);
    pos_offset |= math::max(lvl_delta, int2(0)) << lod;
    neg_offset |= math::max(-lvl_delta, int2(0)) << lod;
  }

  /* Number of levels is limited to 32 by `clipmap_level_range()` for this reason. */
  light.sun.clipmap_base_offset_pos = pos_offset;
  light.sun.clipmap_base_offset_neg = neg_offset;

  float tile_size_max = ShadowDirectional::tile_size_get(levels_range.last());
  int2 level_offset_max = tilemaps_[levels_range.size() - 1]->grid_offset;

  light.type = LIGHT_SUN;

  /* Used for selecting the clipmap level. */
  float3 location = camera.position() * float3x3(object_mat_.view<3, 3>());
  light.object_to_world.x.w = location.x;
  light.object_to_world.y.w = location.y;
  light.object_to_world.z.w = location.z;
  /* Used as origin for the clipmap_base_offset trick. */
  light.sun.clipmap_origin = float2(level_offset_max * tile_size_max);

  light.sun.clipmap_lod_min = levels_range.first();
  light.sun.clipmap_lod_max = levels_range.last();

  light.lod_bias = lod_bias;
}

void ShadowDirectional::sync(const float4x4 &object_mat,
                             float min_resolution,
                             float shadow_disk_angle,
                             float trace_distance)
{
  object_mat_ = object_mat;
  /* Clear embedded custom data. */
  object_mat_[0][3] = object_mat_[1][3] = object_mat_[2][3] = 0.0f;
  object_mat_[3][3] = 1.0f;
  /* Remove translation. */
  object_mat_.location() = float3(0.0f);

  min_resolution_ = min_resolution;
  disk_shape_angle_ = min_ff(shadow_disk_angle, DEG2RADF(179.9f)) / 2.0f;
  trace_distance_ = trace_distance;
}

void ShadowDirectional::release_excess_tilemaps(const Camera &camera, float lod_bias)
{
  IndexRange levels_new = directional_distribution_type_get(camera) == SHADOW_PROJECTION_CASCADE ?
                              cascade_level_range(camera, lod_bias) :
                              clipmap_level_range(camera);

  if (levels_range == levels_new) {
    return;
  }

  IndexRange isect_range = levels_range.intersect(levels_new);
  IndexRange before_range(levels_range.start(), isect_range.start() - levels_range.start());
  IndexRange after_range(isect_range.one_after_last(),
                         levels_range.one_after_last() - isect_range.one_after_last());

  auto span = tilemaps_.as_span();
  shadows_.tilemap_pool.release(span.slice(before_range.shift(-levels_range.start())));
  shadows_.tilemap_pool.release(span.slice(after_range.shift(-levels_range.start())));
  tilemaps_ = span.slice(isect_range.shift(-levels_range.start()));
  levels_range = isect_range;
}

void ShadowDirectional::end_sync(Light &light, const Camera &camera, float lod_bias)
{
  ShadowTileMapPool &tilemap_pool = shadows_.tilemap_pool;
  IndexRange levels_new = directional_distribution_type_get(camera) == SHADOW_PROJECTION_CASCADE ?
                              cascade_level_range(camera, lod_bias) :
                              clipmap_level_range(camera);

  if (levels_range != levels_new) {
    /* Acquire missing tile-maps. */
    IndexRange isect_range = levels_new.intersect(levels_range);
    int64_t before_range = isect_range.start() - levels_new.start();
    int64_t after_range = levels_new.one_after_last() - isect_range.one_after_last();

    Vector<ShadowTileMap *> cached_tilemaps = tilemaps_;
    tilemaps_.clear();
    for (int64_t i = 0; i < before_range; i++) {
      tilemaps_.append(tilemap_pool.acquire());
    }
    /* Keep cached LOD's. */
    tilemaps_.extend(cached_tilemaps);
    for (int64_t i = 0; i < after_range; i++) {
      tilemaps_.append(tilemap_pool.acquire());
    }
    levels_range = levels_new;
  }

  light.tilemap_index = tilemap_pool.tilemaps_data.size();
  light.clip_near = 0x7F7FFFFF;                    /* floatBitsToOrderedInt(FLT_MAX) */
  light.clip_far = int(0xFF7FFFFFu ^ 0x7FFFFFFFu); /* floatBitsToOrderedInt(-FLT_MAX) */
  light.sun.shadow_trace_distance = trace_distance_;
  light.sun.shadow_angle = disk_shape_angle_;

  if (directional_distribution_type_get(camera) == SHADOW_PROJECTION_CASCADE) {
    cascade_tilemaps_distribution(light, camera);
  }
  else {
    clipmap_tilemaps_distribution(light, camera, lod_bias);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Module
 *
 * \{ */

ShadowModule::ShadowModule(Instance &inst, ShadowSceneData &data) : inst_(inst), data_(data)
{
  for (int i = 0; i < statistics_buf_.size(); i++) {
    UNUSED_VARS(i);
    statistics_buf_.current().clear_to_zero();
    statistics_buf_.swap();
  }
}

void ShadowModule::init()
{
  /* Determine shadow update technique and atlas format.
   * NOTE(Metal): Metal utilizes a tile-optimized approach for Apple Silicon's architecture. */
  const bool is_metal_backend = (GPU_backend_get_type() == GPU_BACKEND_METAL);
  const bool is_tile_based_arch = (GPU_platform_architecture() == GPU_ARCHITECTURE_TBDR);
  if (is_metal_backend && is_tile_based_arch) {
    ShadowModule::shadow_technique = ShadowTechnique::TILE_COPY;
  }
  else {
    ShadowModule::shadow_technique = ShadowTechnique::ATOMIC_RASTER;
  }

  ::Scene &scene = *inst_.scene;
  bool enabled = (scene.eevee.flag & SCE_EEVEE_SHADOW_ENABLED) != 0;
  if (assign_if_different(enabled_, enabled)) {
    /* Force light reset. */
    for (Light &light : inst_.lights.light_map_.values()) {
      light.initialized = false;
    }
  }

  jittered_transparency_ = !inst_.is_viewport() ||
                           scene.eevee.flag & SCE_EEVEE_SHADOW_JITTERED_VIEWPORT;

  data_.ray_count = clamp_i(scene.eevee.shadow_ray_count, 1, SHADOW_MAX_RAY);
  data_.step_count = clamp_i(scene.eevee.shadow_step_count, 1, SHADOW_MAX_STEP);

  /* Pool size is in MBytes. */
  const size_t pool_byte_size = enabled_ ? scene.eevee.shadow_pool_size * square_i(1024) : 1;
  const size_t page_byte_size = square_i(shadow_page_size_) * sizeof(int);
  shadow_page_len_ = int(divide_ceil_ul(pool_byte_size, page_byte_size));
  shadow_page_len_ = min_ii(shadow_page_len_, SHADOW_MAX_PAGE);

  lod_bias_ = -log2f(scene.eevee.shadow_resolution_scale);

  const int2 atlas_extent = shadow_page_size_ * int2(SHADOW_PAGE_PER_ROW);
  const int atlas_layers = divide_ceil_u(shadow_page_len_, SHADOW_PAGE_PER_LAYER);

  eGPUTextureUsage tex_usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
  if (ShadowModule::shadow_technique == ShadowTechnique::ATOMIC_RASTER) {
    tex_usage |= GPU_TEXTURE_USAGE_ATOMIC;
  }
  if (atlas_tx_.ensure_2d_array(atlas_type, atlas_extent, atlas_layers, tex_usage)) {
    /* Global update. */
    do_full_update = true;
  }

  /* Make allocation safe. Avoids crash later on. */
  if (!atlas_tx_.is_valid()) {
    atlas_tx_.ensure_2d_array(ShadowModule::atlas_type, int2(1), 1);
    inst_.info += "Error: Could not allocate shadow atlas. Most likely out of GPU memory.\n";
  }

  /* Read end of the swap-chain to avoid stall. */
  {
    if (inst_.sampling.finished_viewport()) {
      /* Swap enough to read the last one. */
      for (int i = 0; i < statistics_buf_.size(); i++) {
        statistics_buf_.swap();
      }
    }
    else {
      statistics_buf_.swap();
    }
    statistics_buf_.current().read();
    ShadowStatistics stats = statistics_buf_.current();

    if (stats.page_used_count > shadow_page_len_ && enabled_) {
      std::stringstream ss;
      ss << "Error: Shadow buffer full, may result in missing shadows and lower performance. ("
         << stats.page_used_count << " / " << shadow_page_len_ << ")\n";
      inst_.info += ss.str();
    }
    if (stats.view_needed_count > SHADOW_VIEW_MAX && enabled_) {
      std::stringstream ss;
      ss << "Error: Too many shadow updates, some shadow might be incorrect.\n";
      inst_.info += ss.str();
    }
  }

  atlas_tx_.filter_mode(false);

  /* Create different viewport to support different update region size. The most fitting viewport
   * is then selected during the tilemap finalize stage in `viewport_select`. */
  for (int i = 0; i < multi_viewports_.size(); i++) {
    /** IMPORTANT: Reflect changes in TBDR tile vertex shader which assumes viewport index 15
     * covers the whole framebuffer. */
    int size_in_tile = min_ii(1 << i, SHADOW_TILEMAP_RES);
    multi_viewports_[i][0] = 0;
    multi_viewports_[i][1] = 0;
    multi_viewports_[i][2] = size_in_tile * shadow_page_size_;
    multi_viewports_[i][3] = size_in_tile * shadow_page_size_;
  }
}

void ShadowModule::begin_sync()
{
  past_casters_updated_.clear();
  curr_casters_updated_.clear();
  curr_casters_.clear();
  jittered_transparent_casters_.clear();
  update_casters_ = true;

  {
    Manager &manager = *inst_.manager;

    PassMain &pass = tilemap_usage_ps_;
    pass.init();

    if (inst_.is_baking()) {
      SurfelBuf &surfels_buf = inst_.volume_probes.bake.surfels_buf_;
      CaptureInfoBuf &capture_info_buf = inst_.volume_probes.bake.capture_info_buf_;
      float surfel_coverage_area = inst_.volume_probes.bake.surfel_density_;

      /* Directional shadows. */
      float texel_size = ShadowDirectional::tile_size_get(0) / float(SHADOW_PAGE_RES);
      int directional_level = std::max(0, int(std::ceil(log2(surfel_coverage_area / texel_size))));

      PassMain::Sub &sub = pass.sub("Surfels");
      sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_USAGE_SURFELS));
      sub.bind_ssbo("tilemaps_buf", &tilemap_pool.tilemaps_data);
      sub.bind_ssbo("tiles_buf", &tilemap_pool.tiles_data);
      sub.bind_ssbo("surfel_buf", &surfels_buf);
      sub.bind_ssbo("capture_info_buf", &capture_info_buf);
      sub.push_constant("directional_level", directional_level);
      sub.bind_resources(inst_.uniform_data);
      sub.bind_resources(inst_.lights);
      sub.dispatch(&inst_.volume_probes.bake.dispatch_per_surfel_);

      /* Skip opaque and transparent tagging for light baking. */
      return;
    }

    {
      /* Use depth buffer to tag needed shadow pages for opaque geometry. */
      PassMain::Sub &sub = pass.sub("Opaque");
      sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_USAGE_OPAQUE));
      sub.bind_ssbo("tilemaps_buf", &tilemap_pool.tilemaps_data);
      sub.bind_ssbo("tiles_buf", &tilemap_pool.tiles_data);
      sub.bind_texture("depth_tx", &src_depth_tx_);
      sub.push_constant("input_depth_extent", &input_depth_extent_);
      sub.bind_resources(inst_.lights);
      sub.bind_resources(inst_.uniform_data);
      sub.bind_resources(inst_.hiz_buffer.front);
      sub.dispatch(&dispatch_depth_scan_size_);
    }
    {
      /* Use bounding boxes for transparent geometry. */
      PassMain::Sub &sub = pass.sub("Transparent");
      /* WORKAROUND: The DRW_STATE_WRITE_STENCIL is here only to avoid enabling the rasterizer
       * discard inside draw manager. */
      sub.state_set(DRW_STATE_CULL_FRONT | DRW_STATE_WRITE_STENCIL);
      sub.state_stencil(0, 0, 0);
      sub.framebuffer_set(&usage_tag_fb);
      sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_USAGE_TRANSPARENT));
      sub.bind_ssbo("tilemaps_buf", &tilemap_pool.tilemaps_data);
      sub.bind_ssbo("tiles_buf", &tilemap_pool.tiles_data);
      sub.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
      sub.push_constant("fb_resolution", &usage_tag_fb_resolution_);
      sub.push_constant("fb_lod", &usage_tag_fb_lod_);
      sub.bind_resources(inst_.uniform_data);
      sub.bind_resources(inst_.hiz_buffer.front);
      sub.bind_resources(inst_.lights);

      box_batch_ = DRW_cache_cube_get();
      tilemap_usage_transparent_ps_ = &sub;
    }
  }
}

void ShadowModule::sync_object(const Object *ob,
                               const ObjectHandle &handle,
                               const ResourceHandle &resource_handle,
                               bool is_alpha_blend,
                               bool has_transparent_shadows)
{
  bool is_shadow_caster = !(ob->visibility_flag & OB_HIDE_SHADOW);
  if (!is_shadow_caster && !is_alpha_blend) {
    return;
  }

  ShadowObject &shadow_ob = objects_.lookup_or_add_default(handle.object_key);
  shadow_ob.used = true;
  const bool is_initialized = shadow_ob.resource_handle.raw != 0;
  const bool has_jittered_transparency = has_transparent_shadows && jittered_transparency_;
  if (is_shadow_caster && (handle.recalc || !is_initialized || has_jittered_transparency)) {
    if (handle.recalc && is_initialized) {
      past_casters_updated_.append(shadow_ob.resource_handle.raw);
    }

    if (has_jittered_transparency) {
      jittered_transparent_casters_.append(resource_handle.raw);
    }
    else {
      curr_casters_updated_.append(resource_handle.raw);
    }
  }
  shadow_ob.resource_handle = resource_handle;

  if (is_shadow_caster) {
    curr_casters_.append(resource_handle.raw);
  }

  if (is_alpha_blend && !inst_.is_baking()) {
    tilemap_usage_transparent_ps_->draw(box_batch_, resource_handle);
  }
}

void ShadowModule::end_sync()
{
  /* Delete unused shadows first to release tile-maps that could be reused for new lights. */
  for (Light &light : inst_.lights.light_map_.values()) {
    if (!light.used || !enabled_) {
      light.shadow_discard_safe(*this);
    }
    else if (light.directional != nullptr) {
      light.directional->release_excess_tilemaps(inst_.camera, light.lod_bias);
    }
    else if (light.punctual != nullptr) {
      light.punctual->release_excess_tilemaps();
    }
  }

  /* Allocate new tile-maps and fill shadow data of the lights. */
  tilemap_pool.tilemaps_data.clear();
  for (Light &light : inst_.lights.light_map_.values()) {
    if (enabled_ == false) {
      light.tilemap_index = LIGHT_NO_SHADOW;
    }
    else if (light.directional != nullptr) {
      light.directional->end_sync(light, inst_.camera, light.lod_bias);
    }
    else if (light.punctual != nullptr) {
      light.punctual->end_sync(light, light.lod_bias);
    }
    else {
      light.tilemap_index = LIGHT_NO_SHADOW;
    }
  }
  tilemap_pool.end_sync(*this);

  /* Search for deleted or updated shadow casters */
  auto it_end = objects_.items().end();
  for (auto it = objects_.items().begin(); it != it_end; ++it) {
    ShadowObject &shadow_ob = (*it).value;
    if (!shadow_ob.used) {
      /* May not be a caster, but it does not matter, be conservative. */
      past_casters_updated_.append(shadow_ob.resource_handle.raw);
      objects_.remove(it);
    }
    else {
      /* Clear for next sync. */
      shadow_ob.used = false;
    }
  }
  past_casters_updated_.push_update();
  curr_casters_updated_.push_update();
  jittered_transparent_casters_.push_update();

  curr_casters_.push_update();

  if (do_full_update) {
    do_full_update = false;
    /* Put all pages in the free heap. */
    for (uint i : IndexRange(shadow_page_len_)) {
      uint3 page = {i % SHADOW_PAGE_PER_ROW,
                    (i / SHADOW_PAGE_PER_ROW) % SHADOW_PAGE_PER_COL,
                    i / SHADOW_PAGE_PER_LAYER};
      pages_free_data_[i] = shadow_page_pack(page);
    }
    for (uint i : IndexRange(shadow_page_len_, SHADOW_MAX_PAGE - shadow_page_len_)) {
      pages_free_data_[i] = 0xFFFFFFFFu;
    }
    pages_free_data_.push_update();

    /* Clear tiles to not reference any page. */
    tilemap_pool.tiles_data.clear_to_zero();
    tilemap_pool.tilemaps_clip.clear_to_zero();

    /* Clear cached page buffer. */
    GPU_storagebuf_clear(pages_cached_data_, -1);

    /* Reset info to match new state. */
    pages_infos_data_.page_free_count = shadow_page_len_;
    pages_infos_data_.page_alloc_count = 0;
    pages_infos_data_.page_cached_next = 0u;
    pages_infos_data_.page_cached_start = 0u;
    pages_infos_data_.page_cached_end = 0u;
    pages_infos_data_.push_update();
  }

  {
    Manager &manager = *inst_.manager;

    {
      PassSimple &pass = tilemap_setup_ps_;
      pass.init();

      {
        /* Clear tile-map clip buffer. */
        PassSimple::Sub &sub = pass.sub("ClearClipmap");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_CLIPMAP_CLEAR));
        sub.bind_ssbo("tilemaps_clip_buf", tilemap_pool.tilemaps_clip);
        sub.push_constant("tilemaps_clip_buf_len", int(tilemap_pool.tilemaps_clip.size()));
        sub.dispatch(int3(
            divide_ceil_u(tilemap_pool.tilemaps_clip.size(), SHADOW_CLIPMAP_GROUP_SIZE), 1, 1));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }

      {
        /* Compute near/far clip distances for directional shadows based on casters bounds. */
        PassSimple::Sub &sub = pass.sub("DirectionalBounds");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_BOUNDS));
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tilemaps_clip_buf", tilemap_pool.tilemaps_clip);
        sub.bind_ssbo("casters_id_buf", curr_casters_);
        sub.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
        sub.push_constant("resource_len", int(curr_casters_.size()));
        sub.bind_resources(inst_.lights);
        sub.dispatch(int3(
            divide_ceil_u(std::max(curr_casters_.size(), int64_t(1)), SHADOW_BOUNDS_GROUP_SIZE),
            1,
            1));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
      {
        /* Clear usage bits. Tag update from the tile-map for sun shadow clip-maps shifting. */
        PassSimple::Sub &sub = pass.sub("Init");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_INIT));
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tilemaps_clip_buf", tilemap_pool.tilemaps_clip);
        sub.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        sub.bind_ssbo("pages_cached_buf", pages_cached_data_);
        sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_data.size()));
        /* Free unused tiles from tile-maps not used by any shadow. */
        if (tilemap_pool.tilemaps_unused.size() > 0) {
          sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_unused);
          sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_unused.size()));
        }
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
    }

    {
      /* Mark for update all shadow pages touching an updated shadow caster. */
      PassSimple &pass = caster_update_ps_;
      pass.init();
      pass.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_UPDATE));
      pass.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
      pass.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
      /* Past caster transforms. */
      if (past_casters_updated_.size() > 0) {
        pass.bind_ssbo("bounds_buf", &manager.bounds_buf.previous());
        pass.bind_ssbo("resource_ids_buf", past_casters_updated_);
        pass.dispatch(int3(past_casters_updated_.size(), 1, tilemap_pool.tilemaps_data.size()));
      }
      /* Current caster transforms. */
      if (curr_casters_updated_.size() > 0) {
        pass.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
        pass.bind_ssbo("resource_ids_buf", curr_casters_updated_);
        pass.dispatch(int3(curr_casters_updated_.size(), 1, tilemap_pool.tilemaps_data.size()));
      }
      pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    }

    {
      /* Mark for update all shadow pages touching a jittered transparency shadow caster. */
      PassSimple &pass = jittered_transparent_caster_update_ps_;
      pass.init();
      if (jittered_transparent_casters_.size() > 0) {
        pass.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_UPDATE));
        pass.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        pass.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        pass.bind_ssbo("bounds_buf", &manager.bounds_buf.current());
        pass.bind_ssbo("resource_ids_buf", jittered_transparent_casters_);
        pass.dispatch(
            int3(jittered_transparent_casters_.size(), 1, tilemap_pool.tilemaps_data.size()));
        pass.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
    }

    /* Non volume usage tagging happens between these two steps.
     * (Setup at begin_sync) */

    if (inst_.volume.needs_shadow_tagging() && !inst_.is_baking()) {
      PassMain::Sub &sub = tilemap_usage_ps_.sub("World Volume");
      sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_TAG_USAGE_VOLUME));
      sub.bind_ssbo("tilemaps_buf", &tilemap_pool.tilemaps_data);
      sub.bind_ssbo("tiles_buf", &tilemap_pool.tiles_data);
      sub.bind_resources(inst_.uniform_data);
      sub.bind_resources(inst_.hiz_buffer.front);
      sub.bind_resources(inst_.sampling);
      sub.bind_resources(inst_.lights);
      sub.bind_resources(inst_.volume.properties);
      sub.bind_resources(inst_.volume.result);
      sub.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
      sub.dispatch(math::divide_ceil(inst_.volume.grid_size(), int3(VOLUME_GROUP_SIZE)));
    }

    {
      PassSimple &pass = tilemap_update_ps_;
      pass.init();
      {
        /* Mark tiles that are redundant in the mipmap chain as unused. */
        PassSimple::Sub &sub = pass.sub("MaskLod");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_MASK));
        sub.push_constant("max_view_per_tilemap", &max_view_per_tilemap_);
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_data.size()));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
      {
        /* Free unused pages & Reclaim cached pages. */
        PassSimple::Sub &sub = pass.sub("Free");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_FREE));
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        sub.bind_ssbo("pages_infos_buf", pages_infos_data_);
        sub.bind_ssbo("pages_free_buf", pages_free_data_);
        sub.bind_ssbo("pages_cached_buf", pages_cached_data_);
        sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_data.size()));
        /* Free unused tiles from tile-maps not used by any shadow. */
        if (tilemap_pool.tilemaps_unused.size() > 0) {
          sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_unused);
          sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_unused.size()));
        }
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
      {
        /* De-fragment the free page heap after cache reuse phase which can leave hole. */
        PassSimple::Sub &sub = pass.sub("Defrag");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_DEFRAG));
        sub.bind_ssbo("pages_infos_buf", pages_infos_data_);
        sub.bind_ssbo("pages_free_buf", pages_free_data_);
        sub.bind_ssbo("pages_cached_buf", pages_cached_data_);
        sub.bind_ssbo("statistics_buf", statistics_buf_.current());
        sub.bind_ssbo("clear_dispatch_buf", clear_dispatch_buf_);
        sub.bind_ssbo("tile_draw_buf", tile_draw_buf_);
        sub.dispatch(int3(1, 1, 1));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
      {
        /* Assign pages to tiles that have been marked as used but possess no page. */
        PassSimple::Sub &sub = pass.sub("AllocatePages");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_ALLOCATE));
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        sub.bind_ssbo("statistics_buf", statistics_buf_.current());
        sub.bind_ssbo("pages_infos_buf", pages_infos_data_);
        sub.bind_ssbo("pages_free_buf", pages_free_data_);
        sub.bind_ssbo("pages_cached_buf", pages_cached_data_);
        sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_data.size()));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
      }
      {
        /* Convert the unordered tiles into a texture used during shading. Creates views. */
        PassSimple::Sub &sub = pass.sub("Finalize");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_FINALIZE));
        sub.bind_ssbo("tilemaps_buf", tilemap_pool.tilemaps_data);
        sub.bind_ssbo("tilemaps_clip_buf", tilemap_pool.tilemaps_clip);
        sub.bind_ssbo("tiles_buf", tilemap_pool.tiles_data);
        sub.bind_ssbo("view_infos_buf", &shadow_multi_view_.matrices_ubo_get());
        sub.bind_ssbo("statistics_buf", statistics_buf_.current());
        sub.bind_ssbo("clear_dispatch_buf", clear_dispatch_buf_);
        sub.bind_ssbo("tile_draw_buf", tile_draw_buf_);
        sub.bind_ssbo("dst_coord_buf", dst_coord_buf_);
        sub.bind_ssbo("src_coord_buf", src_coord_buf_);
        sub.bind_ssbo("render_map_buf", render_map_buf_);
        sub.bind_ssbo("render_view_buf", render_view_buf_);
        sub.bind_ssbo("pages_infos_buf", pages_infos_data_);
        sub.bind_image("tilemaps_img", tilemap_pool.tilemap_tx);
        sub.dispatch(int3(1, 1, tilemap_pool.tilemaps_data.size()));
        sub.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_UNIFORM | GPU_BARRIER_TEXTURE_FETCH |
                    GPU_BARRIER_SHADER_IMAGE_ACCESS);
      }
      {
        /* Amend tilemap_tx content to support clipmap LODs. */
        PassSimple::Sub &sub = pass.sub("Amend");
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_TILEMAP_AMEND));
        sub.bind_image("tilemaps_img", tilemap_pool.tilemap_tx);
        sub.bind_resources(inst_.lights);
        sub.dispatch(int3(1));
        sub.barrier(GPU_BARRIER_TEXTURE_FETCH);
      }

      /* NOTE: We do not need to run the clear pass when using the TBDR update variant, as tiles
       * will be fully cleared as part of the shadow raster step. */
      if (ShadowModule::shadow_technique != ShadowTechnique::TILE_COPY) {
        /** Clear pages that need to be rendered. */
        PassSimple::Sub &sub = pass.sub("RenderClear");
        sub.framebuffer_set(&render_fb_);
        sub.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
        sub.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_CLEAR));
        sub.bind_ssbo("pages_infos_buf", pages_infos_data_);
        sub.bind_ssbo("dst_coord_buf", dst_coord_buf_);
        sub.bind_image("shadow_atlas_img", atlas_tx_);
        sub.dispatch(clear_dispatch_buf_);
        sub.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
      }
    }
  }

  debug_end_sync();
}

void ShadowModule::debug_end_sync()
{
  if (!ELEM(inst_.debug_mode,
            eDebugMode::DEBUG_SHADOW_TILEMAPS,
            eDebugMode::DEBUG_SHADOW_VALUES,
            eDebugMode::DEBUG_SHADOW_TILE_RANDOM_COLOR,
            eDebugMode::DEBUG_SHADOW_TILEMAP_RANDOM_COLOR))
  {
    return;
  }

  /* Init but not filled if no active object. */
  debug_draw_ps_.init();

  Object *object_active = DRW_context_state_get()->obact;
  if (object_active == nullptr) {
    return;
  }

  ObjectKey object_key(DEG_get_original_object(object_active));

  if (inst_.lights.light_map_.contains(object_key) == false) {
    return;
  }

  Light &light = inst_.lights.light_map_.lookup(object_key);

  if (light.tilemap_index >= SHADOW_MAX_TILEMAP) {
    return;
  }

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_BLEND_CUSTOM;

  debug_draw_ps_.state_set(state);
  debug_draw_ps_.shader_set(inst_.shaders.static_shader_get(SHADOW_DEBUG));
  debug_draw_ps_.push_constant("debug_mode", int(inst_.debug_mode));
  debug_draw_ps_.push_constant("debug_tilemap_index", light.tilemap_index);
  debug_draw_ps_.bind_ssbo("tilemaps_buf", &tilemap_pool.tilemaps_data);
  debug_draw_ps_.bind_ssbo("tiles_buf", &tilemap_pool.tiles_data);
  debug_draw_ps_.bind_resources(inst_.uniform_data);
  debug_draw_ps_.bind_resources(inst_.hiz_buffer.front);
  debug_draw_ps_.bind_resources(inst_.lights);
  debug_draw_ps_.bind_resources(inst_.shadows);
  debug_draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

/* Compute approximate screen pixel density (as world space radius). */
float ShadowModule::screen_pixel_radius(const View &view, const int2 &extent)
{
  float min_dim = float(min_ii(extent.x, extent.y));
  float3 p0 = float3(-1.0f, -1.0f, 0.0f);
  float3 p1 = float3(float2(min_dim / extent) * 2.0f - 1.0f, 0.0f);
  mul_project_m4_v3(view.wininv().ptr(), p0);
  mul_project_m4_v3(view.wininv().ptr(), p1);
  /* Compute radius at unit plane from the camera. This is NOT the perspective division. */
  if (view.is_persp()) {
    p0 = p0 / p0.z;
    p1 = p1 / p1.z;
  }
  return math::distance(p0, p1) / min_dim;
}

bool ShadowModule::shadow_update_finished()
{
  if (!inst_.is_image_render()) {
    /* For viewport, only run the shadow update once per redraw.
     * This avoids the stall from the read-back and freezes from long shadow update. */
    return true;
  }

  int max_updated_view_count = tilemap_pool.tilemaps_data.size() * SHADOW_TILEMAP_LOD;
  if (max_updated_view_count <= SHADOW_VIEW_MAX) {
    /* There is enough shadow views to cover all tile-map updates.
     * No read-back needed as it is guaranteed that all of them will be updated. */
    return true;
  }

  /* Read back and check if there is still tile-map to update. */
  statistics_buf_.current().async_flush_to_host();
  statistics_buf_.current().read();
  ShadowStatistics stats = statistics_buf_.current();
  /* Rendering is finished if we rendered all the remaining pages. */
  return stats.page_rendered_count == stats.page_update_count;
}

int ShadowModule::max_view_per_tilemap()
{
  if (inst_.is_image_render()) {
    /* No need to limit updates per lights as we ensure all lights levels will be rendered.
     * is_image_render. */
    return SHADOW_TILEMAP_LOD;
  }
  /* For now very simple heuristic. Can be improved later by taking into consideration how many
   * tilemaps are updating, but we cannot know the ones updated by casters. */
  int potential_view_count = 0;
  for (auto i : IndexRange(tilemap_pool.tilemaps_data.size())) {
    if (tilemap_pool.tilemaps_data[i].projection_type == SHADOW_PROJECTION_CUBEFACE) {
      potential_view_count += SHADOW_TILEMAP_LOD;
    }
    else {
      potential_view_count += 1;
    }
  }
  int max_view_count = divide_ceil_u(SHADOW_VIEW_MAX, math::max(potential_view_count, 1));
  /* For viewport interactivity, have a hard maximum. This allows smoother experience. */
  if (inst_.is_transforming() || inst_.is_navigating()) {
    max_view_count = math::min(2, max_view_count);
  }
  /* For animation playback, we always want the maximum performance. */
  if (inst_.is_playback()) {
    max_view_count = math::min(1, max_view_count);
  }

  return max_view_count;
}

void ShadowModule::set_view(View &view, int2 extent)
{
  if (enabled_ == false) {
    /* All lights have been tagged to have no shadow. */
    return;
  }

  input_depth_extent_ = extent;

  GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();

  dispatch_depth_scan_size_ = int3(math::divide_ceil(extent, int2(SHADOW_DEPTH_SCAN_GROUP_SIZE)),
                                   1);
  max_view_per_tilemap_ = max_view_per_tilemap();

  data_.film_pixel_radius = screen_pixel_radius(view, extent);
  inst_.uniform_data.push_update();

  usage_tag_fb_resolution_ = math::divide_ceil(extent, int2(std::exp2(usage_tag_fb_lod_)));
  usage_tag_fb.ensure(usage_tag_fb_resolution_);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_MEMORYLESS;
  int2 fb_size = int2(SHADOW_TILEMAP_RES * shadow_page_size_);
  int fb_layers = SHADOW_VIEW_MAX;

  if (shadow_technique == ShadowTechnique::ATOMIC_RASTER) {
    /* Create attachment-less framebuffer. */
    shadow_depth_fb_tx_.free();
    shadow_depth_accum_tx_.free();
    render_fb_.ensure(fb_size);
  }
  else if (shadow_technique == ShadowTechnique::TILE_COPY) {
    /* Create memoryless depth attachment for on-tile surface depth accumulation. */
    shadow_depth_fb_tx_.ensure_2d_array(GPU_DEPTH_COMPONENT32F, fb_size, fb_layers, usage);
    shadow_depth_accum_tx_.ensure_2d_array(GPU_R32F, fb_size, fb_layers, usage);
    render_fb_.ensure(GPU_ATTACHMENT_TEXTURE(shadow_depth_fb_tx_),
                      GPU_ATTACHMENT_TEXTURE(shadow_depth_accum_tx_));
  }
  else {
    BLI_assert_unreachable();
  }

  inst_.hiz_buffer.update();
  bool first_loop = true;

  do {
    DRW_stats_group_start("Shadow");
    {
      GPU_uniformbuf_clear_to_zero(shadow_multi_view_.matrices_ubo_get());

      inst_.manager->submit(tilemap_setup_ps_, view);
      if (assign_if_different(update_casters_, false)) {
        /* Run caster update only once. */
        /* TODO(fclem): There is an optimization opportunity here where we can
         * test casters only against the static tilemaps instead of all of them. */
        inst_.manager->submit(caster_update_ps_, view);
      }
      if (assign_if_different(first_loop, false)) {
        inst_.manager->submit(jittered_transparent_caster_update_ps_, view);
      }
      inst_.manager->submit(tilemap_usage_ps_, view);
      inst_.manager->submit(tilemap_update_ps_, view);

      shadow_multi_view_.compute_procedural_bounds();

      statistics_buf_.current().async_flush_to_host();

      /* Isolate shadow update into its own command buffer.
       * If parameter buffer exceeds limits, then other work will not be impacted. */
      bool use_flush = (shadow_technique == ShadowTechnique::TILE_COPY) &&
                       (GPU_backend_get_type() == GPU_BACKEND_METAL);

      if (use_flush) {
        GPU_flush();
      }

      /* TODO(fclem): Move all of this to the draw::PassMain. */
      if (shadow_depth_fb_tx_.is_valid() && shadow_depth_accum_tx_.is_valid()) {
        GPU_framebuffer_bind_ex(
            render_fb_,
            {
                /* Depth is cleared to 0 for TBDR optimization. */
                {GPU_LOADACTION_CLEAR, GPU_STOREACTION_DONT_CARE, {0.0f, 0.0f, 0.0f, 0.0f}},
                {GPU_LOADACTION_CLEAR, GPU_STOREACTION_DONT_CARE, {1.0f, 1.0f, 1.0f, 1.0f}},
            });
      }
      else if (shadow_depth_fb_tx_.is_valid()) {
        GPU_framebuffer_bind_ex(
            render_fb_,
            {
                {GPU_LOADACTION_CLEAR, GPU_STOREACTION_DONT_CARE, {1.0f, 1.0f, 1.0f, 1.0f}},
            });
      }
      else {
        GPU_framebuffer_bind(render_fb_);
      }

      GPU_framebuffer_multi_viewports_set(render_fb_,
                                          reinterpret_cast<int(*)[4]>(multi_viewports_.data()));

      inst_.pipelines.shadow.render(shadow_multi_view_);

      if (use_flush) {
        GPU_flush();
      }

      GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
    }
    DRW_stats_group_end();
  } while (!shadow_update_finished());

  if (prev_fb) {
    GPU_framebuffer_bind(prev_fb);
  }
}

void ShadowModule::debug_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (!ELEM(inst_.debug_mode,
            eDebugMode::DEBUG_SHADOW_TILEMAPS,
            eDebugMode::DEBUG_SHADOW_VALUES,
            eDebugMode::DEBUG_SHADOW_TILE_RANDOM_COLOR,
            eDebugMode::DEBUG_SHADOW_TILEMAP_RANDOM_COLOR))
  {
    return;
  }

  switch (inst_.debug_mode) {
    case DEBUG_SHADOW_TILEMAPS:
      inst_.info += "Debug Mode: Shadow Tilemap\n";
      break;
    case DEBUG_SHADOW_VALUES:
      inst_.info += "Debug Mode: Shadow Values\n";
      break;
    case DEBUG_SHADOW_TILE_RANDOM_COLOR:
      inst_.info += "Debug Mode: Shadow Tile Random Color\n";
      break;
    case DEBUG_SHADOW_TILEMAP_RANDOM_COLOR:
      inst_.info += "Debug Mode: Shadow Tilemap Random Color\n";
      break;
    default:
      break;
  }

  inst_.hiz_buffer.update();

  GPU_framebuffer_bind(view_fb);
  inst_.manager->submit(debug_draw_ps_, view);
}

/** \} */

}  // namespace blender::eevee
