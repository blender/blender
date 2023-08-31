/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "BKE_context.h"

#include "BLI_math_basis_types.hh"
#include "BLI_path_util.h"

struct STLImportParams {
  /** Full path to the source STL file to import. */
  char filepath[FILE_MAX];
  blender::math::AxisSigned forward_axis;
  blender::math::AxisSigned up_axis;
  bool use_facet_normal;
  bool use_scene_unit;
  float global_scale;
  bool use_mesh_validate;
};

/**
 * C-interface for the importer.
 */
void STL_import(bContext *C, const STLImportParams *import_params);
