/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "DNA_lightprobe_types.h"

#include "BLI_math_quaternion_types.hh"

#include "eevee_lightprobe.hh"
#include "eevee_shader_shared.hh"

namespace blender::eevee {

using blender::math::AxisSigned;
using blender::math::CartesianBasis;

class Instance;
class CapturePipeline;
class ShadowModule;
class Camera;
class ReflectionProbeModule;

/**
 * Baking related pass and data. Not used at runtime.
 */
class IrradianceBake {
  friend CapturePipeline;
  friend ShadowModule;
  friend Camera;

 private:
  Instance &inst_;

  /** Light cache being baked. */
  LightCache *light_cache_ = nullptr;
  /** Surface elements that represent the scene. */
  SurfelBuf surfels_buf_;
  /** Capture state. */
  CaptureInfoBuf capture_info_buf_;
  /** Framebuffer. */
  Framebuffer empty_raster_fb_ = {"empty_raster_fb_"};
  /** Evaluate light object contribution and store result to surfel. */
  PassSimple surfel_light_eval_ps_ = {"LightEval"};
  /** Create linked list of surfel to emulated ray-cast. */
  PassSimple surfel_ray_build_ps_ = {"RayBuild"};
  /** Create linked list of surfel to cluster them in the 3D irradiance grid. */
  PassSimple surfel_cluster_build_ps_ = {"RayBuild"};
  /** Propagate light from surfel to surfel. */
  PassSimple surfel_light_propagate_ps_ = {"LightPropagate"};
  /** Capture surfel lighting to irradiance samples. */
  PassSimple irradiance_capture_ps_ = {"IrradianceCapture"};
  /** Compute virtual offset for each irradiance samples. */
  PassSimple irradiance_offset_ps_ = {"IrradianceOffset"};
  /** Compute scene bounding box. */
  PassSimple irradiance_bounds_ps_ = {"IrradianceBounds"};
  /** Index of source and destination radiance in radiance double-buffer. */
  int radiance_src_ = 0, radiance_dst_ = 1;

  /**
   * Basis orientation for each baking projection.
   * Note that this is the view orientation. The projection matrix will take the negative Z axis
   * as forward and Y as up. */
  CartesianBasis basis_x_ = {AxisSigned::Z_POS, AxisSigned::Y_POS, AxisSigned::X_NEG};
  CartesianBasis basis_y_ = {AxisSigned::X_POS, AxisSigned::Z_POS, AxisSigned::Y_NEG};
  CartesianBasis basis_z_ = {AxisSigned::Y_POS, AxisSigned::X_POS, AxisSigned::Z_NEG};
  /** Views for each baking projection. */
  View view_x_ = {"BakingViewX"};
  View view_y_ = {"BakingViewY"};
  View view_z_ = {"BakingViewZ"};
  /** Pixel resolution in each of the projection axes. Match the target surfel density. */
  int3 grid_pixel_extent_ = int3(0);
  /** Information for surfel list building. */
  SurfelListInfoBuf list_info_buf_ = {"list_info_buf_"};
  /** List array containing list start surfel index. Cleared to -1. */
  StorageArrayBuffer<int, 16, true> list_start_buf_ = {"list_start_buf_"};

  /* Dispatch size for per surfel workload. */
  int3 dispatch_per_surfel_ = int3(1);
  /* Dispatch size for per surfel list workload. */
  int3 dispatch_per_list_ = int3(1);
  /* Dispatch size for per grid sample workload. */
  int3 dispatch_per_grid_sample_ = int3(1);

  /** View used to flatten the surfels into surfel lists representing rays. */
  View ray_view_ = {"RayProjectionView"};

  /** Irradiance textures for baking. Only represents one grid in there. */
  Texture irradiance_L0_tx_ = {"irradiance_L0_tx_"};
  Texture irradiance_L1_a_tx_ = {"irradiance_L1_a_tx_"};
  Texture irradiance_L1_b_tx_ = {"irradiance_L1_b_tx_"};
  Texture irradiance_L1_c_tx_ = {"irradiance_L1_c_tx_"};
  /** Offset per irradiance point to apply to the baking location. */
  Texture virtual_offset_tx_ = {"virtual_offset_tx_"};
  /** List of closest surfels per irradiance sample. */
  Texture cluster_list_tx_ = {"cluster_list_tx_"};
  /** Contains ratio of back-face hits. Allows to get rid of invalid probes. */
  Texture validity_tx_ = {"validity_tx_"};

  /* Bounding sphere of the scene being baked. In world space. */
  float4 scene_bound_sphere_;
  /* Surfel per unit distance. */
  float surfel_density_ = 1.0f;
  /**
   * Minimum distance a grid sample point should have with a surface.
   * In minimum grid sample spacing.
   * Avoids samples to be too close to surface even if they are valid.
   */
  float min_distance_to_surface_ = 0.05f;
  /**
   * Maximum distance from the grid sample point to the baking location.
   * In minimum grid sample spacing.
   * Avoids samples to be too far from their actual origin.
   */
  float max_virtual_offset_ = 0.1f;

