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

#include <carve/vector.hpp>
#include <carve/tag.hpp>

#include <vector>
#include <list>

namespace carve {
  namespace poly {



    struct Object;



    template<unsigned ndim>
    class Edge : public tagable {
    public:
      typedef Vertex<ndim> vertex_t;
      typedef typename Vertex<ndim>::vector_t vector_t;
      typedef Object obj_t;

      const vertex_t *v1, *v2;
      const obj_t *owner;

      Edge(const vertex_t *_v1, const vertex_t *_v2, const obj_t *_owner) :
        tagable(), v1(_v1), v2(_v2), owner(_owner) {
      }

      ~Edge() {
      }
    };



    struct hash_edge_ptr {
      template<unsigned ndim>
      size_t operator()(const Edge<ndim> * const &e) const {
        return (size_t)e;
      }
    };



  }
}

