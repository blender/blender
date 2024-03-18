/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "BKE_context.hh"
#include "BLI_path_util.h"
#include "IO_orientation.hh"

struct STLImportParams {
  /** Full path to the source STL file to import. */
  char filepath[FILE_MAX];
  eIOAxis forward_axis;
  eIOAxis up_axis;
  bool use_facet_normal;
  bool use_scene_unit;
  float global_scale;
  bool use_mesh_validate;

  ReportList *reports = nullptr;
};

struct STLExportParams {
  /** Full path to the to-be-saved STL file. */
  char filepath[FILE_MAX];
  eIOAxis forward_axis;
  eIOAxis up_axis;
  float global_scale;
  bool export_selected_objects;
  bool use_scene_unit;
  bool apply_modifiers;
  bool ascii_format;
  bool use_batch;

  ReportList *reports = nullptr;
};

void STL_import(bContext *C, const STLImportParams *import_params);
void STL_export(bContext *C, const STLExportParams *export_params);
