/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_basis_types.hh"
#include "BLI_path_util.h"

#include "BKE_context.h"

#include "DEG_depsgraph.h"

#include "IO_path_util_types.hh"

struct OBJExportParams {
  /** Full path to the destination .OBJ file. */
  char filepath[FILE_MAX];
  /** Pretend that destination file folder is this, if non-empty. Used only for tests. */
  char file_base_for_tests[FILE_MAX];

  /** Full path to current blender file (used for comments in output). */
  const char *blen_filepath;

  /** Whether multiple frames should be exported. */
  bool export_animation;
  /** The first frame to be exported. */
  int start_frame;
  /** The last frame to be exported. */
  int end_frame;

  /* Geometry Transform options. */
  blender::math::AxisSigned forward_axis;
  blender::math::AxisSigned up_axis;
  float global_scale;

  /* File Write Options. */
  bool export_selected_objects;
  bool apply_modifiers;
  eEvaluationMode export_eval_mode;
  bool export_uv;
  bool export_normals;
  bool export_colors;
  bool export_materials;
  bool export_triangulated_mesh;
  bool export_curves_as_nurbs;
  ePathReferenceMode path_mode;
  bool export_pbr_extensions;

  /* Grouping options. */
  bool export_object_groups;
  bool export_material_groups;
  bool export_vertex_groups;
  /* Calculate smooth groups from sharp edges. */
  bool export_smooth_groups;
  /* Create bitflags instead of the default "0"/"1" group IDs. */
  bool smooth_groups_bitflags;
};

struct OBJImportParams {
  /** Full path to the source OBJ file to import. */
  char filepath[FILE_MAX];
  /** Value 0 disables clamping. */
  float clamp_size;
  float global_scale;
  blender::math::AxisSigned forward_axis;
  blender::math::AxisSigned up_axis;
  bool use_split_objects;
  bool use_split_groups;
  bool import_vertex_groups;
  bool validate_meshes;
  bool relative_paths;
  bool clear_selection;
};

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
