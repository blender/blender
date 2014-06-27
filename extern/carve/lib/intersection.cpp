// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <algorithm>

#include <carve/carve.hpp>
#include <carve/poly.hpp>
#include <carve/timing.hpp>
#include <carve/intersection.hpp>



void carve::csg::Intersections::collect(const IObj &obj,
                                        std::vector<carve::mesh::MeshSet<3>::vertex_t *> *collect_v,
                                        std::vector<carve::mesh::MeshSet<3>::edge_t *> *collect_e,
                                        std::vector<carve::mesh::MeshSet<3>::face_t *> *collect_f) const {
  carve::csg::Intersections::const_iterator i = find(obj);
  if (i != end()) {
    Intersections::mapped_type::const_iterator a, b;
    for (a = (*i).second.begin(), b = (*i).second.end(); a != b; ++a) {
      switch ((*a).first.obtype) {
      case carve::csg::IObj::OBTYPE_VERTEX:
        if (collect_v) collect_v->push_back((*a).first.vertex);
        break;
      case carve::csg::IObj::OBTYPE_EDGE:
        if (collect_e) collect_e->push_back((*a).first.edge);
        break;
      case carve::csg::IObj::OBTYPE_FACE:
        if (collect_f) collect_f->push_back((*a).first.face);
        break;
      default:
        throw carve::exception("should not happen " __FILE__ ":" XSTR(__LINE__));
      }
    }
  }
}



bool carve::csg::Intersections::intersectsFace(carve::mesh::MeshSet<3>::vertex_t *v,
                                               carve::mesh::MeshSet<3>::face_t *f) const {
  const_iterator i = find(v);
  if (i != end()) {
    mapped_type::const_iterator a, b;

    for (a = (*i).second.begin(), b = (*i).second.end(); a != b; ++a) {
      switch ((*a).first.obtype) {
      case IObj::OBTYPE_VERTEX: {
        const carve::mesh::MeshSet<3>::edge_t *edge = f->edge;
        do {
          if (edge->vert == (*a).first.vertex) return true;
          edge = edge->next;
        } while (edge != f->edge);
        break;
      }
      case carve::csg::IObj::OBTYPE_EDGE: {
        const carve::mesh::MeshSet<3>::edge_t *edge = f->edge;
        do {
          if (edge == (*a).first.edge) return true;
          edge = edge->next;
        } while (edge != f->edge);
        break;
      }
      case carve::csg::IObj::OBTYPE_FACE: {
        if ((*a).first.face == f) return true;
        break;
      }
      default:
        throw carve::exception("should not happen " __FILE__ ":" XSTR(__LINE__));
      }
    }
  }
  return false;
}
