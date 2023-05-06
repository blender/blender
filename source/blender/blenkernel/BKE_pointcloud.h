/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for point clouds.
 */

#ifdef __cplusplus
#  include <mutex>

#  include "BLI_bounds_types.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_shared_cache.hh"

#  include "DNA_pointcloud_types.h"

#  include "BKE_customdata.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct PointCloud;
struct Scene;

/* PointCloud datablock */
extern const char *POINTCLOUD_ATTR_POSITION;
extern const char *POINTCLOUD_ATTR_RADIUS;

#ifdef __cplusplus
namespace blender::bke {

struct PointCloudRuntime {
  /**
   * A cache of bounds shared between data-blocks with unchanged positions and radii.
   * When data changes affect the bounds, the cache is "un-shared" with other geometries.
   * See #SharedCache comments.
   */
  mutable SharedCache<Bounds<float3>> bounds_cache;

  MEM_CXX_CLASS_ALLOC_FUNCS("PointCloudRuntime");
};

}  // namespace blender::bke

inline blender::Span<blender::float3> PointCloud::positions() const
{
  return {static_cast<const blender::float3 *>(
              CustomData_get_layer_named(&this->pdata, CD_PROP_FLOAT3, "position")),
          this->totpoint};
}

inline blender::MutableSpan<blender::float3> PointCloud::positions_for_write()
{
  return {static_cast<blender::float3 *>(CustomData_get_layer_named_for_write(
              &this->pdata, CD_PROP_FLOAT3, "position", this->totpoint)),
          this->totpoint};
}

#endif

void *BKE_pointcloud_add(struct Main *bmain, const char *name);
void *BKE_pointcloud_add_default(struct Main *bmain, const char *name);
struct PointCloud *BKE_pointcloud_new_nomain(int totpoint);
void BKE_pointcloud_nomain_to_pointcloud(struct PointCloud *pointcloud_src,
                                         struct PointCloud *pointcloud_dst);

struct BoundBox *BKE_pointcloud_boundbox_get(struct Object *ob);

bool BKE_pointcloud_attribute_required(const struct PointCloud *pointcloud, const char *name);

/* Dependency Graph */

struct PointCloud *BKE_pointcloud_copy_for_eval(struct PointCloud *pointcloud_src);

void BKE_pointcloud_data_update(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *object);

/* Draw Cache */

enum {
  BKE_POINTCLOUD_BATCH_DIRTY_ALL = 0,
};

void BKE_pointcloud_batch_cache_dirty_tag(struct PointCloud *pointcloud, int mode);
void BKE_pointcloud_batch_cache_free(struct PointCloud *pointcloud);

extern void (*BKE_pointcloud_batch_cache_dirty_tag_cb)(struct PointCloud *pointcloud, int mode);
extern void (*BKE_pointcloud_batch_cache_free_cb)(struct PointCloud *pointcloud);

#ifdef __cplusplus
}
#endif
