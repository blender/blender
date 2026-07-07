/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include <string>
struct ufbx_material;
namespace blender {

struct Main;
struct Material;
namespace io::fbx {

Material *import_material(Main *bmain, const std::string &base_dir, const ufbx_material &fmat);

}  // namespace io::fbx
}  // namespace blender
