/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <MaterialXCore/Document.h>

#include <functional>
#include <string>

struct Depsgraph;
struct Image;
struct ImageUser;
struct Main;
struct Material;
struct Scene;

namespace blender::nodes::materialx {

struct ExportParams {
  std::string output_node_name;
  std::function<std::string(Main *, Scene *, Image *, ImageUser *)> image_fn;
  std::string new_active_uvmap_name;
  std::string original_active_uvmap_name;
};

MaterialX::DocumentPtr export_to_materialx(Depsgraph *depsgraph,
                                           Material *material,
                                           const ExportParams &export_params);

}  // namespace blender::nodes::materialx
