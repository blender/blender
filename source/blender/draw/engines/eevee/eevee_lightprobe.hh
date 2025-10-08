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

#pragma once

#include "BLI_bit_vector.hh"
#include "BLI_map.hh"

#include "DNA_world_types.h"

#include "draw_view.hh"

#include "eevee_defines.hh"
#include "eevee_lightprobe_shared.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

using namespace draw;

class Instance;
class VolumeProbeModule;

/* -------------------------------------------------------------------- */
/** \name SphereProbeAtlasCoord
 * \{ */

struct SphereProbeAtlasCoord {
  /** On which layer of the texture array is this reflection probe stored. */
  int atlas_layer = -1;
  /** Gives the extent of this probe relative to the atlas size. */
  int subdivision_lvl = -1;
  /** Area index within the layer with the according subdivision level. */
  int area_index = -1;

  /** Release the current atlas space held by this probe. */
  void free()
  {
    atlas_layer = -1;
  }

  /* Return the area extent in pixel. */
  int area_extent(int mip_lvl = 0) const
  {
    return SPHERE_PROBE_ATLAS_RES >> (subdivision_lvl + mip_lvl);
  }

  /* Coordinate of the area in [0..area_count_per_dimension[ range. */
  int2 area_location() const
  {
    const int area_count_per_dimension = 1 << subdivision_lvl;
    return int2(area_index % area_count_per_dimension, area_index / area_count_per_dimension);
  }

  /* Coordinate of the bottom left corner of the area in [0..SPHERE_PROBE_ATLAS_RES[ range. */
  int2 area_offset(int mip_lvl = 0) const
  {
    return area_location() * area_extent(mip_lvl);
  }

  SphereProbeUvArea as_sampling_coord() const
  {
    SphereProbeUvArea coord;
    coord.scale = float(area_extent()) / SPHERE_PROBE_ATLAS_RES;
    coord.offset = float2(area_offset()) / SPHERE_PROBE_ATLAS_RES;
    coord.layer = atlas_layer;
    return coord;
  }

  SphereProbePixelArea as_write_coord(int mip_lvl) const
  {
    SphereProbePixelArea coord;
    coord.extent = area_extent(mip_lvl);
    coord.offset = area_offset(mip_lvl);
    coord.layer = atlas_layer;
    return coord;
  }

  /**
   * Utility class to find a location in the probe atlas that can be used to store a new probe in
   * a specified subdivision level.
   *
   * The allocation space is subdivided in target subdivision level and is multi layered.
   * A layer has `(2 ^ subdivision_lvl) ^ 2` areas.
   *
   * All allocated probe areas are then process and the candidate areas containing allocated probes
   * are marked as occupied. The location finder then return the first available area.
   */
  class LocationFinder {
    BitVector<> areas_occupancy_;
    int subdivision_level_;
    /* Area count for the given subdivision level. */
    int areas_per_dimension_;
    int areas_per_layer_;

   public:
    LocationFinder(int allocated_layer_count, int subdivision_level);

    /* Mark space to be occupied by the given probe_data. */
    void mark_space_used(const SphereProbeAtlasCoord &coord);

    SphereProbeAtlasCoord first_free_spot() const;

    void print_debug() const;
  };
};

/** \} */

struct LightProbe {
  bool used = false;
  bool initialized = false;
  /* NOTE: Might be not needed if depsgraph updates work as intended. */
  bool updated = false;
  /** Display debug visuals in the viewport. */
  bool viewport_display = false;
  float viewport_display_size = 0.0f;
};

