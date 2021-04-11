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
 * \brief Volume data-block rendering and viewport drawing utilities.
 */

#include "BLI_sys_types.h"

#include "DNA_volume_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Volume;
struct VolumeGrid;

/* Dense Voxels */

typedef struct DenseFloatVolumeGrid {
  VolumeGridType type;
  int resolution[3];
  float texture_to_object[4][4];
  int channels;
  float *voxels;
} DenseFloatVolumeGrid;

bool BKE_volume_grid_dense_floats(const struct Volume *volume,
                                  const struct VolumeGrid *volume_grid,
                                  DenseFloatVolumeGrid *r_dense_grid);
void BKE_volume_dense_float_grid_clear(DenseFloatVolumeGrid *dense_grid);

/* Wireframe */

typedef void (*BKE_volume_wireframe_cb)(
    void *userdata, float (*verts)[3], int (*edges)[2], int totvert, int totedge);

void BKE_volume_grid_wireframe(const struct Volume *volume,
                               const struct VolumeGrid *volume_grid,
                               BKE_volume_wireframe_cb cb,
                               void *cb_userdata);

/* Selection Surface */

typedef void (*BKE_volume_selection_surface_cb)(
    void *userdata, float (*verts)[3], int (*tris)[3], int totvert, int tottris);

void BKE_volume_grid_selection_surface(const struct Volume *volume,
                                       const struct VolumeGrid *volume_grid,
                                       BKE_volume_selection_surface_cb cb,
                                       void *cb_userdata);

/* Render */

float BKE_volume_density_scale(const struct Volume *volume, const float matrix[4][4]);

#ifdef __cplusplus
}
#endif
