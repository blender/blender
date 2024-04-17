/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Module that handles light probe update tagging.
 * Lighting data is contained in their respective module `VolumeProbeModule`, `SphereProbeModule`
 * and `PlanarProbeModule`.
 */

#include "DNA_lightprobe_types.h"
#include "WM_api.hh"

#include "eevee_instance.hh"
#include "eevee_lightprobe.hh"

#include "draw_debug.hh"

#include <iostream>

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Light-Probe Module
 * \{ */

LightProbeModule::LightProbeModule(Instance &inst) : inst_(inst)
{
  /* Initialize the world probe. */
  world_sphere_.clipping_distances = float2(1.0f, 10.0f);
  world_sphere_.world_to_probe_transposed = float3x4::identity();
  world_sphere_.influence_shape = SHAPE_ELIPSOID;
  world_sphere_.parallax_shape = SHAPE_ELIPSOID;
  /* Full influence. */
  world_sphere_.influence_scale = 0.0f;
  world_sphere_.influence_bias = 1.0f;
  world_sphere_.parallax_distance = 1e10f;
  /* In any case, the world must always be up to valid and used for render. */
  world_sphere_.use_for_render = true;
}

static eLightProbeResolution resolution_to_probe_resolution_enum(int resolution)
{
  switch (resolution) {
    case 64:
      return LIGHT_PROBE_RESOLUTION_64;
    case 128:
      return LIGHT_PROBE_RESOLUTION_128;
    case 256:
      return LIGHT_PROBE_RESOLUTION_256;
    case 512:
      return LIGHT_PROBE_RESOLUTION_512;
    case 1024:
      return LIGHT_PROBE_RESOLUTION_1024;
    default:
      /* Default to maximum resolution because the old max was 4K for Legacy-EEVEE. */
    case 2048:
      return LIGHT_PROBE_RESOLUTION_2048;
  }
}

void LightProbeModule::init()
{
  const SceneEEVEE &sce_eevee = inst_.scene->eevee;
  sphere_object_resolution_ = resolution_to_probe_resolution_enum(sce_eevee.gi_cubemap_resolution);
}

void LightProbeModule::begin_sync()
{
  auto_bake_enabled_ = inst_.is_viewport() &&
                       (inst_.scene->eevee.flag & SCE_EEVEE_GI_AUTOBAKE) != 0;
}

void LightProbeModule::sync_volume(const Object *ob, ObjectHandle &handle)
{
  VolumeProbe &grid = volume_map_.lookup_or_add_default(handle.object_key);
  grid.used = true;
  if (handle.recalc != 0 || grid.initialized == false) {
    const ::LightProbe *lightprobe = static_cast<const ::LightProbe *>(ob->data);

    grid.initialized = true;
    grid.updated = true;
    grid.surfel_density = static_cast<const ::LightProbe *>(ob->data)->surfel_density;
    grid.object_to_world = ob->object_to_world();
    grid.cache = ob->lightprobe_cache;

    /* Needed for display. */
    // LightProbeGridCacheFrame *cache = grid.cache ? grid.cache->grid_static_cache : nullptr;
    // if (cache != nullptr) {
    //   int3 grid_size = int3(cache->size);
    //   /* Offset sample placement so that border texels are on the edges of the volume. */
    //   float3 grid_scale = float3(grid_size) / float3(grid_size + 1);
    //   grid.object_to_world *= math::from_scale<float4x4>(grid_scale);
    // }
    grid.world_to_object = float4x4(
        math::normalize(math::transpose(float3x3(grid.object_to_world))));

    grid.normal_bias = lightprobe->grid_normal_bias;
    grid.view_bias = lightprobe->grid_view_bias;
    grid.facing_bias = lightprobe->grid_facing_bias;

    grid.validity_threshold = lightprobe->grid_validity_threshold;
    grid.dilation_threshold = lightprobe->grid_dilation_threshold;
    grid.dilation_radius = lightprobe->grid_dilation_radius;
    grid.intensity = lightprobe->intensity;

    grid.viewport_display = lightprobe->flag & LIGHTPROBE_FLAG_SHOW_DATA;
    grid.viewport_display_size = lightprobe->data_display_size;

    /* Force reupload. */
    inst_.volume_probes.bricks_free(grid.bricks);
  }
}

void LightProbeModule::sync_sphere(const Object *ob, ObjectHandle &handle)
{
  SphereProbe &cube = sphere_map_.lookup_or_add_default(handle.object_key);
  cube.used = true;
  if (handle.recalc != 0 || cube.initialized == false) {
    const ::LightProbe &light_probe = *(::LightProbe *)ob->data;

    cube.initialized = true;
    cube.updated = true;
    cube.do_render = true;

    SphereProbeModule &probe_module = inst_.sphere_probes;
    eLightProbeResolution probe_resolution = sphere_object_resolution_;
    int subdivision_lvl = probe_module.subdivision_level_get(probe_resolution);

    if (cube.atlas_coord.subdivision_lvl != subdivision_lvl) {
      cube.atlas_coord.free();
      cube.atlas_coord = find_empty_atlas_region(subdivision_lvl);
      SphereProbeData &cube_data = *static_cast<SphereProbeData *>(&cube);
      /* Update gpu data sampling coordinates. */
      cube_data.atlas_coord = cube.atlas_coord.as_sampling_coord();
      /* Coordinates have changed. Area might contain random data. Do not use for rendering. */
      cube.use_for_render = false;
    }

    bool use_custom_parallax = (light_probe.flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0;
    float influence_distance = light_probe.distinf;
    float influence_falloff = light_probe.falloff;
    float parallax_distance = light_probe.distpar;
    parallax_distance = use_custom_parallax ? max_ff(parallax_distance, influence_distance) :
                                              influence_distance;

    auto to_eevee_shape = [](int bl_shape_type) {
      return (bl_shape_type == LIGHTPROBE_SHAPE_BOX) ? SHAPE_CUBOID : SHAPE_ELIPSOID;
    };
    cube.influence_shape = to_eevee_shape(light_probe.attenuation_type);
    cube.parallax_shape = to_eevee_shape(light_probe.parallax_type);

    float4x4 object_to_world = math::scale(ob->object_to_world(), float3(influence_distance));
    cube.location = object_to_world.location();
    cube.volume = math::abs(math::determinant(object_to_world));
    cube.world_to_probe_transposed = float3x4(math::transpose(math::invert(object_to_world)));
    cube.influence_scale = 1.0 / max_ff(1e-8f, influence_falloff);
    cube.influence_bias = cube.influence_scale;
    cube.parallax_distance = parallax_distance / influence_distance;
    cube.clipping_distances = float2(light_probe.clipsta, light_probe.clipend);

    cube.viewport_display = light_probe.flag & LIGHTPROBE_FLAG_SHOW_DATA;
    cube.viewport_display_size = light_probe.data_display_size;
  }
}

void LightProbeModule::sync_planar(const Object *ob, ObjectHandle &handle)
{
  PlanarProbe &plane = planar_map_.lookup_or_add_default(handle.object_key);
  plane.used = true;
  if (handle.recalc != 0 || plane.initialized == false) {
    const ::LightProbe *light_probe = (::LightProbe *)ob->data;

    plane.initialized = true;
    plane.updated = true;
    plane.plane_to_world = ob->object_to_world();
    plane.plane_to_world.z_axis() = math::normalize(plane.plane_to_world.z_axis()) *
                                    light_probe->distinf;
    plane.world_to_plane = math::invert(plane.plane_to_world);
    plane.clipping_offset = light_probe->clipsta;
    plane.viewport_display = (light_probe->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0;
  }
}

void LightProbeModule::sync_probe(const Object *ob, ObjectHandle &handle)
{
  const ::LightProbe *lightprobe = static_cast<const ::LightProbe *>(ob->data);
  switch (lightprobe->type) {
    case LIGHTPROBE_TYPE_SPHERE:
      sync_sphere(ob, handle);
      return;
    case LIGHTPROBE_TYPE_PLANE:
      sync_planar(ob, handle);
      return;
    case LIGHTPROBE_TYPE_VOLUME:
      sync_volume(ob, handle);
      return;
  }
  BLI_assert_unreachable();
}

void LightProbeModule::sync_world(const ::World *world, bool has_update)
{
  const eLightProbeResolution probe_resolution = static_cast<eLightProbeResolution>(
      world->probe_resolution);

  SphereProbeModule &sph_module = inst_.sphere_probes;
  int subdivision_lvl = sph_module.subdivision_level_get(probe_resolution);

  if (subdivision_lvl != world_sphere_.atlas_coord.subdivision_lvl) {
    world_sphere_.atlas_coord.free();
    world_sphere_.atlas_coord = find_empty_atlas_region(subdivision_lvl);
    SphereProbeData &world_data = *static_cast<SphereProbeData *>(&world_sphere_);
    world_data.atlas_coord = world_sphere_.atlas_coord.as_sampling_coord();
    has_update = true;
  }

  if (has_update) {
    world_sphere_.do_render = true;
  }
}

void LightProbeModule::end_sync()
{
  /* Check for deleted or updated grid. */
  volume_update_ = false;
  volume_map_.remove_if([&](const Map<ObjectKey, VolumeProbe>::MutableItem &item) {
    VolumeProbe &grid = item.value;
    bool remove_grid = !grid.used;
    if (grid.updated || remove_grid) {
      volume_update_ = true;
    }
    grid.updated = false;
    grid.used = false;
    return remove_grid;
  });

  /* Check for deleted or updated cube. */
  sphere_update_ = false;
  sphere_map_.remove_if([&](const Map<ObjectKey, SphereProbe>::MutableItem &item) {
    SphereProbe &cube = item.value;
    bool remove_cube = !cube.used;
    if (cube.updated || remove_cube) {
      sphere_update_ = true;
    }
    cube.updated = false;
    cube.used = false;
    return remove_cube;
  });

  /* Check for deleted or updated plane. */
  planar_update_ = false;
  planar_map_.remove_if([&](const Map<ObjectKey, PlanarProbe>::MutableItem &item) {
    PlanarProbe &plane = item.value;
    bool remove_plane = !plane.used;
    if (plane.updated || remove_plane) {
      planar_update_ = true;
    }
    plane.updated = false;
    plane.used = false;
    return remove_plane;
  });
}

SphereProbeAtlasCoord LightProbeModule::find_empty_atlas_region(int subdivision_level) const
{
  int layer_count = sphere_layer_count();
  SphereProbeAtlasCoord::LocationFinder location_finder(layer_count, subdivision_level);

  location_finder.mark_space_used(world_sphere_.atlas_coord);
  for (const SphereProbe &probe : sphere_map_.values()) {
    location_finder.mark_space_used(probe.atlas_coord);
  }
  return location_finder.first_free_spot();
}

int LightProbeModule::sphere_layer_count() const
{
  int max_layer = world_sphere_.atlas_coord.atlas_layer;
  for (const SphereProbe &probe : sphere_map_.values()) {
    max_layer = max_ii(max_layer, probe.atlas_coord.atlas_layer);
  }
  int layer_count = max_layer + 1;
  return layer_count;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name SphereProbeAtlasCoord
 * \{ */

SphereProbeAtlasCoord::LocationFinder::LocationFinder(int allocated_layer_count,
                                                      int subdivision_level)
{
  subdivision_level_ = subdivision_level;
  areas_per_dimension_ = 1 << subdivision_level_;
  areas_per_layer_ = square_i(areas_per_dimension_);
  /* Always add an additional layer to make sure that there is always a free area.
   * If this area is chosen the atlas will grow. */
  int area_len = (allocated_layer_count + 1) * areas_per_layer_;
  areas_occupancy_.resize(area_len, false);
}

void SphereProbeAtlasCoord::LocationFinder::mark_space_used(const SphereProbeAtlasCoord &coord)
{
  if (coord.atlas_layer == -1) {
    /* Coordinate not allocated yet. */
    return;
  }
  /* The input probe data can be stored in a different subdivision level and should tag all areas
   * of the target subdivision level. Shift right if subdivision is higher, left if lower. */
  const int shift_right = max_ii(coord.subdivision_lvl - subdivision_level_, 0);
  const int shift_left = max_ii(subdivision_level_ - coord.subdivision_lvl, 0);
  const int2 pos_in_location_finder = (coord.area_location() >> shift_right) << shift_left;
  /* Tag all areas this probe overlaps. */
  const int layer_offset = coord.atlas_layer * areas_per_layer_;
  const int areas_overlapped_per_dim = 1 << shift_left;
  for (const int y : IndexRange(areas_overlapped_per_dim)) {
    for (const int x : IndexRange(areas_overlapped_per_dim)) {
      const int2 pos = pos_in_location_finder + int2(x, y);
      const int area_index = pos.x + pos.y * areas_per_dimension_;
      areas_occupancy_[area_index + layer_offset].set();
    }
  }
}

SphereProbeAtlasCoord SphereProbeAtlasCoord::LocationFinder::first_free_spot() const
{
  SphereProbeAtlasCoord result;
  result.subdivision_lvl = subdivision_level_;
  for (int index : areas_occupancy_.index_range()) {
    if (!areas_occupancy_[index]) {
      result.atlas_layer = index / areas_per_layer_;
      result.area_index = index % areas_per_layer_;
      return result;
    }
  }
  /* There should always be a free area. See constructor. */
  BLI_assert_unreachable();
  return result;
}

void SphereProbeAtlasCoord::LocationFinder::print_debug() const
{
  std::ostream &os = std::cout;
  int layer = 0, row = 0, column = 0;
  os << "subdivision " << subdivision_level_ << "\n";
  for (bool spot_taken : areas_occupancy_) {
    if (row == 0 && column == 0) {
      os << "layer " << layer << "\n";
    }
    os << (spot_taken ? 'X' : '-');
    column++;
    if (column == areas_per_dimension_) {
      os << "\n";
      column = 0;
      row++;
    }
    if (row == areas_per_dimension_) {
      row = 0;
      layer++;
    }
  }
}

/** \} */

}  // namespace blender::eevee
