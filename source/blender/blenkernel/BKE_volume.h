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

#ifndef __BKE_VOLUME_H__
#define __BKE_VOLUME_H__

/** \file BKE_volume.h
 *  \ingroup bke
 *  \brief Volume datablock.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;
struct Volume;
struct VolumeGridVector;

/* Module */

void BKE_volumes_init(void);

/* Datablock Management */

void BKE_volume_init_grids(struct Volume *volume);
void *BKE_volume_add(struct Main *bmain, const char *name);
struct Volume *BKE_volume_copy(struct Main *bmain, const struct Volume *volume);

struct BoundBox *BKE_volume_boundbox_get(struct Object *ob);

bool BKE_volume_is_y_up(const struct Volume *volume);
bool BKE_volume_is_points_only(const struct Volume *volume);

/* Depsgraph */

void BKE_volume_eval_geometry(struct Depsgraph *depsgraph, struct Volume *volume);
void BKE_volume_data_update(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *object);

void BKE_volume_grids_backup_restore(struct Volume *volume,
                                     struct VolumeGridVector *grids,
                                     const char *filepath);

/* Draw Cache */

enum {
  BKE_VOLUME_BATCH_DIRTY_ALL = 0,
};

void BKE_volume_batch_cache_dirty_tag(struct Volume *volume, int mode);
void BKE_volume_batch_cache_free(struct Volume *volume);

extern void (*BKE_volume_batch_cache_dirty_tag_cb)(struct Volume *volume, int mode);
extern void (*BKE_volume_batch_cache_free_cb)(struct Volume *volume);

/* Grids
 *
 * For volumes referencing a file, the list of grids and metadata must be
 * loaded before it can be accessed. This happens on-demand, only when needed
 * by the user interface, dependency graph or render engine. */

typedef struct VolumeGrid VolumeGrid;

bool BKE_volume_load(struct Volume *volume, struct Main *bmain);
void BKE_volume_unload(struct Volume *volume);
bool BKE_volume_is_loaded(const struct Volume *volume);

int BKE_volume_num_grids(const struct Volume *volume);
const char *BKE_volume_grids_error_msg(const struct Volume *volume);
VolumeGrid *BKE_volume_grid_get(const struct Volume *volume, int grid_index);
VolumeGrid *BKE_volume_grid_active_get(const struct Volume *volume);
VolumeGrid *BKE_volume_grid_find(const struct Volume *volume, const char *name);

/* Grid
 *
 * By default only grid metadata is loaded, for access to the tree and voxels
 * BKE_volume_grid_load must be called first. */

typedef enum VolumeGridType {
  VOLUME_GRID_UNKNOWN = 0,
  VOLUME_GRID_BOOLEAN,
  VOLUME_GRID_FLOAT,
  VOLUME_GRID_DOUBLE,
  VOLUME_GRID_INT,
  VOLUME_GRID_INT64,
  VOLUME_GRID_MASK,
  VOLUME_GRID_STRING,
  VOLUME_GRID_VECTOR_FLOAT,
  VOLUME_GRID_VECTOR_DOUBLE,
  VOLUME_GRID_VECTOR_INT,
  VOLUME_GRID_POINTS,
} VolumeGridType;

bool BKE_volume_grid_load(const struct Volume *volume, struct VolumeGrid *grid);
void BKE_volume_grid_unload(const struct Volume *volume, struct VolumeGrid *grid);
bool BKE_volume_grid_is_loaded(const struct VolumeGrid *grid);

/* Metadata */
const char *BKE_volume_grid_name(const struct VolumeGrid *grid);
VolumeGridType BKE_volume_grid_type(const struct VolumeGrid *grid);
int BKE_volume_grid_channels(const struct VolumeGrid *grid);
void BKE_volume_grid_transform_matrix(const struct VolumeGrid *grid, float mat[4][4]);

/* Bounds */
bool BKE_volume_grid_bounds(const struct VolumeGrid *grid, float min[3], float max[3]);

/* Volume Editing
 *
 * These are intended for modifiers to use on evaluated datablocks.
 *
 * new_for_eval creates a volume datablock with no grids or file path, but
 * preserves other settings such as viewport display options.
 *
 * copy_for_eval creates a volume datablock preserving everything except the
 * file path. Grids are shared with the source datablock, not copied. */

struct Volume *BKE_volume_new_for_eval(const struct Volume *volume_src);
struct Volume *BKE_volume_copy_for_eval(struct Volume *volume_src, bool reference);

struct VolumeGrid *BKE_volume_grid_add(struct Volume *volume,
                                       const char *name,
                                       VolumeGridType type);
void BKE_volume_grid_remove(struct Volume *volume, struct VolumeGrid *grid);

#ifdef __cplusplus
}
#endif

/* OpenVDB Grid Access
 *
 * Access to OpenVDB grid for C++. These will automatically load grids from
 * file or copy shared grids to make them writeable. */

#if defined(__cplusplus) && defined(WITH_OPENVDB)
#  include <openvdb/openvdb.h>
openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_metadata(const struct VolumeGrid *grid);
openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_read(const struct Volume *volume,
                                                             struct VolumeGrid *grid);
openvdb::GridBase::Ptr BKE_volume_grid_openvdb_for_write(const struct Volume *volume,
                                                         struct VolumeGrid *grid,
                                                         const bool clear);
#endif

#endif
