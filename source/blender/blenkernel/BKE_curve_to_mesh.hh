/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

struct CurveEval;
struct Mesh;

/** \file
 * \ingroup bke
 */

namespace blender::bke {

/**
 * Extrude all splines in the profile curve along the path of every spline in the curve input.
 * Transfer curve attributes to the mesh.
 *
 * \note Normal calculation is by far the slowest part of calculations relating to the result mesh.
 * Although it would be a sensible decision to use the better topology information available while
 * generating the mesh to also generate the normals, that work may wasted if the output mesh is
 * changed anyway in a way that affects the normals. So currently this code uses the safer /
 * simpler solution of deferring normal calculation to the rest of Blender.
 */
Mesh *curve_to_mesh_sweep(const CurveEval &curve, const CurveEval &profile, bool fill_caps);
/**
 * Create a loose-edge mesh based on the evaluated path of the curve's splines.
 * Transfer curve attributes to the mesh.
 */
Mesh *curve_to_wire_mesh(const CurveEval &curve);

}  // namespace blender::bke
