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

#include <iterator>
#include <list>
#include <iterator>
#include <limits>
#include <cstddef>

#include <carve/polyline_decl.hpp>

namespace carve {
  namespace line {

    struct polyline_vertex_iter : public std::iterator<std::random_access_iterator_tag, Vertex *> {
      Polyline *base;
      size_t idx;

      polyline_vertex_iter(Polyline *_base) : base(_base), idx(0) {
      }

      polyline_vertex_iter(Polyline *_base, size_t _idx) : base(_base), idx(_idx) {
      }

      polyline_vertex_iter operator++(int) { return polyline_vertex_iter(base, idx++); }
      polyline_vertex_iter &operator++() { ++idx; return *this; }
      polyline_vertex_iter &operator+=(int v) { idx += v; return *this; }

      polyline_vertex_iter operator--(int) { return polyline_vertex_iter(base, idx--); }
      polyline_vertex_iter &operator--() { --idx; return *this; }
      polyline_vertex_iter &operator-=(int v) { idx -= v; return *this; }

      Vertex *operator*() const {
        return base->vertex(idx);
      }
    };



    static inline ptrdiff_t operator-(const polyline_vertex_iter &a, const polyline_vertex_iter &b) { return a.idx - b.idx; }
                
    static inline bool operator==(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx == b.idx; }
    static inline bool operator!=(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx != b.idx; }
    static inline bool operator<(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx < b.idx; }
    static inline bool operator>(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx > b.idx; }
    static inline bool operator<=(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx <= b.idx; }
    static inline bool operator>=(const polyline_vertex_iter&a, const polyline_vertex_iter &b) { return a.idx >= b.idx; }



    struct polyline_vertex_const_iter : public std::iterator<std::random_access_iterator_tag, Vertex *> {
      const Polyline *base;
      size_t idx;

      polyline_vertex_const_iter(const Polyline *_base) : base(_base), idx(0) {
      }

      polyline_vertex_const_iter(const Polyline *_base, size_t _idx) : base(_base), idx(_idx) {
      }

      polyline_vertex_const_iter operator++(int) { return polyline_vertex_const_iter(base, idx++); }
      polyline_vertex_const_iter &operator++() { ++idx; return *this; }
      polyline_vertex_const_iter &operator+=(int v) { idx += v; return *this; }

      polyline_vertex_const_iter operator--(int) { return polyline_vertex_const_iter(base, idx--); }
      polyline_vertex_const_iter &operator--() { --idx; return *this; }
      polyline_vertex_const_iter &operator-=(int v) { idx -= v; return *this; }

      const Vertex *operator*() const {
        return base->vertex(idx);
      }
    };



    static inline ptrdiff_t operator-(const polyline_vertex_const_iter &a, const polyline_vertex_const_iter &b) { return a.idx - b.idx; }
                
    static inline bool operator==(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx == b.idx; }
    static inline bool operator!=(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx != b.idx; }
    static inline bool operator<(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx < b.idx; }
    static inline bool operator>(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx > b.idx; }
    static inline bool operator<=(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx <= b.idx; }
    static inline bool operator>=(const polyline_vertex_const_iter&a, const polyline_vertex_const_iter &b) { return a.idx >= b.idx; }

    inline polyline_vertex_const_iter Polyline::vbegin() const { 
      return polyline_vertex_const_iter(this, 0);
    }
    inline polyline_vertex_const_iter Polyline::vend() const { 
      return polyline_vertex_const_iter(this, vertexCount());
    }
    inline polyline_vertex_iter Polyline::vbegin() { 
      return polyline_vertex_iter(this, 0);
    }
    inline polyline_vertex_iter Polyline::vend() { 
      return polyline_vertex_iter(this, vertexCount());
    }



    struct polyline_edge_iter : public std::iterator<std::random_access_iterator_tag, PolylineEdge *> {
      Polyline *base;
      size_t idx;

      polyline_edge_iter(Polyline *_base) : base(_base), idx(0) {
      }

      polyline_edge_iter(Polyline *_base, size_t _idx) : base(_base), idx(_idx) {
      }

      polyline_edge_iter operator++(int) { return polyline_edge_iter(base, idx++); }
      polyline_edge_iter &operator++() { ++idx; return *this; }
      polyline_edge_iter &operator+=(int v) { idx += v; return *this; }

      polyline_edge_iter operator--(int) { return polyline_edge_iter(base, idx--); }
      polyline_edge_iter &operator--() { --idx; return *this; }
      polyline_edge_iter &operator-=(int v) { idx -= v; return *this; }

      PolylineEdge *operator*() const {
        return base->edge(idx);
      }
    };



    static inline int operator-(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx - b.idx; }
                
    static inline bool operator==(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx == b.idx; }
    static inline bool operator!=(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx != b.idx; }
    static inline bool operator<(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx < b.idx; }
    static inline bool operator>(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx > b.idx; }
    static inline bool operator<=(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx <= b.idx; }
    static inline bool operator>=(const polyline_edge_iter&a, const polyline_edge_iter &b) { return a.idx >= b.idx; }



    struct polyline_edge_const_iter : public std::iterator<std::random_access_iterator_tag, PolylineEdge *> {
      const Polyline *base;
      size_t idx;

      polyline_edge_const_iter(const Polyline *_base) : base(_base), idx(0) {
      }

      polyline_edge_const_iter(const Polyline *_base, size_t _idx) : base(_base), idx(_idx) {
      }

      polyline_edge_const_iter operator++(int) { return polyline_edge_const_iter(base, idx++); }
      polyline_edge_const_iter &operator++() { ++idx; return *this; }
      polyline_edge_const_iter &operator+=(int v) { idx += v; return *this; }

      polyline_edge_const_iter operator--(int) { return polyline_edge_const_iter(base, idx--); }
      polyline_edge_const_iter &operator--() { --idx; return *this; }
      polyline_edge_const_iter &operator-=(int v) { idx -= v; return *this; }

      const PolylineEdge *operator*() const {
        return base->edge(idx);
      }
    };



    static inline int operator-(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx - b.idx; }
                
    static inline bool operator==(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx == b.idx; }
    static inline bool operator!=(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx != b.idx; }
    static inline bool operator<(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx < b.idx; }
    static inline bool operator>(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx > b.idx; }
    static inline bool operator<=(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx <= b.idx; }
    static inline bool operator>=(const polyline_edge_const_iter&a, const polyline_edge_const_iter &b) { return a.idx >= b.idx; }

    inline polyline_edge_const_iter Polyline::ebegin() const { 
      return polyline_edge_const_iter(this, 0);
    }
    inline polyline_edge_const_iter Polyline::eend() const { 
      return polyline_edge_const_iter(this, edgeCount());
    }
    inline polyline_edge_iter Polyline::ebegin() { 
      return polyline_edge_iter(this, 0);
    }
    inline polyline_edge_iter Polyline::eend() { 
      return polyline_edge_iter(this, edgeCount());
    }

  }
}
