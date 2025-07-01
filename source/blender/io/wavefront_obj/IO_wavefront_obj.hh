/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <limits.h>

#include "BLI_path_utils.hh"

#include "BKE_geometry_set.hh"

#include "DEG_depsgraph.hh"

#include "IO_orientation.hh"
#include "IO_path_util_types.hh"

struct bContext;
struct ReportList;

struct OBJExportParams {
  /** Full path to the destination `.OBJ` file. */
  char filepath[FILE_MAX] = "";
  /** Pretend that destination file folder is this, if non-empty. Used only for tests. */
  char file_base_for_tests[FILE_MAX] = "";
  char collection[MAX_ID_NAME - 2] = "";

  /** Full path to current blender file (used for comments in output). */
  const char *blen_filepath = nullptr;

  /** Whether multiple frames should be exported. */
  bool export_animation = false;
  /** The first frame to be exported. */
  int start_frame = INT_MIN;
  /** The last frame to be exported. */
  int end_frame = INT_MAX;

  /* Geometry Transform options. */
  eIOAxis forward_axis = IO_AXIS_NEGATIVE_Z;
  eIOAxis up_axis = IO_AXIS_Y;
  float global_scale = 1.0f;

  /* File Write Options. */
  bool export_selected_objects = false;
  bool apply_modifiers = true;
  eEvaluationMode export_eval_mode = DAG_EVAL_VIEWPORT;
  bool export_uv = true;
  bool export_normals = true;
  bool export_colors = false;
  bool export_materials = true;
  bool export_triangulated_mesh = false;
  bool export_curves_as_nurbs = false;
  ePathReferenceMode path_mode = PATH_REFERENCE_AUTO;
  bool export_pbr_extensions = false;

  /* Grouping options. */
  bool export_object_groups = false;
  bool export_material_groups = false;
  bool export_vertex_groups = false;
  /* Calculate smooth groups from sharp edges. */
  bool export_smooth_groups = false;
  /* Create bitflags instead of the default "0"/"1" group IDs. */
  bool smooth_groups_bitflags = false;

  ReportList *reports = nullptr;
};

/**
 * Behavior when the name of an imported material
 * conflicts with an existing material.
 */
enum eOBJMtlNameCollisionMode {
  OBJ_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  OBJ_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
};

struct OBJImportParams {
  /** Full path to the source OBJ file to import. */
  char filepath[FILE_MAX] = "";
  /** Value 0 disables clamping. */
  float clamp_size = 0.0f;
  float global_scale = 1.0f;
  eIOAxis forward_axis = IO_AXIS_NEGATIVE_Z;
  eIOAxis up_axis = IO_AXIS_Y;
  char collection_separator = 0;
  bool use_split_objects = true;
  bool use_split_groups = false;
  bool import_vertex_groups = false;
  bool validate_meshes = true;
  bool close_spline_loops = true;
  bool relative_paths = true;
  bool clear_selection = true;

  /** How to handle material name collisions during import. */
  eOBJMtlNameCollisionMode mtl_name_collision_mode = OBJ_MTL_NAME_COLLISION_MAKE_UNIQUE;

  ReportList *reports = nullptr;
};

/**
 * Reads and returns just the meshes in the obj file
 */
void OBJ_import_geometries(const OBJImportParams *import_params,
                           blender::Vector<blender::bke::GeometrySet> &geometries);

/**
 * Perform the full import process.
 * Import also changes the selection & the active object; callers
 * need to update the UI bits if needed.
 */
void OBJ_import(bContext *C, const OBJImportParams *import_params);

/**
 * Perform the full export process.
 */
void OBJ_export(bContext *C, const OBJExportParams *export_params);
