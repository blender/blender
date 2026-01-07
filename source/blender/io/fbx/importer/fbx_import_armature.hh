/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "fbx_import_util.hh"

namespace blender {

struct FBXImportParams;
struct Main;

namespace io::fbx {

void import_armatures(Main &bmain,
                      const ufbx_scene &fbx,
                      FbxElementMapping &mapping,
                      const FBXImportParams &params);

}  // namespace io::fbx
}  // namespace blender
