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

#include <carve/geom3d.hpp>

#include <carve/vertex_decl.hpp>
#include <carve/edge_decl.hpp>
#include <carve/face_decl.hpp>

#include <stddef.h>

namespace carve {
  namespace poly {



    struct Object {
    };



    template<typename array_t>
    ptrdiff_t ptrToIndex_fast(const array_t &a, const typename array_t::value_type *v) {
      return v - &a[0];
    }

    template<typename array_t>
    ptrdiff_t ptrToIndex(const array_t &a, const typename array_t::value_type *v) {
      if (v < &a.front() || v > &a.back()) return -1;
      return v - &a[0];
    }
    

    template<unsigned ndim>
    struct Geometry : public Object {
      struct Connectivity {
      } connectivity;
    };



    template<>
    struct Geometry<2> : public Object {
      typedef Vertex<2> vertex_t;
      typedef Edge<2> edge_t;

      struct Connectivity {
        std::vector<std::vector<const edge_t *> > vertex_to_edge;
      } connectivity;

      std::vector<vertex_t> vertices;
      std::vector<edge_t> edges;

      ptrdiff_t vertexToIndex_fast(const vertex_t *v) const { return ptrToIndex_fast(vertices, v); }
      ptrdiff_t vertexToIndex(const vertex_t *v) const { return ptrToIndex(vertices, v); }

      ptrdiff_t edgeToIndex_fast(const edge_t *e) const { return ptrToIndex_fast(edges, e); }
      ptrdiff_t edgeToIndex(const edge_t *e) const { return ptrToIndex(edges, e); }



      // *** connectivity queries

      template<typename T>
      int vertexToEdges(const vertex_t *v, T result) const;
    };



    template<>
    struct Geometry<3> : public Object {
      typedef Vertex<3> vertex_t;
      typedef Edge<3> edge_t;
      typedef Face<3> face_t;

      struct Connectivity {
        std::vector<std::vector<const edge_t *> > vertex_to_edge;
        std::vector<std::vector<const face_t *> > vertex_to_face;
        std::vector<std::vector<const face_t *> > edge_to_face;
      } connectivity;

      std::vector<vertex_t> vertices;
      std::vector<edge_t> edges;
      std::vector<face_t> faces;

      ptrdiff_t vertexToIndex_fast(const vertex_t *v) const { return ptrToIndex_fast(vertices, v); }
      ptrdiff_t vertexToIndex(const vertex_t *v) const { return ptrToIndex(vertices, v); }

      ptrdiff_t edgeToIndex_fast(const edge_t *e) const { return ptrToIndex_fast(edges, e); }
      ptrdiff_t edgeToIndex(const edge_t *e) const { return ptrToIndex(edges, e); }

      ptrdiff_t faceToIndex_fast(const face_t *f) const { return ptrToIndex_fast(faces, f); }
      ptrdiff_t faceToIndex(const face_t *f) const { return ptrToIndex(faces, f); }

      template<typename order_t>
      bool orderVertices(order_t order);

      bool orderVertices() { return orderVertices(std::less<vertex_t::vector_t>()); }



      // *** connectivity queries

      const face_t *connectedFace(const face_t *, const edge_t *) const;

      template<typename T>
      int _faceNeighbourhood(const face_t *f, int depth, T *result) const;

      template<typename T>
      int faceNeighbourhood(const face_t *f, int depth, T result) const;

      template<typename T>
      int faceNeighbourhood(const edge_t *e, int m_id, int depth, T result) const;

      template<typename T>
      int faceNeighbourhood(const vertex_t *v, int m_id, int depth, T result) const;

      template<typename T>
      int vertexToEdges(const vertex_t *v, T result) const;

      template<typename T>
      int edgeToFaces(const edge_t *e, T result) const;

      template<typename T>
      int vertexToFaces(const vertex_t *v, T result) const;
    };



  }
}
