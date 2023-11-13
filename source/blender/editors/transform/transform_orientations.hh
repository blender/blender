/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

#include "RE_engine.h"

struct bPoseChannel;
struct Object;
struct TransInfo;
struct TransformOrientation;

bool gimbal_axis_pose(Object *ob, const bPoseChannel *pchan, float gmat[3][3]);
bool gimbal_axis_object(Object *ob, float gmat[3][3]);

/**
 * Sets the matrix of the specified space orientation.
 * If the matrix cannot be obtained, an orientation different from the one informed is returned.
 */
short transform_orientation_matrix_get(bContext *C,
                                       TransInfo *t,
                                       short orient_index,
                                       const float custom[3][3],
                                       float r_spacemtx[3][3]);
const char *transform_orientations_spacename_get(TransInfo *t, short orient_type);
void transform_orientations_current_set(TransInfo *t, short orient_index);

/**
 * Those two fill in mat and return non-zero on success.
 */
bool transform_orientations_create_from_axis(float mat[3][3],
                                             const float x[3],
                                             const float y[3],
                                             const float z[3]);
bool createSpaceNormal(float mat[3][3], const float normal[3]);
/**
 * \note To recreate an orientation from the matrix:
 * - (plane  == mat[1])
 * - (normal == mat[2])
 */
bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3]);

TransformOrientation *addMatrixSpace(bContext *C,
                                     float mat[3][3],
                                     const char *name,
                                     bool overwrite);
void applyTransformOrientation(const TransformOrientation *ts, float r_mat[3][3], char r_name[64]);

enum {
  ORIENTATION_NONE = 0,
  ORIENTATION_NORMAL = 1,
  ORIENTATION_VERT = 2,
  ORIENTATION_EDGE = 3,
  ORIENTATION_FACE = 4,
};
#define ORIENTATION_USE_PLANE(ty) ELEM(ty, ORIENTATION_NORMAL, ORIENTATION_EDGE, ORIENTATION_FACE)

int getTransformOrientation_ex(const Scene *scene,
                               ViewLayer *view_layer,
                               const View3D *v3d,
                               Object *ob,
                               Object *obedit,
                               float normal[3],
                               float plane[3],
                               short around);
int getTransformOrientation(const bContext *C, float normal[3], float plane[3]);