 public:
  IrradianceBake(Instance &inst) : inst_(inst){};

  void init(const Object &probe_object);
  void sync();

  /** Create the views used to rasterize the scene into surfel representation. */
  void surfel_raster_views_sync(const float3 &scene_min, const float3 &scene_max);
  /** Create a surfel representation of the scene from the probe using the capture pipeline. */
  void surfels_create(const Object &probe_object);
  /** Evaluate direct lighting (and also clear the surfels radiance). */
  void surfels_lights_eval();
  /**
   * Create a surfel lists per irradiance probe in order to compute the virtual baking offset.
   * NOTE: The resulting lists are only valid until `clusters_build()` or `raylists_build()` are
   * called since they share the same links inside the Surfel struct.
   */
  void clusters_build();
  /**
   * Create a surfel lists to emulate ray-casts for the current sample random direction.
   * NOTE: The resulting lists are only valid until `clusters_build()` or `raylists_build()` are
   * called since they share the same links inside the Surfel struct.
   */
  void raylists_build();
  /** Propagate light from surfel to surfel in a random direction over the sphere. */
  void propagate_light();
  /** Compute offset to bias irradiance capture location. */
  void irradiance_offset();
  /** Store surfel irradiance inside the irradiance grid samples. */
  void irradiance_capture();

  /** Read grid unpacked irradiance back to CPU and returns as a #LightProbeGridCacheFrame. */
  LightProbeGridCacheFrame *read_result_unpacked();
  /** Read grid packed irradiance back to CPU and returns as a #LightProbeGridCacheFrame. */
  LightProbeGridCacheFrame *read_result_packed();

 private:
  /** Read surfel data back to CPU into \a cache_frame . */
  void read_surfels(LightProbeGridCacheFrame *cache_frame);
  /** Read virtual offset back to CPU into \a cache_frame . */
  void read_virtual_offset(LightProbeGridCacheFrame *cache_frame);
};

/**
 * Runtime container of diffuse indirect lighting.
 * Also have debug and baking components.
 */
class IrradianceCache {
 public:
  IrradianceBake bake;

 private:
  Instance &inst_;

  /** Atlas 3D texture containing all loaded grid data. */
  Texture irradiance_atlas_tx_ = {"irradiance_atlas_tx_"};
  /** Reserved atlas brick for world irradiance. */
  int world_brick_index_ = 0;
  /** Data structure used to index irradiance cache pages inside the atlas. */
  IrradianceGridDataBuf grids_infos_buf_ = {"grids_infos_buf_"};
  IrradianceBrickBuf bricks_infos_buf_ = {"bricks_infos_buf_"};
  /** Pool of atlas regions to allocate to different grids. */
  Vector<IrradianceBrickPacked> brick_pool_;
  /** Stream data into the irradiance atlas texture. */
  PassSimple grid_upload_ps_ = {"IrradianceCache.Upload"};
  /** If true, will trigger the reupload of all grid data instead of just streaming new ones. */
  bool do_full_update_ = true;

  /** Display debug data. */
  PassSimple debug_ps_ = {"IrradianceCache.Debug"};
  /** Debug surfel elements copied from the light cache. */
  draw::StorageArrayBuffer<Surfel> debug_surfels_buf_;

  /** Display grid cache data. */
  bool display_grids_enabled_ = false;
  PassSimple display_grids_ps_ = {"IrradianceCache.Display Grids"};

 public:
  IrradianceCache(Instance &inst) : bake(inst), inst_(inst){};
  ~IrradianceCache(){};

  void init();
  void sync();
  void set_view(View &view);
  void viewport_draw(View &view, GPUFrameBuffer *view_fb);

  Vector<IrradianceBrickPacked> bricks_alloc(int brick_len);
  void bricks_free(Vector<IrradianceBrickPacked> &bricks);

  template<typename T> void bind_resources(draw::detail::PassBase<T> *pass)
  {
    pass->bind_ubo(IRRADIANCE_GRID_BUF_SLOT, &grids_infos_buf_);
    pass->bind_ssbo(IRRADIANCE_BRICK_BUF_SLOT, &bricks_infos_buf_);
    pass->bind_texture(IRRADIANCE_ATLAS_TEX_SLOT, &irradiance_atlas_tx_);
  }

 private:
  void debug_pass_draw(View &view, GPUFrameBuffer *view_fb);
  void display_pass_draw(View &view, GPUFrameBuffer *view_fb);

  friend class ReflectionProbeModule;
};

}  // namespace blender::eevee
