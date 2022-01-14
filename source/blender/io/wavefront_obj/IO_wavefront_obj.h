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

/** \file
 * \ingroup obj
 */

#pragma once

#include "BKE_context.h"
#include "BLI_path_util.h"
#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OBJ_AXIS_X_UP = 0,
  OBJ_AXIS_Y_UP = 1,
  OBJ_AXIS_Z_UP = 2,
  OBJ_AXIS_NEGATIVE_X_UP = 3,
  OBJ_AXIS_NEGATIVE_Y_UP = 4,
  OBJ_AXIS_NEGATIVE_Z_UP = 5,
} eTransformAxisUp;

typedef enum {
  OBJ_AXIS_X_FORWARD = 0,
  OBJ_AXIS_Y_FORWARD = 1,
  OBJ_AXIS_Z_FORWARD = 2,
  OBJ_AXIS_NEGATIVE_X_FORWARD = 3,
  OBJ_AXIS_NEGATIVE_Y_FORWARD = 4,
  OBJ_AXIS_NEGATIVE_Z_FORWARD = 5,
} eTransformAxisForward;

static const int TOTAL_AXES = 3;

struct OBJExportParams {
  /** Full path to the destination .OBJ file. */
  char filepath[FILE_MAX];

  /** Full path to current blender file (used for comments in output). */
  const char *blen_filepath;

  /** Whether multiple frames should be exported. */
  bool export_animation;
  /** The first frame to be exported. */
  int start_frame;
  /** The last frame to be exported. */
  int end_frame;

  /* Geometry Transform options. */
  eTransformAxisForward forward_axis;
  eTransformAxisUp up_axis;
  float scaling_factor;

  /* File Write Options. */
  bool export_selected_objects;
  eEvaluationMode export_eval_mode;
  bool export_uv;
  bool export_normals;
  bool export_materials;
  bool export_triangulated_mesh;
  bool export_curves_as_nurbs;

  /* Grouping options. */
  bool export_object_groups;
  bool export_material_groups;
  bool export_vertex_groups;
  /**
   * Calculate smooth groups from sharp edges.
   */
  bool export_smooth_groups;
  /**
   * Create bitflags instead of the default "0"/"1" group IDs.
   */
  bool smooth_groups_bitflags;
};

void OBJ_export(bContext *C, const struct OBJExportParams *export_params);

#ifdef __cplusplus
}
#endif
