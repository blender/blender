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


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/csg.hpp>
#include "csg_detail.hpp"


const char *carve::csg::ENUM(carve::csg::FaceClass f) {
  if (f == FACE_ON_ORIENT_OUT) return "FACE_ON_ORIENT_OUT";
  if (f == FACE_OUT) return "FACE_OUT";
  if (f == FACE_IN) return "FACE_IN";
  if (f == FACE_ON_ORIENT_IN) return "FACE_ON_ORIENT_IN";
  return "???";
}



const char *carve::csg::ENUM(carve::PointClass p) {
  if (p == POINT_UNK) return "POINT_UNK";
  if (p == POINT_OUT) return "POINT_OUT";
  if (p == POINT_ON) return "POINT_ON";
  if (p == POINT_IN) return "POINT_IN";
  if (p == POINT_VERTEX) return "POINT_VERTEX";
  if (p == POINT_EDGE) return "POINT_EDGE";
  return "???";
}



void carve::csg::detail::LoopEdges::addFaceLoop(FaceLoop *fl) {
  carve::mesh::MeshSet<3>::vertex_t *v1, *v2;
  v1 = fl->vertices[fl->vertices.size() - 1];
  for (unsigned j = 0; j < fl->vertices.size(); ++j) {
    v2 = fl->vertices[j];
    (*this)[std::make_pair(v1, v2)].push_back(fl);
    v1 = v2;
  }
}



void carve::csg::detail::LoopEdges::sortFaceLoopLists() {
  for (super::iterator i = begin(), e = end(); i != e; ++i) {
    (*i).second.sort();
  }
}



void carve::csg::detail::LoopEdges::removeFaceLoop(FaceLoop *fl) {
  carve::mesh::MeshSet<3>::vertex_t *v1, *v2;
  v1 = fl->vertices[fl->vertices.size() - 1];
  for (unsigned j = 0; j < fl->vertices.size(); ++j) {
    v2 = fl->vertices[j];
    iterator l(find(std::make_pair(v1, v2)));
    if (l != end()) {
      (*l).second.remove(fl);
      if (!(*l).second.size()) {
        erase(l);
      }
    }
    v1 = v2;
  }
}



carve::csg::FaceClass carve::csg::FaceLoopGroup::classificationAgainst(const carve::mesh::MeshSet<3>::mesh_t *mesh) const {
  for (std::list<ClassificationInfo>::const_iterator i = classification.begin(); i != classification.end(); ++i) {
    if ((*i).intersected_mesh == mesh) {
      return (*i).classification;
    }
  }
  return FACE_UNCLASSIFIED;
}
