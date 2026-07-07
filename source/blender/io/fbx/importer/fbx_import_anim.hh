/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "fbx_import_util.hh"

namespace blender {

struct Main;

namespace io::fbx {

void import_animations(Main &bmain,
                       const ufbx_scene &fbx,
                       const FbxElementMapping &mapping,
                       const double fps,
                       const float anim_offset);

}  // namespace io::fbx
}  // namespace blender
