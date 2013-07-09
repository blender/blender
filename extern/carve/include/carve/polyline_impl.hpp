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

namespace carve {
  namespace line {

    inline PolylineEdge::PolylineEdge(Polyline *_parent, int _edgenum, Vertex *_v1, Vertex *_v2) :
        tagable(), parent(_parent), edgenum(_edgenum), v1(_v1), v2(_v2) {
    }

    inline carve::geom3d::AABB PolylineEdge::aabb() const {
      carve::geom3d::AABB a;
      a.fit(v1->v, v2->v);
      return a;
    }

    inline PolylineEdge *PolylineEdge::prevEdge() const {
      if (edgenum) {
        return parent->edge(edgenum - 1);
      } else {
        if (parent->closed) {
          return parent->edge(parent->edgeCount() - 1);
        } else {
          return NULL;
        }
      }
    }

    inline PolylineEdge *PolylineEdge::nextEdge() const {
      if (edgenum + 1 < parent->edgeCount()) {
        return parent->edge(edgenum + 1);
      } else {
        if (parent->closed) {
          return parent->edge(0);
        } else {
          return NULL;
        }
      }
    }



    inline Polyline::Polyline() : edges() {
    }

    inline size_t Polyline::vertexCount() const {
      return edgeCount() + (closed ? 0 : 1);
    }

    inline size_t Polyline::edgeCount() const {
      return edges.size();
    }

    inline const PolylineEdge *Polyline::edge(size_t e) const {
      return edges[e % edges.size()];
    }

    inline PolylineEdge *Polyline::edge(size_t e) {
      return edges[e % edges.size()];
    }

    inline const Vertex *Polyline::vertex(size_t v) const {
      if (closed) {
        v %= edgeCount();
      } else if (v >= edgeCount()) {
        return v == edgeCount() ? edges.back()->v2 : NULL;
      }
      return edges[v]->v1;
    }

    inline Vertex *Polyline::vertex(size_t v) {
      if (closed) {
        v %= edgeCount();
      } else if (v >= edgeCount()) {
        return v == edgeCount() ? edges.back()->v2 : NULL;
      }
      return edges[v]->v1;
    }

    inline bool Polyline::isClosed() const {
      return closed;
    }

    template<typename iter_t>
    void Polyline::_init(bool c, iter_t begin, iter_t end, std::vector<Vertex> &vertices) {
      closed = c;

      PolylineEdge *e;
      if (begin == end) return;
      size_t v1 = (int)*begin++;
      if (begin == end) return;

      while (begin != end) {
        size_t v2 = (int)*begin++;
        e = new PolylineEdge(this, edges.size(), &vertices[v1], &vertices[v2]);
        edges.push_back(e);
        v1 = v2;
      }

      if (closed) {
        e = new PolylineEdge(this, edges.size(), edges.back()->v2, edges.front()->v1);
        edges.push_back(e);

        edges.front()->v1->addEdgePair(edges.back(), edges.front());
        for (size_t i = 1; i < edges.size(); ++i) {
          edges[i]->v1->addEdgePair(edges[i-1], edges[i]);
        }
      } else {
        edges.front()->v1->addEdgePair(NULL, edges.front());
        for (size_t i = 1; i < edges.size(); ++i) {
          edges[i]->v1->addEdgePair(edges[i-1], edges[i]);
        }
        edges.back()->v2->addEdgePair(edges.back(), NULL);
      }
    }

    template<typename iter_t>
    void Polyline::_init(bool closed, iter_t begin, iter_t end, std::vector<Vertex> &vertices, std::forward_iterator_tag) {
      _init(closed, begin, end, vertices);
    }

    template<typename iter_t>
    void Polyline::_init(bool closed, iter_t begin, iter_t end, std::vector<Vertex> &vertices, std::random_access_iterator_tag) {
      edges.reserve(end - begin - (closed ? 0 : 1));
      _init(closed, begin, end, vertices);
    }

    template<typename iter_t>
    Polyline::Polyline(bool closed, iter_t begin, iter_t end, std::vector<Vertex> &vertices) {
      _init(closed, begin, end, vertices, typename std::iterator_traits<iter_t>::iterator_category());
    }



    template<typename iter_t>
    void PolylineSet::addPolyline(bool closed, iter_t begin, iter_t end) {
      Polyline *p = new Polyline(closed, begin, end, vertices);
      lines.push_back(p);
    }

    inline size_t PolylineSet::vertexToIndex_fast(const Vertex *v) const {
      return v - &vertices[0];
    }
  }
}