struct VolumeProbe : public LightProbe, VolumeProbeData {
  /** Copy of the transform matrix. */
  float4x4 object_to_world;
  /** Precomputed inverse transform with normalized axes. No position. Used for rotating SH. */
  float4x4 world_to_object;
  /**
   * Reference to the light-cache data.
   * Do not try to dereference it before LightProbeModule::end_sync() as the grid could
   * already have been freed (along with its cache). It is only safe to dereference after the
   * pruning have been done.
   */
  const LightProbeObjectCache *cache = nullptr;
  /** List of associated atlas bricks that are used by this grid. */
  Vector<IrradianceBrickPacked> bricks;
  /** True if the grid needs to be re-uploaded & re-composited with other light-grids. */
  bool do_update;
  /** Index of the grid inside the grid UBO. */
  int grid_index;
  /** Copy of surfel density for debugging purpose. */
  float surfel_density;
  /** Copy of DNA members. */
  float validity_threshold;
  float dilation_threshold;
  float dilation_radius;
  float intensity;
};

struct SphereProbe : public LightProbe, SphereProbeData {
  /** Used to sort the probes by priority. */
  float volume;
  /** True if the area in the atlas needs to be updated. */
  bool do_render = true;
  /** False if the area in the atlas contains undefined data. */
  bool use_for_render = false;
  /** Far and near clipping distances for rendering. */
  float2 clipping_distances;
  /** Atlas region this probe is rendered at (or will be rendered at). */
  SphereProbeAtlasCoord atlas_coord;
};

struct PlanarProbe : public LightProbe, PlanarProbeData {
  /* Copy of object matrices. */
  float4x4 plane_to_world;
  float4x4 world_to_plane;
  /* Offset to the clipping plane in the normal direction. */
  float clipping_offset;
  /* Index in the resource array. */
  int resource_index;

 public:
  /**
   * Update the PlanarProbeData part of the struct.
   * `view` is the view we want to render this probe with.
   */
  void set_view(const draw::View &view, int layer_id);

  /**
   * Create the reflection clip plane equation that clips along the XY plane of the given
   * transform. The `clip_offset` will push the clip plane a bit further to avoid missing pixels in
   * reflections. The transform does not need to be normalized but is expected to be orthogonal.
   * \note Only works after `set_view` was called.
   */
  float4 reflection_clip_plane_get()
  {
    return float4(-normal, math::dot(normal, plane_to_world.location()) - clipping_offset);
  }

 private:
  /**
   * Create the reflection matrix that reflect along the XY plane of the given transform.
   * The transform does not need to be normalized but is expected to be orthogonal.
   */
  float4x4 reflection_matrix_get()
  {
    return plane_to_world * math::from_scale<float4x4>(float3(1, 1, -1)) * world_to_plane;
  }
};

class LightProbeModule {
  friend class IrradianceBake;
  friend class VolumeProbeModule;
  friend class PlanarProbeModule;
  friend class SphereProbeModule;
  friend class BackgroundPipeline;

 private:
  Instance &inst_;

  /** Light Probe map to detect deletion and store associated data. */
  Map<ObjectKey, VolumeProbe> volume_map_;
  Map<ObjectKey, SphereProbe> sphere_map_;
  Map<ObjectKey, PlanarProbe> planar_map_;
  /* World probe is stored separately. */
  SphereProbe world_sphere_;
  /** True if a light-probe update was detected. */
  bool volume_update_;
  bool sphere_update_;
  bool planar_update_;
  /** True if the auto bake feature is enabled & available in this context. */
  bool auto_bake_enabled_;

  eLightProbeResolution sphere_object_resolution_ = LIGHT_PROBE_RESOLUTION_128;

 public:
  LightProbeModule(Instance &inst);
  ~LightProbeModule() {};

  void init();

  void begin_sync();
  void sync_probe(const Object *ob, ObjectHandle &handle);
  void sync_world(const ::World *world, bool has_update);
  void end_sync();

 private:
  void sync_sphere(const Object *ob, ObjectHandle &handle);
  void sync_volume(const Object *ob, ObjectHandle &handle);
  void sync_planar(const Object *ob, ObjectHandle &handle);

  /** Get the number of atlas layers needed to store light probe spheres. */
  int sphere_layer_count() const;

  /** Returns coordinates of an area in the atlas for a probe with the given subdivision level. */
  SphereProbeAtlasCoord find_empty_atlas_region(int subdivision_level) const;
};

}  // namespace blender::eevee
