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
    class Edge;



    template<unsigned ndim>
    struct p2_adapt_project {
      typedef carve::geom2d::P2 (*proj_t)(const carve::geom::vector<ndim> &);
      proj_t proj;
      p2_adapt_project(proj_t _proj) : proj(_proj) { }
      carve::geom2d::P2 operator()(const carve::geom::vector<ndim> &v) const { return proj(v); }
      carve::geom2d::P2 operator()(const carve::geom::vector<ndim> *v) const { return proj(*v); }
      carve::geom2d::P2 operator()(const Vertex<ndim> &v) const { return proj(v.v); }
      carve::geom2d::P2 operator()(const Vertex<ndim> *v) const { return proj(v->v); }
    };


    template<unsigned ndim>
    class Face : public tagable {
    public:
      typedef Vertex<ndim> vertex_t;
      typedef typename Vertex<ndim>::vector_t vector_t;
      typedef Edge<ndim> edge_t;
      typedef Object obj_t;
      typedef carve::geom::aabb<ndim> aabb_t;
      typedef carve::geom::plane<ndim> plane_t;

      typedef carve::geom2d::P2 (*project_t)(const vector_t &);
      typedef vector_t (*unproject_t)(const carve::geom2d::P2 &, const plane_t &);

    protected:
      std::vector<const vertex_t *> vertices; // pointer into polyhedron.vertices
      std::vector<const edge_t *> edges; // pointer into polyhedron.edges

      project_t getProjector(bool positive_facing, int axis);
      unproject_t getUnprojector(bool positive_facing, int axis);

    public:
      typedef typename std::vector<const vertex_t *>::iterator vertex_iter_t;
      typedef typename std::vector<const vertex_t *>::const_iterator const_vertex_iter_t;

      typedef typename std::vector<const edge_t *>::iterator edge_iter_t;
      typedef typename std::vector<const edge_t *>::const_iterator const_edge_iter_t;

      obj_t *owner;

      aabb_t aabb;
      plane_t plane_eqn;
      int manifold_id;
      int group_id;

      project_t project;
      unproject_t unproject;

      Face(const std::vector<const vertex_t *> &_vertices, bool delay_recalc = false);
      Face(const vertex_t *v1, const vertex_t *v2, const vertex_t *v3, bool delay_recalc = false);
      Face(const vertex_t *v1, const vertex_t *v2, const vertex_t *v3, const vertex_t *v4, bool delay_recalc = false);

      template <typename iter_t>
      Face(const Face *base, iter_t vbegin, iter_t vend, bool flipped) {
        init(base, vbegin, vend, flipped);
      }

      Face(const Face *base, const std::vector<const vertex_t *> &_vertices, bool flipped) {
        init(base, _vertices, flipped);
      }

      Face() {}
      ~Face() {}

      bool recalc();

      template<typename iter_t>
      Face *init(const Face *base, iter_t vbegin, iter_t vend, bool flipped);
      Face *init(const Face *base, const std::vector<const vertex_t *> &_vertices, bool flipped);

      template<typename iter_t>
      Face *create(iter_t vbegin, iter_t vend, bool flipped) const;
      Face *create(const std::vector<const vertex_t *> &_vertices, bool flipped) const;

      Face *clone(bool flipped = false) const;
      void invert();

      void getVertexLoop(std::vector<const vertex_t *> &loop) const;

      const vertex_t *&vertex(size_t idx);
      const vertex_t *vertex(size_t idx) const;
      size_t nVertices() const;

      vertex_iter_t vbegin() { return vertices.begin(); }
      vertex_iter_t vend() { return vertices.end(); }
      const_vertex_iter_t vbegin() const { return vertices.begin(); }
      const_vertex_iter_t vend() const { return vertices.end(); }

      std::vector<carve::geom::vector<2> > projectedVertices() const;

      const edge_t *&edge(size_t idx);
      const edge_t *edge(size_t idx) const;
      size_t nEdges() const;

      edge_iter_t ebegin() { return edges.begin(); }
      edge_iter_t eend() { return edges.end(); }
      const_edge_iter_t ebegin() const { return edges.begin(); }
      const_edge_iter_t eend() const { return edges.end(); }

      bool containsPoint(const vector_t &p) const;
      bool containsPointInProjection(const vector_t &p) const;
      bool simpleLineSegmentIntersection(const carve::geom::linesegment<ndim> &line,
                                         vector_t &intersection) const;
      IntersectionClass lineSegmentIntersection(const carve::geom::linesegment<ndim> &line,
                                                vector_t &intersection) const;
      vector_t centroid() const;

      p2_adapt_project<ndim> projector() const {
        return p2_adapt_project<ndim>(project);
      }

      void swap(Face<ndim> &other);
    };



    struct hash_face_ptr {
      template<unsigned ndim>
      size_t operator()(const Face<ndim> * const &f) const {
        return (size_t)f;
      }
    };



    namespace face {



      template<unsigned ndim>
      static inline carve::geom2d::P2 project(const Face<ndim> *f, const typename Face<ndim>::vector_t &v) {
        return f->project(v);
      }



      template<unsigned ndim>
      static inline carve::geom2d::P2 project(const Face<ndim> &f, const typename Face<ndim>::vector_t &v) {
        return f.project(v);
      }



      template<unsigned ndim>
      static inline typename Face<ndim>::vector_t unproject(const Face<ndim> *f, const carve::geom2d::P2 &p) {
        return f->unproject(p, f->plane_eqn);
      }



      template<unsigned ndim>
      static inline typename Face<ndim>::vector_t unproject(const Face<ndim> &f, const carve::geom2d::P2 &p) {
        return f.unproject(p, f.plane_eqn);
      }



    }



  }
}
