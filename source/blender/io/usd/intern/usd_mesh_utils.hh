/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <pxr/usd/usdGeom/primvar.h>

namespace blender {

struct Mesh;

namespace io::usd {

void read_generic_mesh_primvar(Mesh *mesh,
                               const pxr::UsdGeomPrimvar &primvar,
                               pxr::UsdTimeCode time,
                               bool is_left_handed);

}  // namespace io::usd
}  // namespace blender
