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

#include <carve/polyhedron_base.hpp>

namespace carve {
  namespace csg {
    namespace detail {
      typedef std::map<carve::mesh::MeshSet<3>::vertex_t *,
                       std::set<std::pair<carve::mesh::MeshSet<3>::face_t *, double> > > EdgeIntInfo;

      typedef std::unordered_set<carve::mesh::MeshSet<3>::vertex_t *> VSet;
      typedef std::unordered_set<carve::mesh::MeshSet<3>::face_t *> FSet;

      typedef std::set<carve::mesh::MeshSet<3>::vertex_t *> VSetSmall;
      typedef std::set<csg::V2> V2SetSmall;
      typedef std::set<carve::mesh::MeshSet<3>::face_t *> FSetSmall;

      typedef std::unordered_map<carve::mesh::MeshSet<3>::vertex_t *, VSetSmall> VVSMap;
      typedef std::unordered_map<carve::mesh::MeshSet<3>::edge_t *, EdgeIntInfo> EIntMap;
      typedef std::unordered_map<carve::mesh::MeshSet<3>::face_t *, VSetSmall> FVSMap;

      typedef std::unordered_map<carve::mesh::MeshSet<3>::vertex_t *, FSetSmall> VFSMap;
      typedef std::unordered_map<carve::mesh::MeshSet<3>::face_t *, V2SetSmall> FV2SMap;

      typedef std::unordered_map<
        carve::mesh::MeshSet<3>::edge_t *,
        std::vector<carve::mesh::MeshSet<3>::vertex_t *> > EVVMap;

      typedef std::unordered_map<carve::mesh::MeshSet<3>::vertex_t *,
                                 std::vector<carve::mesh::MeshSet<3>::edge_t *> > VEVecMap;


      class LoopEdges : public std::unordered_map<V2, std::list<FaceLoop *> > {
        typedef std::unordered_map<V2, std::list<FaceLoop *> > super;

      public:
        void addFaceLoop(FaceLoop *fl);
        void sortFaceLoopLists();
        void removeFaceLoop(FaceLoop *fl);
      };

    }
  }
}



static inline std::ostream &operator<<(std::ostream &o, const carve::csg::detail::FSet &s) {
  const char *sep="";
  for (carve::csg::detail::FSet::const_iterator i = s.begin(); i != s.end(); ++i) {
    o << sep << *i; sep=",";
  }
  return o;
}
