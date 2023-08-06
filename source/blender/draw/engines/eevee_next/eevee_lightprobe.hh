/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Module that handles light probe update tagging.
 * Lighting data is contained in their respective module `IrradianceCache` and `ReflectionProbes`.
 */

#pragma once

#include "BLI_map.hh"

#include "eevee_sync.hh"

namespace blender::eevee {

class Instance;
class IrradianceCache;

struct LightProbe {
  bool used = false;
  bool initialized = false;
  bool updated = false;
};

struct IrradianceGrid : public LightProbe, IrradianceGridData {
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
  /** True if the grid needs to be reuploaded & re-composited with other light-grids. */
  bool do_update;
  /** Index of the grid inside the grid UBO. */
  int grid_index;
  /** Copy of surfel density for debugging purpose. */
  float surfel_density;
  /** Copy of DNA members. */
  float validity_threshold;
  float dilation_threshold;
  float dilation_radius;
};

struct ReflectionCube : public LightProbe {
};

class LightProbeModule {
  friend class IrradianceCache;

 private:
  Instance &inst_;

  /** Light Probe map to detect deletion and store associated data. */
  Map<ObjectKey, IrradianceGrid> grid_map_;
  Map<ObjectKey, ReflectionCube> cube_map_;
  /** True if a grid update was detected. It will trigger a bake if auto bake is enabled. */
  bool grid_update_;
  /** True if a grid update was detected. It will trigger a bake if auto bake is enabled. */
  bool cube_update_;
  /** True if the auto bake feature is enabled & available in this context. */
  bool auto_bake_enabled_;

 public:
  LightProbeModule(Instance &inst) : inst_(inst){};
  ~LightProbeModule(){};

  void begin_sync();

  void sync_cube(ObjectHandle &handle);
  void sync_grid(const Object *ob, ObjectHandle &handle);

  void sync_probe(const Object *ob, ObjectHandle &handle);

  void end_sync();
};

}  // namespace blender::eevee
