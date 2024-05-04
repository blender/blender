/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_path_util.h"

#include "DNA_ID.h"

#include "IO_orientation.hh"

struct bContext;
struct ReportList;

enum ePLYVertexColorMode {
  PLY_VERTEX_COLOR_NONE = 0,
  PLY_VERTEX_COLOR_SRGB = 1,
  PLY_VERTEX_COLOR_LINEAR = 2,
};

struct PLYExportParams {
  /** Full path to the destination `.PLY` file. */
  char filepath[FILE_MAX];
  /** Pretend that destination file folder is this, if non-empty. Used only for tests. */
  char file_base_for_tests[FILE_MAX];

  /** Full path to current blender file (used for comments in output). */
  const char *blen_filepath;

  /** File export format, ASCII if true, binary otherwise. */
  bool ascii_format;

  /* Geometry Transform options. */
  eIOAxis forward_axis;
  eIOAxis up_axis;
  float global_scale;

  /* File Write Options. */
  bool export_selected_objects;
  bool apply_modifiers;
  bool export_uv;
  bool export_normals;
  ePLYVertexColorMode vertex_colors;
  bool export_attributes;
  bool export_triangulated_mesh;
  char collection[MAX_IDPROP_NAME] = "";

  ReportList *reports = nullptr;
};

struct PLYImportParams {
  /** Full path to the source PLY file to import. */
  char filepath[FILE_MAX];
  eIOAxis forward_axis;
  eIOAxis up_axis;
  bool use_scene_unit;
  float global_scale;
  ePLYVertexColorMode vertex_colors;
  bool import_attributes;
  bool merge_verts;

  ReportList *reports = nullptr;
};

/**
 * C-interface for the importer and exporter.
 */
void PLY_export(bContext *C, const PLYExportParams *export_params);

void PLY_import(bContext *C, const PLYImportParams *import_params);
