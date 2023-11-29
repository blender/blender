/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <MaterialXCore/Document.h>

struct Depsgraph;
struct Material;

class ExportImageFunction;

namespace blender::nodes::materialx {

MaterialX::DocumentPtr export_to_materialx(Depsgraph *depsgraph,
                                           Material *material,
                                           ExportImageFunction export_image_fn);

}  // namespace blender::nodes::materialx
