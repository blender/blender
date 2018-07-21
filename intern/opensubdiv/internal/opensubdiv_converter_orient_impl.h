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

#ifndef OPENSUBDIV_CONVERTER_ORIENT_IMPL_H_
#define OPENSUBDIV_CONVERTER_ORIENT_IMPL_H_

#include "internal/opensubdiv_converter_orient.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace opensubdiv_capi {

inline void reverseFaceVertices(int* face_vertices, const int num_vertices) {
  int last_vert = face_vertices[num_vertices - 1];
  for (int i = num_vertices - 1; i > 0; --i) {
    face_vertices[i] = face_vertices[i - 1];
  }
  face_vertices[0] = last_vert;
}

inline int getLoopWinding(int vert0_of_face, int vert1_of_face) {
  int delta_face = vert1_of_face - vert0_of_face;
  if (abs(delta_face) != 1) {
    if (delta_face > 0) {
      delta_face = -1;
    } else {
      delta_face = 1;
    }
  }
  return delta_face;
}

inline void reverseFaceLoops(
    OpenSubdiv::Far::IndexArray* face_vertices,
    OpenSubdiv::Far::IndexArray* face_edges) {
  const int num_face_vertices = face_vertices->size();
  for (int i = 0; i < num_face_vertices / 2; ++i) {
    const int j = num_face_vertices - i - 1;
    if (i != j) {
      std::swap((*face_vertices)[i], (*face_vertices)[j]);
      std::swap((*face_edges)[i], (*face_edges)[j]);
    }
  }
  reverseFaceVertices(&(*face_vertices)[0], num_face_vertices);
}

}  // namespace opensubdiv_capi

#endif  // OPENSUBDIV_CONVERTER_ORIENT_IMPL_H_
