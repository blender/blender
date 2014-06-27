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


#pragma once

#include <carve/carve.hpp>

#include <carve/geom2d.hpp>
#include <carve/vector.hpp>
#include <carve/matrix.hpp>
#include <carve/geom3d.hpp>
#include <carve/aabb.hpp>
#include <carve/tag.hpp>

#include <vector>
#include <list>
#include <map>

namespace carve {
  namespace poly {



    struct Object;



    template<unsigned ndim>
    class Vertex : public tagable {
    public:
      typedef carve::geom::vector<ndim> vector_t;
      typedef Object obj_t;

      vector_t v;
      obj_t *owner;

      Vertex() : tagable(), v() {
      }

      ~Vertex() {
      }

      Vertex(const vector_t &_v) : tagable(), v(_v) {
      }
    };



    struct hash_vertex_ptr {
      template<unsigned ndim>
      size_t operator()(const Vertex<ndim> * const &v) const {
        return (size_t)v;
      }

      template<unsigned ndim>
      size_t operator()(const std::pair<const Vertex<ndim> *, const Vertex<ndim> *> &v) const {
        size_t r = (size_t)v.first;
        size_t s = (size_t)v.second;
        return r ^ ((s >> 16) | (s << 16));
      }

    };



    template<unsigned ndim>
    double distance(const Vertex<ndim> *v1, const Vertex<ndim> *v2) {
      return distance(v1->v, v2->v);
    }

    template<unsigned ndim>
    double distance(const Vertex<ndim> &v1, const Vertex<ndim> &v2) {
      return distance(v1.v, v2.v);
    }

    struct vec_adapt_vertex_ref {
      template<unsigned ndim>
      const typename Vertex<ndim>::vector_t &operator()(const Vertex<ndim> &v) const { return v.v; }

      template<unsigned ndim>
      typename Vertex<ndim>::vector_t &operator()(Vertex<ndim> &v) const { return v.v; }
    };



    struct vec_adapt_vertex_ptr {
      template<unsigned ndim>
      const typename Vertex<ndim>::vector_t &operator()(const Vertex<ndim> *v) const { return v->v; }

      template<unsigned ndim>
      typename Vertex<ndim>::vector_t &operator()(Vertex<ndim> *v) const { return v->v; }
    };



  }
}
