/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for point clouds.
 */

#include <mutex>

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_shared_cache.hh"

#include "DNA_pointcloud_types.h"

struct Depsgraph;
struct Main;
struct Object;
struct PointCloud;
struct Scene;
namespace blender::bke::bake {
struct BakeMaterialsList;
}

/* PointCloud datablock */
extern const char *POINTCLOUD_ATTR_POSITION;
extern const char *POINTCLOUD_ATTR_RADIUS;

namespace blender::bke {

struct PointCloudRuntime {
  /**
   * A cache of bounds shared between data-blocks with unchanged positions and radii.
   * When data changes affect the bounds, the cache is "un-shared" with other geometries.
   * See #SharedCache comments.
   */
  mutable SharedCache<Bounds<float3>> bounds_cache;

  /** Stores weak references to material data blocks. */
  std::unique_ptr<bake::BakeMaterialsList> bake_materials;

  MEM_CXX_CLASS_ALLOC_FUNCS("PointCloudRuntime");
};

}  // namespace blender::bke

void *BKE_pointcloud_add(Main *bmain, const char *name);
void *BKE_pointcloud_add_default(Main *bmain, const char *name);
PointCloud *BKE_pointcloud_new_nomain(int totpoint);
void BKE_pointcloud_nomain_to_pointcloud(PointCloud *pointcloud_src, PointCloud *pointcloud_dst);

bool BKE_pointcloud_attribute_required(const PointCloud *pointcloud, const char *name);

/* Dependency Graph */

PointCloud *BKE_pointcloud_copy_for_eval(const PointCloud *pointcloud_src);

void BKE_pointcloud_data_update(Depsgraph *depsgraph, Scene *scene, Object *object);

/* Draw Cache */

enum {
  BKE_POINTCLOUD_BATCH_DIRTY_ALL = 0,
};

void BKE_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode);
void BKE_pointcloud_batch_cache_free(PointCloud *pointcloud);

extern void (*BKE_pointcloud_batch_cache_dirty_tag_cb)(PointCloud *pointcloud, int mode);
extern void (*BKE_pointcloud_batch_cache_free_cb)(PointCloud *pointcloud);
