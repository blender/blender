/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for point clouds.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct CustomDataLayer;
struct Depsgraph;
struct Main;
struct Object;
struct PointCloud;
struct Scene;

/* PointCloud datablock */
extern const char *POINTCLOUD_ATTR_POSITION;
extern const char *POINTCLOUD_ATTR_RADIUS;

void *BKE_pointcloud_add(struct Main *bmain, const char *name);
void *BKE_pointcloud_add_default(struct Main *bmain, const char *name);
struct PointCloud *BKE_pointcloud_new_nomain(int totpoint);

struct BoundBox *BKE_pointcloud_boundbox_get(struct Object *ob);
bool BKE_pointcloud_minmax(const struct PointCloud *pointcloud, float r_min[3], float r_max[3]);

void BKE_pointcloud_update_customdata_pointers(struct PointCloud *pointcloud);
bool BKE_pointcloud_customdata_required(const struct PointCloud *pointcloud,
                                        struct CustomDataLayer *layer);

/* Dependency Graph */

struct PointCloud *BKE_pointcloud_new_for_eval(const struct PointCloud *pointcloud_src,
                                               int totpoint);
struct PointCloud *BKE_pointcloud_copy_for_eval(struct PointCloud *pointcloud_src, bool reference);

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
