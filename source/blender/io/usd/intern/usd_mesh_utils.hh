/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <pxr/usd/usdGeom/primvar.h>

struct Mesh;

namespace blender::io::usd {

void read_generic_mesh_primvar(Mesh *mesh,
                               const pxr::UsdGeomPrimvar &primvar,
                               double motionSampleTime,
                               bool is_left_handed);

}  // namespace blender::io::usd
