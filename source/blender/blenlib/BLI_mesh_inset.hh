/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This header file contains a C++ interface to a 3D mesh inset algorithm
 * which is based on a 2D Straight Skeleton construction.
 */

#include "BLI_array.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"


namespace blender::meshinset {

class MeshInset_Input {
public:
  /** The vertices. Can be a superset of the needed vertices. */
  Span<float3> vert;
  /** The faces, each a CCW ordering of vertex indices. */
  Span<Vector<int>> face;
  /** The contours to inset; ints are vert indices; contour is on left side of implied edges. */
  Span<Vector<int>> contour;
  float inset_amount;
  float slope;
  bool need_ids;
};

class MeshInset_Result {
public:
  /** The output vertices. A subset (perhaps) of input vertices, plus some new ones. */
  Array<float3> vert;
  /** The output faces, each a CCW ordering of the output vertices. */
  Array<Vector<int>> face;
  /** The output contours -- where the input contours ended up. */
  Array<Vector<int>> contour;
  /** Maps output vertex indices to input vertex indices, -1 if there is none. */
  Array<int> orig_vert;
  /** Maps output faces tot input faces that they were part of. */
  Array<int> orig_face;
};

MeshInset_Result mesh_inset_calc(const MeshInset_Input &input);

}  // namespace blender::meshinset
