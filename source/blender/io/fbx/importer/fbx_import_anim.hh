/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

#include "fbx_import_util.hh"

struct Main;

namespace blender::io::fbx {

void import_animations(Main &bmain,
                       const ufbx_scene &fbx,
                       const FbxElementMapping &mapping,
                       const double fps,
                       const float anim_offset);

}  // namespace blender::io::fbx
