
// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <carve/carve.hpp>

#include <carve/mesh.hpp>

namespace carve {
  namespace csg {

    typedef std::pair<
      carve::mesh::MeshSet<3>::vertex_t *,
      carve::mesh::MeshSet<3>::vertex_t *> V2;

    typedef std::pair<
      carve::mesh::MeshSet<3>::face_t *,
      carve::mesh::MeshSet<3>::face_t *> F2;

    static inline V2 ordered_edge(
      carve::mesh::MeshSet<3>::vertex_t *a,
      carve::mesh::MeshSet<3>::vertex_t *b) {
      return V2(std::min(a, b), std::max(a, b));
    }

    static inline V2 flip(const V2 &v) {
      return V2(v.second, v.first);
    }

    // include/carve/csg.hpp include/carve/faceloop.hpp
    // lib/intersect.cpp lib/intersect_classify_common_impl.hpp
    // lib/intersect_classify_edge.cpp
    // lib/intersect_classify_group.cpp
    // lib/intersect_classify_simple.cpp
    // lib/intersect_face_division.cpp lib/intersect_group.cpp
    // lib/intersect_half_classify_group.cpp
    typedef std::unordered_set<V2> V2Set;

    // include/carve/csg.hpp include/carve/polyhedron_decl.hpp
    // lib/csg_collector.cpp lib/intersect.cpp
    // lib/intersect_common.hpp lib/intersect_face_division.cpp
    // lib/polyhedron.cpp
    typedef std::unordered_map<
      carve::mesh::MeshSet<3>::vertex_t *,
      carve::mesh::MeshSet<3>::vertex_t *> VVMap;
  }
}
