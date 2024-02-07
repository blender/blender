/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec2f.h>

struct ARegion;
struct Object;
struct View3D;
struct Depsgraph;
struct RenderData;

namespace blender::render::hydra {

pxr::GfCamera gf_camera(const Depsgraph *depsgraph,
                        const View3D *v3d,
                        const ARegion *region,
                        const pxr::GfVec4f &border);

pxr::GfCamera gf_camera(const Object *camera_obj,
                        const pxr::GfVec2i &res,
                        const pxr::GfVec4f &border);

}  // namespace blender::render::hydra
