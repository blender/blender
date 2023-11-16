/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Volume data-block.
 */

#include "BLI_math_vector_types.hh"

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct ReportList;
struct Scene;
struct Volume;
struct VolumeGrid;
struct VolumeGridVector;

/* Module */

void BKE_volumes_init();

/* Data-block Management */

void BKE_volume_init_grids(Volume *volume);
void *BKE_volume_add(Main *bmain, const char *name);

BoundBox *BKE_volume_boundbox_get(Object *ob);

bool BKE_volume_is_y_up(const Volume *volume);
bool BKE_volume_is_points_only(const Volume *volume);

/* Depsgraph */

void BKE_volume_eval_geometry(Depsgraph *depsgraph, Volume *volume);
void BKE_volume_data_update(Depsgraph *depsgraph, Scene *scene, Object *object);

void BKE_volume_grids_backup_restore(Volume *volume,
                                     VolumeGridVector *grids,
                                     const char *filepath);

/* Draw Cache */

enum {
  BKE_VOLUME_BATCH_DIRTY_ALL = 0,
};

void BKE_volume_batch_cache_dirty_tag(Volume *volume, int mode);
void BKE_volume_batch_cache_free(Volume *volume);

extern void (*BKE_volume_batch_cache_dirty_tag_cb)(Volume *volume, int mode);
extern void (*BKE_volume_batch_cache_free_cb)(Volume *volume);

/* Grids
 *
 * For volumes referencing a file, the list of grids and metadata must be
 * loaded before it can be accessed. This happens on-demand, only when needed
 * by the user interface, dependency graph or render engine. */

bool BKE_volume_load(const Volume *volume, const Main *bmain);
void BKE_volume_unload(Volume *volume);
bool BKE_volume_is_loaded(const Volume *volume);

int BKE_volume_num_grids(const Volume *volume);
const char *BKE_volume_grids_error_msg(const Volume *volume);
const char *BKE_volume_grids_frame_filepath(const Volume *volume);
const VolumeGrid *BKE_volume_grid_get_for_read(const Volume *volume, int grid_index);
VolumeGrid *BKE_volume_grid_get_for_write(Volume *volume, int grid_index);
const VolumeGrid *BKE_volume_grid_active_get_for_read(const Volume *volume);
/* Tries to find a grid with the given name. Make sure that the volume has been loaded. */
const VolumeGrid *BKE_volume_grid_find_for_read(const Volume *volume, const char *name);
VolumeGrid *BKE_volume_grid_find_for_write(Volume *volume, const char *name);

/* Tries to set the name of the velocity field. If no such grid exists with the given base name,
 * this will try common post-fixes in order to detect velocity fields split into multiple grids.
 * Return false if neither finding with the base name nor with the post-fixes succeeded. */
bool BKE_volume_set_velocity_grid_by_name(Volume *volume, const char *base_name);

/* Grid
 *
 * By default only grid metadata is loaded, for access to the tree and voxels
 * BKE_volume_grid_load must be called first. */

enum VolumeGridType : int8_t {
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
};

bool BKE_volume_grid_load(const Volume *volume, const VolumeGrid *grid);
void BKE_volume_grid_unload(const Volume *volume, const VolumeGrid *grid);
bool BKE_volume_grid_is_loaded(const VolumeGrid *grid);

/* Metadata */

const char *BKE_volume_grid_name(const VolumeGrid *grid);
VolumeGridType BKE_volume_grid_type(const VolumeGrid *grid);
int BKE_volume_grid_channels(const VolumeGrid *grid);
/**
 * Transformation from index space to object space.
 */
void BKE_volume_grid_transform_matrix(const VolumeGrid *grid, float mat[4][4]);
void BKE_volume_grid_transform_matrix_set(const Volume *volume,
                                          VolumeGrid *volume_grid,
                                          const float mat[4][4]);

/* Volume Editing
 *
 * These are intended for modifiers to use on evaluated data-blocks.
 *
 * new_for_eval creates a volume data-block with no grids or file path, but
 * preserves other settings such as viewport display options.
 *
 * copy_for_eval creates a volume data-block preserving everything except the
 * file path. Grids are shared with the source data-block, not copied. */

Volume *BKE_volume_new_for_eval(const Volume *volume_src);
Volume *BKE_volume_copy_for_eval(const Volume *volume_src);

VolumeGrid *BKE_volume_grid_add(Volume *volume, const char *name, VolumeGridType type);
void BKE_volume_grid_remove(Volume *volume, VolumeGrid *grid);

/**
 * OpenVDB crashes when the determinant of the transform matrix becomes too small.
 */
bool BKE_volume_grid_determinant_valid(double determinant);

/* Simplify */
int BKE_volume_simplify_level(const Depsgraph *depsgraph);
float BKE_volume_simplify_factor(const Depsgraph *depsgraph);

/* File Save */
bool BKE_volume_save(const Volume *volume,
                     const Main *bmain,
                     ReportList *reports,
                     const char *filepath);

/* OpenVDB Grid Access
 *
 * Access to OpenVDB grid for C++. These will automatically load grids from
 * file or copy shared grids to make them writeable. */

bool BKE_volume_min_max(const Volume *volume, blender::float3 &r_min, blender::float3 &r_max);
