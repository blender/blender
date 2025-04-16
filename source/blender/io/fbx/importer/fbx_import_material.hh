/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include <string>

struct Main;
struct Material;
struct ufbx_material;

namespace blender::io::fbx {

Material *import_material(Main *bmain, const std::string &base_dir, const ufbx_material &fmat);

}  // namespace blender::io::fbx
