/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#pragma once

namespace blender {

struct FBXImportParams;
struct Main;
struct Scene;
struct ViewLayer;

namespace io::fbx {

void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const FBXImportParams &params);

}  // namespace io::fbx
}  // namespace blender
