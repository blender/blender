/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_path_utils.hh"

#include "DNA_ID.h"

#include "IO_orientation.hh"

struct Mesh;
struct bContext;
struct ReportList;

enum class ePLYVertexColorMode {
  None = 0,
  sRGB = 1,
  Linear = 2,
};

struct PLYExportParams {
  /** Full path to the destination `.PLY` file. */
  char filepath[FILE_MAX] = "";
  /** Pretend that destination file folder is this, if non-empty. Used only for tests. */
  char file_base_for_tests[FILE_MAX] = "";

  /** Full path to current blender file (used for comments in output). */
  const char *blen_filepath = nullptr;

  /** File export format, ASCII if true, binary otherwise. */
  bool ascii_format = false;

  /* Geometry Transform options. */
  eIOAxis forward_axis = IO_AXIS_Y;
  eIOAxis up_axis = IO_AXIS_Z;
  float global_scale = 1.0f;

  /* File Write Options. */
  bool export_selected_objects = false;
  bool apply_modifiers = true;
  bool export_uv = true;
  bool export_normals = false;
  ePLYVertexColorMode vertex_colors = ePLYVertexColorMode::sRGB;
  bool export_attributes = true;
  bool export_triangulated_mesh = false;
  char collection[MAX_IDPROP_NAME] = "";

  ReportList *reports = nullptr;
};

struct PLYImportParams {
  /** Full path to the source PLY file to import. */
  char filepath[FILE_MAX] = "";
  eIOAxis forward_axis = IO_AXIS_Y;
  eIOAxis up_axis = IO_AXIS_Z;
  bool use_scene_unit = false;
  float global_scale = 1.0f;
  ePLYVertexColorMode vertex_colors = ePLYVertexColorMode::sRGB;
  bool import_attributes = true;
  bool merge_verts = false;

  ReportList *reports = nullptr;
};

void PLY_export(bContext *C, const PLYExportParams &params);

void PLY_import(bContext *C, const PLYImportParams &params);

Mesh *PLY_import_mesh(const PLYImportParams &params);
