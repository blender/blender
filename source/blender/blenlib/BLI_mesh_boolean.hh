/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

/* The boolean functions in Blenlib use exact arithmetic, so require GMP. */
#ifdef WITH_GMP

#  include "BLI_mesh_intersect.hh"
#  include <functional>

namespace blender::meshintersect {

/**
 * Enum values after BOOLEAN_NONE need to match BMESH_ISECT_BOOLEAN_... values in
 * `editmesh_intersect.cc`.
 */
enum class BoolOpType {
  None = -1,
  /* Aligned with #BooleanModifierOp. */
  Intersect = 0,
  Union = 1,
  Difference = 2,
};

/**
 * Do the boolean operation op on the mesh pm_in.
 * The boolean operation has \a nshapes input shapes. Each is a disjoint subset of the input mesh.
 * The shape_fn argument, when applied to an input face argument, says which shape it is in
 * (should be a value from -1 to `nshapes - 1`: if -1, it is not part of any shape).
 * The use_self argument says whether or not the function should assume that faces in the
 * same shape intersect - if the argument is true, such self-intersections will be found.
 * Sometimes the caller has already done a triangulation of the faces,
 * and if so, *pm_triangulated contains a triangulation: if non-null, it contains a mesh
 * of triangles, each of whose orig_field says which face in pm that triangle belongs to.
 * pm argument isn't `const` because we may populate its verts (for debugging).
 * Same goes for the pm_triangulated argument.
 * The output #IMesh will have faces whose orig fields map back to faces and edges in
 * the input mesh.
 */
IMesh boolean_mesh(IMesh &imesh,
                   BoolOpType op,
                   int nshapes,
                   std::function<int(int)> shape_fn,
                   bool use_self,
                   bool hole_tolerant,
                   IMesh *imesh_triangulated,
                   IMeshArena *arena);

/**
 * This is like boolean, but operates on #IMesh's whose faces are all triangles.
 * It is exposed mainly for unit testing, at the moment: boolean_mesh() uses
 * it to do most of its work.
 */
IMesh boolean_trimesh(IMesh &tm_in,
                      BoolOpType op,
                      int nshapes,
                      std::function<int(int)> shape_fn,
                      bool use_self,
                      bool hole_tolerant,
                      IMeshArena *arena);

}  // namespace blender::meshintersect

#endif /* WITH_GMP */
