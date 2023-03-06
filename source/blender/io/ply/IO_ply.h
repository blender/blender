/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BKE_context.h"

#include "BLI_path_util.h"
#include "DNA_windowmanager_types.h"
#include "IO_orientation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PLY_VERTEX_COLOR_NONE = 0,
  PLY_VERTEX_COLOR_SRGB = 1,
  PLY_VERTEX_COLOR_LINEAR = 2,
} ePLYVertexColorMode;

struct PLYExportParams {
  /** Full path to the destination .PLY file. */
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
  bool export_triangulated_mesh;
};

struct PLYImportParams {
  /** Full path to the source PLY file to import. */
  char filepath[FILE_MAX];
  eIOAxis forward_axis;
  eIOAxis up_axis;
  bool use_scene_unit;
  float global_scale;
  ePLYVertexColorMode vertex_colors;
  bool merge_verts;
};

/**
 * C-interface for the importer and exporter.
 */
void PLY_export(bContext *C, const struct PLYExportParams *export_params);

void PLY_import(bContext *C, const struct PLYImportParams *import_params, wmOperator *op);

#ifdef __cplusplus
}
#endif
