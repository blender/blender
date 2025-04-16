/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "BLI_path_utils.hh"

#include "DNA_ID.h"

#include "IO_orientation.hh"

struct Mesh;
struct bContext;
struct ReportList;

enum class eFBXVertexColorMode {
  None = 0,
  sRGB = 1,
  Linear = 2,
};

struct FBXImportParams {
  char filepath[FILE_MAX] = "";
  float global_scale = 1.0f;
  eFBXVertexColorMode vertex_colors = eFBXVertexColorMode::sRGB;
  bool validate_meshes = true;
  bool use_custom_normals = true;
  bool import_subdivision = false;
  bool use_custom_props = true;
  bool props_enum_as_string = true;
  bool ignore_leaf_bones = false;

  bool use_anim = true;
  float anim_offset = 1.0f;

  ReportList *reports = nullptr;
};

void FBX_import(bContext *C, const FBXImportParams &params);
