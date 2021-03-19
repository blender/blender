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
 * editmesh_intersect.c. */
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
