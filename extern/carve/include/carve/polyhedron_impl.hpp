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

#include <carve/timing.hpp>

#include <assert.h>
#include <list>

namespace carve {
  namespace poly {



    template<typename order_t>
    struct VPtrSort {
      order_t order;

      VPtrSort(const order_t &_order) : order(_order) {}
      bool operator()(carve::poly::Polyhedron::vertex_t const *a,
                      carve::poly::Polyhedron::vertex_t const *b) const {
        return order(a->v, b->v);
      }
    };

    template<typename order_t>
    bool Geometry<3>::orderVertices(order_t order) {
      static carve::TimingName FUNC_NAME("Geometry<3>::orderVertices()");
      carve::TimingBlock block(FUNC_NAME);

      std::vector<vertex_t *> vptr;
      std::vector<vertex_t *> vmap;
      std::vector<vertex_t> vout;
      const size_t N = vertices.size();

      vptr.reserve(N);
      vout.reserve(N);
      vmap.resize(N);

      for (size_t i = 0; i != N; ++i) {
        vptr.push_back(&vertices[i]);
      }
      std::sort(vptr.begin(), vptr.end(), VPtrSort<order_t>(order));

      for (size_t i = 0; i != N; ++i) {
        vout.push_back(*vptr[i]);
        vmap[(size_t)vertexToIndex_fast(vptr[i])] = &vout[i];
      }

      for (size_t i = 0; i < faces.size(); ++i) {
        face_t &f = faces[i];
        for (size_t j = 0; j < f.nVertices(); ++j) {
          f.vertex(j) = vmap[(size_t)vertexToIndex_fast(f.vertex(j))];
        }
      }
      for (size_t i = 0; i < edges.size(); ++i) {
        edges[i].v1 = vmap[(size_t)vertexToIndex_fast(edges[i].v1)];
        edges[i].v2 = vmap[(size_t)vertexToIndex_fast(edges[i].v2)];
      }

      vout.swap(vertices);

      return true;
    }



    template<typename T>
    int Geometry<3>::_faceNeighbourhood(const face_t *f, int depth, T *result) const {
      if (depth < 0 || f->is_tagged()) return 0;

      f->tag();
      *(*result)++ = f;

      int r = 1;
      for (size_t i = 0; i < f->nEdges(); ++i) {
        const face_t *f2 = connectedFace(f, f->edge(i));
        if (f2) {
          r += _faceNeighbourhood(f2, depth - 1, (*result));
        }
      }
      return r;
    }



    template<typename T>
    int Geometry<3>::faceNeighbourhood(const face_t *f, int depth, T result) const {
      tagable::tag_begin();

      return _faceNeighbourhood(f, depth, &result);
    }



    template<typename T>
    int Geometry<3>::faceNeighbourhood(const edge_t *e, int m_id, int depth, T result) const {
      tagable::tag_begin();

      int r = 0;
      const std::vector<const face_t *> &edge_faces = connectivity.edge_to_face[(size_t)edgeToIndex_fast(e)];
      for (size_t i = 0; i < edge_faces.size(); ++i) {
        const face_t *f = edge_faces[i];
        if (f && f->manifold_id == m_id) { r += _faceNeighbourhood(f, depth, &result); }
      }
      return r;
    }



    template<typename T>
    int Geometry<3>::faceNeighbourhood(const vertex_t *v, int m_id, int depth, T result) const {
      tagable::tag_begin();

      int r = 0;
      const std::vector<const face_t *> &vertex_faces = connectivity.vertex_to_face[(size_t)vertexToIndex_fast(v)];
      for (size_t i = 0; i < vertex_faces.size(); ++i) {
        const face_t *f = vertex_faces[i];
        if (f && f->manifold_id == m_id) { r += _faceNeighbourhood(f, depth, &result); }
      }
      return r;
    }



    // accessing connectivity information.
    template<typename T>
    int Geometry<3>::vertexToEdges(const vertex_t *v, T result) const {
      const std::vector<const edge_t *> &e = connectivity.vertex_to_edge[(size_t)vertexToIndex_fast(v)];
      std::copy(e.begin(), e.end(), result);
      return e.size();
    }



