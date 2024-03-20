/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

struct Mesh;

namespace blender::geometry::boolean {

/** Specifies which solver to use. */
enum class Solver {
  /**
   * The exact solver based on the Mesh Arrangments for Solid Geometry paper,
   * by Zhou, Grinspun, Zorin, and Jacobson.
   */
  MeshArr = 0,
  /** The original BMesh floating point solver. */
  Float = 1,
};

enum class Operation {
  Intersect = 0,
  Union = 1,
  Difference = 2,
};

/**
 * BooleanOpParameters bundles together the global parameters for the boolean operation.
 * As well as saying which particular operation (intersect, difference, union) is desired,
 * it also states some assumptions that the algorithm is allowed to make about the input
 * (e.g., whether or not there are any self intersections).
 */
struct BooleanOpParameters {
  Operation boolean_mode;
  /** Can we assume there are no self-intersections in any of the operands? */
  bool no_self_intersections = true;
  /** Can we assume there are no nested components (e.g., a box inside a box) in any of the
   * components? */
  bool no_nested_components = true;
  /** Can we assume the argument meshes are watertight volume enclosing? */
  bool watertight = true;
};

/**
 * Do a mesh boolean operation directly on meshes.
 * Boolean operations operate on the volumes enclosed by the operands.
 * If is only one operand, the non-float versions will do self-intersection and remove
 * internal faces.
 * If there are more than two meshes, the first mesh is operand 0 and the rest of the
 * meshes are operand 1 (i.e., as if all of operands 1, ... are joined into one mesh.
 * The exact solvers assume that the meshes are PWN (piecewise winding number,
 * which approximately means that the meshes are enclosed watertight voluems,
 * and all edges are manifold, though there are allowable exceptions to that last condition).
 * If the meshes don't sastisfy those conditions, all solvers will try to use ray-shooting
 * to determine whether particular faces survive or not.  This may or may not work
 * in the way the user hopes.
 *
 * \param meshes: The meshes that are operands of the boolean operation.
 * \param transforms: An array of transform matrices used for each mesh's positions.
 * \param target_transform: the result needs to be transformed by this.
 * \param material_remaps: An array of maps from material slot numbers in the corresponding mesh
 * to the material slot in the first mesh. It is OK for material_remaps or any of its constituent
 * arrays to be empty. A -1 value means that the original index should be used with no mapping.
 * \param op_params: Specifies the boolean operation and assumptions we can make.
 * \param solver: which solver to use
 * \param r_intersecting_edges: Vector to store indices of edges on the resulting mesh in. These
 * 'new' edges are the result of the intersections.
 */
Mesh *mesh_boolean(Span<const Mesh *> meshes,
                   Span<float4x4> transforms,
                   const float4x4 &target_transform,
                   Span<Array<short>> material_remaps,
                   BooleanOpParameters op_params,
                   Solver solver,
                   Vector<int> *r_intersecting_edges);

}  // namespace blender::geometry::boolean
