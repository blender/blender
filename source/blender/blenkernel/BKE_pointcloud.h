/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_POINTCLOUD_H__
#define __BKE_POINTCLOUD_H__

/** \file BKE_pointcloud.h
 *  \ingroup bke
 *  \brief General operations for pointclouds.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct PointCloud;
struct Scene;

void *BKE_pointcloud_add(struct Main *bmain, const char *name);
struct PointCloud *BKE_pointcloud_copy(struct Main *bmain, const struct PointCloud *pointcloud);

struct BoundBox *BKE_pointcloud_boundbox_get(struct Object *ob);

void BKE_pointcloud_update_customdata_pointers(struct PointCloud *pointcloud);

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

#endif
