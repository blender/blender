/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "BLI_path_utils.hh"

#include "DNA_ID.h"

#include "IO_orientation.hh"

struct Mesh;
struct bContext;
struct ReportList;

struct STLImportParams {
  /** Full path to the source STL file to import. */
  char filepath[FILE_MAX] = "";
  eIOAxis forward_axis = IO_AXIS_Y;
  eIOAxis up_axis = IO_AXIS_Z;
  bool use_facet_normal = false;
  bool use_scene_unit = false;
  float global_scale = 1.0f;
  bool use_mesh_validate = true;

  ReportList *reports = nullptr;
};

struct STLExportParams {
  /** Full path to the to-be-saved STL file. */
  char filepath[FILE_MAX] = "";
  eIOAxis forward_axis = IO_AXIS_Y;
  eIOAxis up_axis = IO_AXIS_Z;
  float global_scale = 1.0f;
  bool export_selected_objects = false;
  bool use_scene_unit = false;
  bool apply_modifiers = true;
  bool ascii_format = false;
  bool use_batch = false;
  char collection[MAX_IDPROP_NAME] = "";

  ReportList *reports = nullptr;
};

void STL_import(bContext *C, const STLImportParams *import_params);
void STL_export(bContext *C, const STLExportParams *export_params);

Mesh *STL_import_mesh(const STLImportParams *import_params);
