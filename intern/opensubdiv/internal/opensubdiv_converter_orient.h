// Copyright 2018 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_CONVERTER_ORIENT_H_
#define OPENSUBDIV_CONVERTER_ORIENT_H_

#include <opensubdiv/far/types.h>

// Set of utility functions which are needed to bring topology to an orientation
// (or, winding, if you wish) which OpenSubdiv expects.

namespace opensubdiv_capi {

inline void reverseFaceVertices(int* face_vertices, const int num_vertices);

// TODO(sergey): Document which value corresponds to which winding.
inline int getLoopWinding(int vert0_of_face, int vert1_of_face);

inline void reverseFaceLoops(
    OpenSubdiv::Far::IndexArray* face_vertices,
    OpenSubdiv::Far::IndexArray* face_edges);

// Used for debugging, checks whether orientation happened correct.
void checkOrientedVertexConnectivity(const int num_vertex_edges,
                                     const int num_vertex_faces,
                                     const int* vertex_edges,
                                     const int* vertex_faces,
                                     const int* dst_vertex_edges,
                                     const int* dst_vertex_faces);

}  // namespace opensubdiv_capi

#endif  // OPENSUBDIV_CONVERTER_ORIENT_H_

#include "internal/opensubdiv_converter_orient_impl.h"
