/*
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
 */

#pragma once

/** \file
 * \ingroup bke
 * \brief Volume data-block.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct ReportList;
struct Scene;
struct Volume;
struct VolumeGridVector;

/* Module */

void BKE_volumes_init(void);

/* Data-block Management */

void BKE_volume_init_grids(struct Volume *volume);
void *BKE_volume_add(struct Main *bmain, const char *name);

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

bool BKE_volume_load(const struct Volume *volume, const struct Main *bmain);
void BKE_volume_unload(struct Volume *volume);
bool BKE_volume_is_loaded(const struct Volume *volume);

int BKE_volume_num_grids(const struct Volume *volume);
const char *BKE_volume_grids_error_msg(const struct Volume *volume);
const char *BKE_volume_grids_frame_filepath(const struct Volume *volume);
const VolumeGrid *BKE_volume_grid_get_for_read(const struct Volume *volume, int grid_index);
VolumeGrid *BKE_volume_grid_get_for_write(struct Volume *volume, int grid_index);
const VolumeGrid *BKE_volume_grid_active_get_for_read(const struct Volume *volume);
/* Tries to find a grid with the given name. Make sure that the volume has been loaded. */
const VolumeGrid *BKE_volume_grid_find_for_read(const struct Volume *volume, const char *name);

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
  VOLUME_GRID_VECTOR_FLOAT,
  VOLUME_GRID_VECTOR_DOUBLE,
  VOLUME_GRID_VECTOR_INT,
  VOLUME_GRID_POINTS,
} VolumeGridType;

bool BKE_volume_grid_load(const struct Volume *volume, const struct VolumeGrid *grid);
void BKE_volume_grid_unload(const struct Volume *volume, const struct VolumeGrid *grid);
bool BKE_volume_grid_is_loaded(const struct VolumeGrid *grid);

/* Metadata */

const char *BKE_volume_grid_name(const struct VolumeGrid *grid);
VolumeGridType BKE_volume_grid_type(const struct VolumeGrid *grid);
int BKE_volume_grid_channels(const struct VolumeGrid *grid);
/**
 * Transformation from index space to object space.
 */
void BKE_volume_grid_transform_matrix(const struct VolumeGrid *grid, float mat[4][4]);

/* Volume Editing
 *
 * These are intended for modifiers to use on evaluated data-blocks.
 *
 * new_for_eval creates a volume data-block with no grids or file path, but
 * preserves other settings such as viewport display options.
 *
 * copy_for_eval creates a volume data-block preserving everything except the
 * file path. Grids are shared with the source data-block, not copied. */

struct Volume *BKE_volume_new_for_eval(const struct Volume *volume_src);
struct Volume *BKE_volume_copy_for_eval(struct Volume *volume_src, bool reference);

struct VolumeGrid *BKE_volume_grid_add(struct Volume *volume,
                                       const char *name,
                                       VolumeGridType type);
void BKE_volume_grid_remove(struct Volume *volume, struct VolumeGrid *grid);

/* Simplify */
int BKE_volume_simplify_level(const struct Depsgraph *depsgraph);
float BKE_volume_simplify_factor(const struct Depsgraph *depsgraph);

/* File Save */
bool BKE_volume_save(const struct Volume *volume,
                     const struct Main *bmain,
                     struct ReportList *reports,
                     const char *filepath);

#ifdef __cplusplus
}
#endif

/* OpenVDB Grid Access
 *
 * Access to OpenVDB grid for C++. These will automatically load grids from
 * file or copy shared grids to make them writeable. */

#ifdef __cplusplus
#  include "BLI_float4x4.hh"
#  include "BLI_math_vec_types.hh"
#  include "BLI_string_ref.hh"

bool BKE_volume_min_max(const Volume *volume, blender::float3 &r_min, blender::float3 &r_max);

#  ifdef WITH_OPENVDB
#    include <openvdb/openvdb.h>
#    include <openvdb/points/PointDataGrid.h>

VolumeGrid *BKE_volume_grid_add_vdb(Volume &volume,
                                    blender::StringRef name,
                                    openvdb::GridBase::Ptr vdb_grid);

bool BKE_volume_grid_bounds(openvdb::GridBase::ConstPtr grid,
                            blender::float3 &r_min,
                            blender::float3 &r_max);

/**
 * Return a new grid pointer with only the metadata and transform changed.
 * This is useful for instances, where there is a separate transform on top of the original
 * grid transform that must be applied for some operations that only take a grid argument.
 */
openvdb::GridBase::ConstPtr BKE_volume_grid_shallow_transform(openvdb::GridBase::ConstPtr grid,
                                                              const blender::float4x4 &transform);

openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_metadata(const struct VolumeGrid *grid);
openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_read(const struct Volume *volume,
                                                             const struct VolumeGrid *grid);
openvdb::GridBase::Ptr BKE_volume_grid_openvdb_for_write(const struct Volume *volume,
                                                         struct VolumeGrid *grid,
                                                         bool clear);

VolumeGridType BKE_volume_grid_type_openvdb(const openvdb::GridBase &grid);

template<typename OpType>
auto BKE_volume_grid_type_operation(const VolumeGridType grid_type, OpType &&op)
{
  switch (grid_type) {
    case VOLUME_GRID_FLOAT:
      return op.template operator()<openvdb::FloatGrid>();
    case VOLUME_GRID_VECTOR_FLOAT:
      return op.template operator()<openvdb::Vec3fGrid>();
    case VOLUME_GRID_BOOLEAN:
      return op.template operator()<openvdb::BoolGrid>();
    case VOLUME_GRID_DOUBLE:
      return op.template operator()<openvdb::DoubleGrid>();
    case VOLUME_GRID_INT:
      return op.template operator()<openvdb::Int32Grid>();
    case VOLUME_GRID_INT64:
      return op.template operator()<openvdb::Int64Grid>();
    case VOLUME_GRID_VECTOR_INT:
      return op.template operator()<openvdb::Vec3IGrid>();
    case VOLUME_GRID_VECTOR_DOUBLE:
      return op.template operator()<openvdb::Vec3dGrid>();
    case VOLUME_GRID_MASK:
      return op.template operator()<openvdb::MaskGrid>();
    case VOLUME_GRID_POINTS:
      return op.template operator()<openvdb::points::PointDataGrid>();
    case VOLUME_GRID_UNKNOWN:
      break;
  }

  /* Should never be called. */
  BLI_assert_msg(0, "should never be reached");
  return op.template operator()<openvdb::FloatGrid>();
}

openvdb::GridBase::Ptr BKE_volume_grid_create_with_changed_resolution(
    const VolumeGridType grid_type, const openvdb::GridBase &old_grid, float resolution_factor);

#  endif
#endif
