/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "fbx_import_util.hh"

struct FBXImportParams;
struct Main;

namespace blender::io::fbx {

void import_meshes(Main &bmain,
                   const ufbx_scene &fbx,
                   FbxElementMapping &mapping,
                   const FBXImportParams &params);

}  // namespace blender::io::fbx