    template<typename T>
    int Geometry<3>::vertexToFaces(const vertex_t *v, T result) const {
      const std::vector<const face_t *> &vertex_faces = connectivity.vertex_to_face[(size_t)vertexToIndex_fast(v)];
      int c = 0;
      for (size_t i = 0; i < vertex_faces.size(); ++i) {
        *result++ = vertex_faces[i]; ++c;
      }
      return c;
    }



    template<typename T>
    int Geometry<3>::edgeToFaces(const edge_t *e, T result) const {
      const std::vector<const face_t *> &edge_faces = connectivity.edge_to_face[(size_t)edgeToIndex_fast(e)];
      int c = 0;
      for (size_t i = 0; i < edge_faces.size(); ++i) {
        if (edge_faces[i] != NULL) { *result++ = edge_faces[i]; ++c; }
      }
      return c;
    }



    inline const Geometry<3>::face_t *Geometry<3>::connectedFace(const face_t *f, const edge_t *e) const {
      const std::vector<const face_t *> &edge_faces = connectivity.edge_to_face[(size_t)edgeToIndex_fast(e)];
      for (size_t i = 0; i < (edge_faces.size() & ~1U); i++) {
        if (edge_faces[i] == f) return edge_faces[i^1];
      }
      return NULL;
    }



    inline void Polyhedron::invert(int m_id) {
      std::vector<bool> selected_manifolds(manifold_is_closed.size(), false);
      if (m_id >=0 && (size_t)m_id < selected_manifolds.size()) selected_manifolds[(size_t)m_id] = true;
      invert(selected_manifolds);
    }


    
    inline void Polyhedron::invert() {
      invertAll();
    }



    inline bool Polyhedron::edgeOnManifold(const edge_t *e, int m_id) const {
      const std::vector<const face_t *> &edge_faces = connectivity.edge_to_face[(size_t)edgeToIndex_fast(e)];

      for (size_t i = 0; i < edge_faces.size(); ++i) {
        if (edge_faces[i] && edge_faces[i]->manifold_id == m_id) return true;
      }
      return false;
    }

    inline bool Polyhedron::vertexOnManifold(const vertex_t *v, int m_id) const {
      const std::vector<const face_t *> &f = connectivity.vertex_to_face[(size_t)vertexToIndex_fast(v)];

      for (size_t i = 0; i < f.size(); ++i) {
        if (f[i]->manifold_id == m_id) return true;
      }
      return false;
    }



    template<typename T>
    int Polyhedron::edgeManifolds(const edge_t *e, T result) const {
      const std::vector<const face_t *> &edge_faces = connectivity.edge_to_face[(size_t)edgeToIndex_fast(e)];

      for (size_t i = 0; i < (edge_faces.size() & ~1U); i += 2) {
        const face_t *f1 = edge_faces[i];
        const face_t *f2 = edge_faces[i+1];
        assert (f1 || f2);
        if (f1)
          *result++ = f1->manifold_id;
        else if (f2)
          *result++ = f2->manifold_id;
      }
      return (int)(edge_faces.size() >> 1);
    }



    template<typename T>
    int Polyhedron::vertexManifolds(const vertex_t *v, T result) const {
      const std::vector<const face_t *> &f = connectivity.vertex_to_face[(size_t)vertexToIndex_fast(v)];
      std::set<int> em;

      for (size_t i = 0; i < f.size(); ++i) {
        em.insert(f[i]->manifold_id);
      }

      std::copy(em.begin(), em.end(), result);
      return em.size();
    }



    template<typename T>
    void Polyhedron::transform(const T &xform) {
      for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].v = xform(vertices[i].v);
      }
      faceRecalc();
      init();
    }



    inline size_t Polyhedron::manifoldCount() const {
      return manifold_is_closed.size();
    }



    inline bool Polyhedron::hasOpenManifolds() const {
      for (size_t i = 0; i < manifold_is_closed.size(); ++i) {
        if (!manifold_is_closed[i]) return true;
      }
      return false;
    }



    inline std::ostream &operator<<(std::ostream &o, const Polyhedron &p) {
      p.print(o);
      return o;
    }



  }
}
