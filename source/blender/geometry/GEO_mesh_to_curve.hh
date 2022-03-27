/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"

#pragma once

struct Mesh;
struct Curves;
class MeshComponent;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Convert the mesh into one or many poly splines. Since splines cannot have branches,
 * intersections of more than three edges will become breaks in splines. Attributes that
 * are not built-in on meshes and not curves are transferred to the result curve.
 */
Curves *mesh_to_curve_convert(const MeshComponent &mesh_component, const IndexMask selection);

}  // namespace blender::geometry
