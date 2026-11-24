/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include <string>

struct ufbx_scene;

namespace blender {

struct FBXImportParams;
struct Main;

namespace io::fbx {

struct FbxElementMapping;

void import_materials(Main &bmain,
                      const ufbx_scene &fbx,
                      FbxElementMapping &mapping,
                      const FBXImportParams &params,
                      const std::string &base_dir);

}  // namespace io::fbx
}  // namespace blender
