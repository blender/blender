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

class ExportImageFunction;

namespace blender::nodes::materialx {

using ExportImageFunction = std::function<std::string(Main *, Scene *, Image *, ImageUser *)>;

MaterialX::DocumentPtr export_to_materialx(Depsgraph *depsgraph,
                                           Material *material,
                                           const std::string &material_name,
                                           ExportImageFunction export_image_fn);

}  // namespace blender::nodes::materialx
