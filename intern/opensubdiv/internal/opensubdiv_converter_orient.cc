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

#include "internal/opensubdiv_converter_orient.h"

#include "internal/opensubdiv_internal.h"

namespace opensubdiv_capi {

void checkOrientedVertexConnectivity(const int num_vertex_edges,
                                     const int num_vertex_faces,
                                     const int* vertex_edges,
                                     const int* vertex_faces,
                                     const int* dst_vertex_edges,
                                     const int* dst_vertex_faces) {
#ifndef NDEBUG
  for (int i = 0; i < num_vertex_faces; ++i) {
    bool found = false;
    for (int j = 0; j < num_vertex_faces; ++j) {
      if (vertex_faces[i] == dst_vertex_faces[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      assert(!"vert-faces connectivity ruined");
    }
  }
  for (int i = 0; i < num_vertex_edges; ++i) {
    bool found = false;
    for (int j = 0; j < num_vertex_edges; ++j) {
      if (vertex_edges[i] == dst_vertex_edges[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      assert(!"vert-edges connectivity ruined");
    }
  }
#else
  (void) num_vertex_edges;
  (void) num_vertex_faces;
  (void) vertex_edges;
  (void) vertex_faces;
  (void) dst_vertex_edges;
  (void) dst_vertex_faces;
#endif
}

}  // namespace opensubdiv_capi
