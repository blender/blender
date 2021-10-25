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

namespace std {
  template<unsigned ndim>
  inline void swap(carve::poly::Face<ndim> &a, carve::poly::Face<ndim> &b) {
    a.swap(b);
  }
}

namespace carve {
  namespace poly {
    template<unsigned ndim>
    void Face<ndim>::swap(Face<ndim> &other) {
      std::swap(vertices,    other.vertices);
      std::swap(edges,       other.edges);
      std::swap(owner,       other.owner);
      std::swap(aabb,        other.aabb);
      std::swap(plane_eqn,   other.plane_eqn);
      std::swap(manifold_id, other.manifold_id);
      std::swap(group_id,    other.group_id);
      std::swap(project,     other.project);
      std::swap(unproject,   other.unproject);
    }

    template<unsigned ndim>
    template<typename iter_t>
    Face<ndim> *Face<ndim>::init(const Face<ndim> *base, iter_t vbegin, iter_t vend, bool flipped) {
      CARVE_ASSERT(vbegin < vend);

      vertices.reserve((size_t)std::distance(vbegin, vend));

      if (flipped) {
        std::reverse_copy(vbegin, vend, std::back_inserter(vertices));
        plane_eqn = -base->plane_eqn;
      } else {
        std::copy(vbegin, vend, std::back_inserter(vertices));
        plane_eqn = base->plane_eqn;
      }

      edges.clear();
      edges.resize(nVertices(), NULL);

      aabb.fit(vertices.begin(), vertices.end(), vec_adapt_vertex_ptr());
      untag();

      int da = carve::geom::largestAxis(plane_eqn.N);

      project = getProjector(plane_eqn.N.v[da] > 0, da);
      unproject = getUnprojector(plane_eqn.N.v[da] > 0, da);

      return this;
    }

    template<unsigned ndim>
    template<typename iter_t>
    Face<ndim> *Face<ndim>::create(iter_t vbegin, iter_t vend, bool flipped) const {
      return (new Face)->init(this, vbegin, vend, flipped);
    }

    template<unsigned ndim>
    Face<ndim> *Face<ndim>::create(const std::vector<const vertex_t *> &_vertices, bool flipped) const {
      return (new Face)->init(this, _vertices.begin(), _vertices.end(), flipped);
    }

    template<unsigned ndim>
    Face<ndim> *Face<ndim>::clone(bool flipped) const {
      return (new Face)->init(this, vertices, flipped);
    }

    template<unsigned ndim>
    void Face<ndim>::getVertexLoop(std::vector<const vertex_t *> &loop) const {
      loop.resize(nVertices(), NULL);
      std::copy(vbegin(), vend(), loop.begin());
    }

    template<unsigned ndim>
    const typename Face<ndim>::edge_t *&Face<ndim>::edge(size_t idx) {
      return edges[idx];
    }

    template<unsigned ndim>
    const typename Face<ndim>::edge_t *Face<ndim>::edge(size_t idx) const {
      return edges[idx];
    }

    template<unsigned ndim>
    size_t Face<ndim>::nEdges() const {
      return edges.size();
    }

    template<unsigned ndim>
    const typename Face<ndim>::vertex_t *&Face<ndim>::vertex(size_t idx) {
      return vertices[idx];
    }

    template<unsigned ndim>
    const typename Face<ndim>::vertex_t *Face<ndim>::vertex(size_t idx) const {
      return vertices[idx];
    }

    template<unsigned ndim>
    size_t Face<ndim>::nVertices() const {
      return vertices.size();
    }

    template<unsigned ndim>
    typename Face<ndim>::vector_t Face<ndim>::centroid() const {
      vector_t c;
      carve::geom::centroid(vertices.begin(), vertices.end(), vec_adapt_vertex_ptr(), c);
      return c;
    }

    template<unsigned ndim>
    std::vector<carve::geom::vector<2> > Face<ndim>::projectedVertices() const {
      p2_adapt_project<ndim> proj = projector();
      std::vector<carve::geom::vector<2> > result;
      result.reserve(nVertices());
      for (size_t i = 0; i < nVertices(); ++i) {
        result.push_back(proj(vertex(i)->v));
      }
      return result;
    }

  }
}
