/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This header file contains a C++ interface to a 3D mesh inset algorithm
 * which is based on a 2D Straight Skeleton construction.
 */

#include "BLI_array.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"
#include "BLI_math_vector_types.hh"


namespace blender::meshinset {

/*
 * This is the library interface to a function that can inset
 * contours (closed sequences of vertices) of a 3D mesh.
 * For generality, the mesh is specified by #Span of faces,
 * where each face has the sequence of vertex indices that
 * are traversed in CCW order to form the face.
 * The indices given the position in a #Span of #float3 entries,
 * which are 3D coordinates.
 *
 * An "inset" of a contour by a given amount is conceptually
 * formed as follows: offset each edge of the contour on its left
 * side by the specified amount, shortening and joining up each
 * offset edge with its neighbor offset edges. If the contour
 * forms a face, this is typically known as a "face inset".
 * However, that conceptual description fails to describe what
 * to do if an offset edge shortens so much that it vanishes,
 * or if advancing intersection points of offset edges collide
 * into offset edges from another part of the contour (or another
 * contour).
 *
 * An algorithm called the "Straight Skeleton Algorithm"
 * (see https://wikipedia.org/wiki/Straight_skeleton)
 * deals with such complications, and is what is used in this
 * library routine. That algorithm regards each edge of the
 * contour as a wavefront that advances at a constant speed,
 * dealing with topological changes as wavefront edges collapse
 * or crash into opposite ones. The Straight Skeleton is what
 * remains if you advance the wavefronts as far as they can go,
 * but we can stop at any particular amount of advancement to
 * achieve an inset by that amount.
 *
 * However, the Straight Skeleton Algorithm is a 2D algorithm,
 * doesn't deal with internal geometry. This library function
 * is adapted to work in 3D and "flow over" internal geometry
 * as the wavefronts advance.
 *
 * Also, an extra feature of this library is to allow the advancing
 * wavefronts to raise (along face normals) at a given slope.
 * Users like this as an option to a "face inset" function.
 *
 * Usage:
 * Populate a #MeshInset_Input structure with the mesh
 * (vertex coordinates and faces), the contours to inset
 * (vertex indices forming closed loops to inset),
 * and the amount to inset and the slope.
 * Pass this to #mesh_inset_calc, and receive a #MeshInset_Result
 * as output.
 * The #MeshInset_Result has a new mesh, also give by vertex
 * coordinates and faces. It also has some data to help understand
 * how to map the output back to the input:
 * TODO: Document the extras when this interface finally settles down.
 */

/** #MeshInset_Input is the input structure for #mesh_inset_calc. */
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

/** #MeshInset_Result is the output structure for #mesh_inset_calc. */
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

/**
 * Calculate a mesh inset -- the offset of a set of contours, dealing with collisions.
 *
 * \param input: a #MeshInset_Input containing a mesh, contours to offet, and offset parameters.
 * \return a #MeshInset_Result giving a new mesh and data to relate the output to the input.
 */
MeshInset_Result mesh_inset_calc(const MeshInset_Input &input);

}  // namespace blender::meshinset
