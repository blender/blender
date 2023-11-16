/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Volume data-block rendering and viewport drawing utilities.
 */

#include "BLI_sys_types.h"

#include "DNA_volume_types.h"

struct Volume;
struct VolumeGrid;
enum VolumeGridType : int8_t;

/* Dense Voxels */

struct DenseFloatVolumeGrid {
  VolumeGridType type;
  int resolution[3];
  float texture_to_object[4][4];
  int channels;
  float *voxels;
};

bool BKE_volume_grid_dense_floats(const Volume *volume,
                                  const VolumeGrid *volume_grid,
                                  DenseFloatVolumeGrid *r_dense_grid);
void BKE_volume_dense_float_grid_clear(DenseFloatVolumeGrid *dense_grid);

/* Wireframe */

typedef void (*BKE_volume_wireframe_cb)(
    void *userdata, const float (*verts)[3], const int (*edges)[2], int totvert, int totedge);

void BKE_volume_grid_wireframe(const Volume *volume,
                               const VolumeGrid *volume_grid,
                               BKE_volume_wireframe_cb cb,
                               void *cb_userdata);

/* Selection Surface */

using BKE_volume_selection_surface_cb =
    void (*)(void *userdata, float (*verts)[3], int (*tris)[3], int totvert, int tottris);

void BKE_volume_grid_selection_surface(const Volume *volume,
                                       const VolumeGrid *volume_grid,
                                       BKE_volume_selection_surface_cb cb,
                                       void *cb_userdata);

/* Render */

float BKE_volume_density_scale(const Volume *volume, const float matrix[4][4]);
