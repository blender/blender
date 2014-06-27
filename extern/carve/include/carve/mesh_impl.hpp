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

#include <carve/geom2d.hpp>
#include <carve/geom3d.hpp>
#include <carve/djset.hpp>

#include <iostream>
#include <deque>

#include <stddef.h>

namespace carve {
  namespace mesh {



    namespace detail {
      template<typename list_t>
      struct list_iter_t {
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef list_t                          value_type;
        typedef ptrdiff_t                       difference_type;
        typedef value_type &                    reference;
        typedef value_type *                    pointer;

        list_t *curr;
        int pos;

        list_iter_t() { }
        list_iter_t(list_t *_curr, int _pos) : curr(_curr), pos(_pos) { }

        list_iter_t operator++(int) { list_iter_t result(*this); ++pos; curr = curr->next; return result; }
        list_iter_t operator--(int) { list_iter_t result(*this); --pos; curr = curr->prev; return result; }

        list_iter_t operator++() { ++pos; curr = curr->next; return *this; }
        list_iter_t operator--() { --pos; curr = curr->prev; return *this; }

        bool operator==(const list_iter_t &other) const { return curr == other.curr && pos == other.pos; }
        bool operator!=(const list_iter_t &other) const { return curr != other.curr || pos != other.pos; }

        reference operator*() { return *curr; }
        pointer operator->() { return curr; }

        int idx() const { return pos; }
      };
    }



    template<unsigned ndim>
    Edge<ndim> *Edge<ndim>::mergeFaces() {
      if (rev == NULL) return NULL;

      face_t *fwdface = face;
      face_t *revface = rev->face;

      size_t n_removed = 0;

      Edge *splice_beg = this;
      do {
        splice_beg = splice_beg->prev;
        ++n_removed;
      } while (splice_beg != this &&
               splice_beg->rev &&
               splice_beg->next->rev->prev == splice_beg->rev);

      if (splice_beg == this) {
        // edge loops are completely matched.
        return NULL;
      }

      Edge *splice_end = this;
      do {
        splice_end = splice_end->next;
        ++n_removed;
      } while (splice_end->rev &&
               splice_end->prev->rev->next == splice_end->rev);

      --n_removed;

      Edge *link1_p = splice_beg;
      Edge *link1_n = splice_beg->next->rev->next;

      Edge *link2_p = splice_end->prev->rev->prev;
      Edge *link2_n = splice_end;

      CARVE_ASSERT(link1_p->face == fwdface);
      CARVE_ASSERT(link1_n->face == revface);

      CARVE_ASSERT(link2_p->face == revface);
      CARVE_ASSERT(link2_n->face == fwdface);

      Edge *left_loop = link1_p->next;

      CARVE_ASSERT(left_loop->rev == link1_n->prev);

      _link(link2_n->prev, link1_p->next);
      _link(link1_n->prev, link2_p->next);

      _link(link1_p, link1_n);
      _link(link2_p, link2_n);

      fwdface->edge = link1_p;

      for (Edge *e = link1_n; e != link2_n; e = e->next) {
        CARVE_ASSERT(e->face == revface);
        e->face = fwdface;
        fwdface->n_edges++;
      }
      for (Edge *e = link2_n; e != link1_n; e = e->next) {
        CARVE_ASSERT(e->face == fwdface);
      }

      fwdface->n_edges -= n_removed;

      revface->n_edges = 0;
      revface->edge = NULL;

      _setloopface(left_loop, NULL);
      _setloopface(left_loop->rev, NULL);

      return left_loop;
    }



    template<unsigned ndim>
    Edge<ndim> *Edge<ndim>::removeHalfEdge() {
      Edge *n = NULL;
      if (face) {
        face->n_edges--;
      }

      if (next == this) {
        if (face) face->edge = NULL;
      } else {
        if (face && face->edge == this) face->edge = next;
        next->prev = prev;
        prev->next = next;
        n = next;
      }
      delete this;
      return n;
    }



    template<unsigned ndim>
    Edge<ndim> *Edge<ndim>::removeEdge() {
      if (rev) {
        rev->removeHalfEdge();
      }
      return removeHalfEdge();
    }



    template<unsigned ndim>
    void Edge<ndim>::unlink() {
      if (rev) { rev->rev = NULL; rev = NULL; }
      if (prev->rev) { prev->rev->rev = NULL; prev->rev = NULL; }

      if (face) {
        face->n_edges--;
        if (face->edge == this) face->edge = next;
        face = NULL;
      }

      next->prev = prev;
      prev->next = next;

      prev = next = this;
    }



    template<unsigned ndim>
    void Edge<ndim>::insertBefore(Edge<ndim> *other) {
      if (prev != this) unlink();
      prev = other->prev;
      next = other;
      next->prev = this;
      prev->next = this;

      if (prev->rev) { prev->rev->rev = NULL;  prev->rev = NULL; }
    }



    template<unsigned ndim>
    void Edge<ndim>::insertAfter(Edge<ndim> *other) {
      if (prev != this) unlink();
      next = other->next;
      prev = other;
      next->prev = this;
      prev->next = this;

      if (prev->rev) { prev->rev->rev = NULL;  prev->rev = NULL; }
    }



    template<unsigned ndim>
    size_t Edge<ndim>::loopSize() const {
      const Edge *e = this;
      size_t n = 0;
      do { e = e->next; ++n; } while (e != this);
      return n;
    }



    template<unsigned ndim>
    Edge<ndim> *Edge<ndim>::perimNext() const {
      if (rev) return NULL;
      Edge *e = next;
      while(e->rev) {
        e = e->rev->next;
      }
      return e;
    }



    template<unsigned ndim>
    Edge<ndim> *Edge<ndim>::perimPrev() const {
      if (rev) return NULL;
      Edge *e = prev;
      while(e->rev) {
        e = e->rev->prev;
      }
      return e;
    }



    template<unsigned ndim>
    Edge<ndim>::Edge(vertex_t *_vert, face_t *_face) :
        vert(_vert), face(_face), prev(NULL), next(NULL), rev(NULL) {
      prev = next = this;
    }



    template<unsigned ndim>
    Edge<ndim>::~Edge() {
    }



    template<unsigned ndim>
    typename Face<ndim>::aabb_t Face<ndim>::getAABB() const {
      aabb_t aabb;
      aabb.fit(begin(), end(), vector_mapping());
      return aabb;
    }



    template<unsigned ndim>
    bool Face<ndim>::recalc() {
      if (!carve::geom3d::fitPlane(begin(), end(), vector_mapping(), plane)) {
        return false;
      }

      int da = carve::geom::largestAxis(plane.N);
      double A = carve::geom2d::signedArea(begin(), end(), projection_mapping(getProjector(false, da)));

      if ((A < 0.0) ^ (plane.N.v[da] < 0.0)) {
        plane.negate();
      }

      project = getProjector(plane.N.v[da] > 0, da);
      unproject = getUnprojector(plane.N.v[da] > 0, da);

      return true;
    }



    template<unsigned ndim>
    void Face<ndim>::clearEdges() {
      if (!edge) return;

      edge_t *curr = edge;
      do {
        edge_t *next = curr->next;
        delete curr;
        curr = next;
      } while (curr != edge);

      edge = NULL;

      n_edges = 0;
    }



    template<unsigned ndim>
    template<typename iter_t>
    void Face<ndim>::loopFwd(iter_t begin, iter_t end) {
      clearEdges();
      if (begin == end) return;
      edge = new edge_t(*begin, this); ++n_edges; ++begin;
      while (begin != end) {
        edge_t *e = new edge_t(*begin, this);
        e->insertAfter(edge->prev);
        ++n_edges;
        ++begin;
      }
    }



    template<unsigned ndim>
    template<typename iter_t>
    void Face<ndim>::loopRev(iter_t begin, iter_t end) {
      clearEdges();
      if (begin == end) return;
      edge = new edge_t(*begin, this); ++n_edges; ++begin;
      while (begin != end) {
        edge_t *e = new edge_t(*begin, this);
        e->insertBefore(edge->next);
        ++n_edges;
        ++begin;
      }
    }



    template<unsigned ndim>
    template<typename iter_t>
    void Face<ndim>::init(iter_t begin, iter_t end) {
      loopFwd(begin, end);
    }



    template<unsigned ndim>
    void Face<ndim>::init(vertex_t *a, vertex_t *b, vertex_t *c) {
      clearEdges();
      edge_t *ea = new edge_t(a, this);
      edge_t *eb = new edge_t(b, this);
      edge_t *ec = new edge_t(c, this);
      eb->insertAfter(ea);
      ec->insertAfter(eb);
      edge = ea;
      n_edges = 3;
    }



    template<unsigned ndim>
    void Face<ndim>::init(vertex_t *a, vertex_t *b, vertex_t *c, vertex_t *d) {
      clearEdges();
      edge_t *ea = new edge_t(a, this);
      edge_t *eb = new edge_t(b, this);
      edge_t *ec = new edge_t(c, this);
      edge_t *ed = new edge_t(d, this);
      eb->insertAfter(ea);
      ec->insertAfter(eb);
      ed->insertAfter(ec);
      edge = ea;
      n_edges = 4;
    }



    template<unsigned ndim>
    void Face<ndim>::getVertices(std::vector<vertex_t *> &verts) const {
      verts.clear();
      verts.reserve(n_edges);
      const edge_t *e = edge;
      do { verts.push_back(e->vert); e = e->next; } while (e != edge);
    }



    template<unsigned ndim>
    void Face<ndim>::getProjectedVertices(std::vector<carve::geom::vector<2> > &verts) const {
      verts.clear();
      verts.reserve(n_edges);
      const edge_t *e = edge;
      do { verts.push_back(project(e->vert->v)); e = e->next; } while (e != edge);
    }



    template<unsigned ndim>
    typename Face<ndim>::vector_t Face<ndim>::centroid() const {
      vector_t v;
      edge_t *e = edge;
      do {
        v += e->vert->v;
        e = e->next;
      } while(e != edge);
      v /= n_edges;
      return v;
    }



    template<unsigned ndim>
    void Face<ndim>::canonicalize() {
      edge_t *min = edge;
      edge_t *e = edge;

      do {
        if (e->vert < min->vert) min = e;
        e = e->next;
      } while (e != edge);

      edge = min;
    }



    template<unsigned ndim>
    template<typename iter_t>
    Face<ndim> *Face<ndim>::create(iter_t beg, iter_t end, bool reversed) const {
      Face *r = new Face();

      if (reversed) {
        r->loopRev(beg, end);
        r->plane = -plane;
      } else {
        r->loopFwd(beg, end);
        r->plane = plane;
      }

      int da = carve::geom::largestAxis(r->plane.N);

      r->project = r->getProjector(r->plane.N.v[da] > 0, da);
      r->unproject = r->getUnprojector(r->plane.N.v[da] > 0, da);

      return r;
    }



    template<unsigned ndim>
    Face<ndim> *Face<ndim>::clone(const vertex_t *old_base,
                                  vertex_t *new_base,
                                  std::unordered_map<const edge_t *, edge_t *> &edge_map) const {
      Face *r = new Face(*this);

      edge_t *e = edge;
      edge_t *r_p = NULL;
      edge_t *r_e;
      do {
        r_e = new edge_t(e->vert - old_base + new_base, r);
        edge_map[e] = r_e;
        if (r_p) {
          r_p->next = r_e;
          r_e->prev = r_p;
        } else {
          r->edge = r_e;
        }
        r_p = r_e;

        if (e->rev) {
          typename std::unordered_map<const edge_t *, edge_t *>::iterator rev_i = edge_map.find(e->rev);
          if (rev_i != edge_map.end()) {
            r_e->rev = (*rev_i).second;
            (*rev_i).second->rev = r_e;
          }
        }

        e = e->next;
      } while (e != edge);
      r_e->next = r->edge;
      r->edge->prev = r_e;
      return r;
    }



    template<unsigned ndim>
    Mesh<ndim>::Mesh(std::vector<face_t *> &_faces,
                     std::vector<edge_t *> &_open_edges,
                     std::vector<edge_t *> &_closed_edges,
                     bool _is_negative) {
      std::swap(faces, _faces);
      std::swap(open_edges, _open_edges);
      std::swap(closed_edges, _closed_edges);
      is_negative = _is_negative;
      meshset = NULL;

      for (size_t i = 0; i < faces.size(); ++i) {
        faces[i]->mesh = this;
      }
    }



    namespace detail {
      template<typename iter_t>
      void FaceStitcher::initEdges(iter_t begin,
                                   iter_t end) {
        size_t c = 0;
        for (iter_t i = begin; i != end; ++i) {
          face_t *face = *i;
          CARVE_ASSERT(face->mesh == NULL); // for the moment, can only insert a face into a mesh once.

          face->id = c++;
          edge_t *e = face->edge;
          do {
            edges[vpair_t(e->v1(), e->v2())].push_back(e);
            e = e->next;
            if (e->rev) { e->rev->rev = NULL; e->rev = NULL; }
          } while (e != face->edge);
        }
        face_groups.init(c);
        is_open.clear();
        is_open.resize(c, false);
      }

      template<typename iter_t>
      void FaceStitcher::build(iter_t begin,
                               iter_t end,
                               std::vector<Mesh<3> *> &meshes) {
        // work out what set each face belongs to, and then construct
        // mesh instances for each set of faces.
        std::vector<size_t> index_set;
        std::vector<size_t> set_size;
        face_groups.get_index_to_set(index_set, set_size);

        std::vector<std::vector<face_t *> > mesh_faces;
        mesh_faces.resize(set_size.size());
        for (size_t i = 0; i < set_size.size(); ++i) {
          mesh_faces[i].reserve(set_size[i]);
        }
      
        for (iter_t i = begin; i != end; ++i) {
          face_t *face = *i;
          mesh_faces[index_set[face->id]].push_back(face);
        }

        meshes.clear();
        meshes.reserve(mesh_faces.size());
        for (size_t i = 0; i < mesh_faces.size(); ++i) {
          meshes.push_back(new Mesh<3>(mesh_faces[i]));
        }
      }

      template<typename iter_t>
      void FaceStitcher::create(iter_t begin,
                                iter_t end,
                                std::vector<Mesh<3> *> &meshes) {
        initEdges(begin, end);
        construct();
        build(begin, end, meshes);
      }
    }



    template<unsigned ndim>
    void Mesh<ndim>::cacheEdges() {
      closed_edges.clear();
      open_edges.clear();

      for (size_t i = 0; i < faces.size(); ++i) {
        face_t *face = faces[i];
        edge_t *e = face->edge;
        do {
          if (e->rev == NULL) {
            open_edges.push_back(e);
          } else if (e < e->rev) {
            closed_edges.push_back(e);
          }
          e = e->next;
        } while (e != face->edge);
      }
    }



    template<unsigned ndim>
    Mesh<ndim>::Mesh(std::vector<face_t *> &_faces) : faces(), open_edges(), closed_edges(), meshset(NULL) {
      faces.swap(_faces);
      for (size_t i = 0; i < faces.size(); ++i) {
        faces[i]->mesh = this;
      }
      cacheEdges();
      calcOrientation();
    }



    template<unsigned ndim>
    int Mesh<ndim>::orientationAtVertex(edge_t *e_base) {
#if defined(CARVE_DEBUG)
      std::cerr << "warning: vertex orientation not defined for ndim=" << ndim << std::endl;
#endif
      return 0;
    }



    template<>
    inline int Mesh<3>::orientationAtVertex(edge_t *e_base) {
      edge_t *e = e_base;
      vertex_t::vector_t v_base = e->v1()->v;
      std::vector<vertex_t::vector_t> v_edge;

      if (v_edge.size() < 3) {
        return 0;
      }

      do {
        v_edge.push_back(e->v2()->v);
        e = e->rev->next;
      } while (e != e_base);

      const size_t N = v_edge.size();

      for (size_t i = 0; i < N; ++i) {
        size_t j = (i + 1) % N;

        double o_hi = 0.0;
        double o_lo = 0.0;

        for (size_t k = (j + 1) % N; k != i; k = (k + 1) % N) {
          double o = carve::geom3d::orient3d(v_edge[i], v_base, v_edge[j], v_edge[k]);
          o_hi = std::max(o_hi, o);
          o_lo = std::max(o_lo, o);
        }

        if (o_lo >= 0.0) return +1;
        if (o_hi <= 0.0) return -1;
      }

      return 0;
    }



    template<unsigned ndim>
    void Mesh<ndim>::calcOrientation() {
      if (open_edges.size() || !closed_edges.size()) {
        is_negative = false;
        return;
      }

      edge_t *emin = closed_edges[0];

      if (emin->rev->v1()->v < emin->v1()->v) emin = emin->rev;

      for (size_t i = 1; i < closed_edges.size(); ++i) {
        if (closed_edges[i]->v1()->v      < emin->v1()->v) emin = closed_edges[i];
        if (closed_edges[i]->rev->v1()->v < emin->v1()->v) emin = closed_edges[i]->rev;
      }

      int orientation = orientationAtVertex(emin);

#if defined(CARVE_DEBUG)
      if (orientation == 0) {
        std::cerr << "warning: could not determine orientation for mesh " << this << std::endl;
      }
#endif

      is_negative = orientation == -1;
    }



    template<unsigned ndim>
    Mesh<ndim> *Mesh<ndim>::clone(const vertex_t *old_base,
                                  vertex_t *new_base) const {
      std::vector<face_t *> r_faces;
      std::vector<edge_t *> r_open_edges;
      std::vector<edge_t *> r_closed_edges;
      std::unordered_map<const edge_t *, edge_t *> edge_map;

      r_faces.reserve(faces.size());
      r_open_edges.reserve(r_open_edges.size());
      r_closed_edges.reserve(r_closed_edges.size());

      for (size_t i = 0; i < faces.size(); ++i) {
        r_faces.push_back(faces[i]->clone(old_base, new_base, edge_map));
      }
      for (size_t i = 0; i < closed_edges.size(); ++i) {
        r_closed_edges.push_back(edge_map[closed_edges[i]]);
        r_closed_edges.back()->rev = edge_map[closed_edges[i]->rev];
      }
      for (size_t i = 0; i < open_edges.size(); ++i) {
        r_open_edges.push_back(edge_map[open_edges[i]]);
      }

      return new Mesh(r_faces, r_open_edges, r_closed_edges, is_negative);
    }



    template<unsigned ndim>
    Mesh<ndim>::~Mesh() {
      for (size_t i = 0; i < faces.size(); ++i) {
        delete faces[i];
      }
    }



    template<unsigned ndim>
    template<typename iter_t>
    void Mesh<ndim>::create(iter_t begin, iter_t end, std::vector<Mesh<ndim> *> &meshes, const MeshOptions &opts) {
      meshes.clear();
    }



    template<>
    template<typename iter_t>
    void Mesh<3>::create(iter_t begin, iter_t end, std::vector<Mesh<3> *> &meshes, const MeshOptions &opts) {
      detail::FaceStitcher(opts).create(begin, end, meshes);
    }



    template<unsigned ndim>
    template<typename iter_t>
    void MeshSet<ndim>::_init_from_faces(iter_t begin, iter_t end, const MeshOptions &opts) {
      typedef std::unordered_map<const vertex_t *, size_t> map_t;
      map_t vmap;

      for (iter_t i = begin; i != end; ++i) {
        face_t *f = *i;
        edge_t *e = f->edge;
        do {
          typename map_t::const_iterator j = vmap.find(e->vert);
          if (j == vmap.end()) {
            size_t idx = vmap.size();
            vmap[e->vert] = idx;
          }
          e = e->next;
        } while (e != f->edge);
      }

      vertex_storage.resize(vmap.size());
      for (typename map_t::const_iterator i = vmap.begin(); i != vmap.end(); ++i) {
        vertex_storage[(*i).second].v = (*i).first->v;
      }

      for (iter_t i = begin; i != end; ++i) {
        face_t *f = *i;
        edge_t *e = f->edge;
        do {
          e->vert = &vertex_storage[vmap[e->vert]];
          e = e->next;
        } while (e != f->edge);
      }

      mesh_t::create(begin, end, meshes, opts);

      for (size_t i = 0; i < meshes.size(); ++i) {
        meshes[i]->meshset = this;
      }
    }



    template<unsigned ndim>
    MeshSet<ndim>::MeshSet(const std::vector<typename MeshSet<ndim>::vertex_t::vector_t> &points,
                           size_t n_faces,
                           const std::vector<int> &face_indices,
                           const MeshOptions &opts) {
      vertex_storage.reserve(points.size());
      std::vector<face_t *> faces;
      faces.reserve(n_faces);
      for (size_t i = 0; i < points.size(); ++i) {
        vertex_storage.push_back(vertex_t(points[i]));
      }

      std::vector<vertex_t *> v;
      size_t p = 0;
      for (size_t i = 0; i < n_faces; ++i) {
        CARVE_ASSERT(face_indices[p] > 1);

        const size_t N = (size_t)face_indices[p++];
        v.clear();
        v.reserve(N);
        for (size_t j = 0; j < N; ++j) {
          v.push_back(&vertex_storage[face_indices[p++]]);
        }
        faces.push_back(new face_t(v.begin(), v.end()));
      }
      CARVE_ASSERT(p == face_indices.size());
      mesh_t::create(faces.begin(), faces.end(), meshes, opts);

      for (size_t i = 0; i < meshes.size(); ++i) {
        meshes[i]->meshset = this;
      }
    }



    template<unsigned ndim>
    MeshSet<ndim>::MeshSet(std::vector<face_t *> &faces, const MeshOptions &opts) {
      _init_from_faces(faces.begin(), faces.end(), opts);
    }



    template<unsigned ndim>
    MeshSet<ndim>::MeshSet(std::list<face_t *> &faces, const MeshOptions &opts) {
      _init_from_faces(faces.begin(), faces.end(), opts);
    }



    template<unsigned ndim>
    MeshSet<ndim>::MeshSet(std::vector<vertex_t> &_vertex_storage,
                           std::vector<mesh_t *> &_meshes) {
      vertex_storage.swap(_vertex_storage);
      meshes.swap(_meshes);

      for (size_t i = 0; i < meshes.size(); ++i) {
        meshes[i]->meshset = this;
      }
    }



    template<unsigned ndim>
    MeshSet<ndim>::MeshSet(std::vector<typename MeshSet<ndim>::mesh_t *> &_meshes) {
      meshes.swap(_meshes);
      std::unordered_map<vertex_t *, size_t> vert_idx;

      for (size_t m = 0; m < meshes.size(); ++m) {
        mesh_t *mesh = meshes[m];
        CARVE_ASSERT(mesh->meshset == NULL);
        mesh->meshset = this;
        for (size_t f = 0; f < mesh->faces.size(); ++f) {
          face_t *face = mesh->faces[f];
          edge_t *edge = face->edge;
          do {
            vert_idx[edge->vert] = 0;
            edge = edge->next;
          } while (edge != face->edge);
        }
      }

      vertex_storage.reserve(vert_idx.size());
      for (typename std::unordered_map<vertex_t *, size_t>::iterator i = vert_idx.begin(); i != vert_idx.end(); ++i) {
        (*i).second = vertex_storage.size();
        vertex_storage.push_back(*(*i).first);
      }

      for (size_t m = 0; m < meshes.size(); ++m) {
        mesh_t *mesh = meshes[m];
        for (size_t f = 0; f < mesh->faces.size(); ++f) {
          face_t *face = mesh->faces[f];
          edge_t *edge = face->edge;
          do {
            size_t i = vert_idx[edge->vert];
            edge->vert = &vertex_storage[i];
            edge = edge->next;
          } while (edge != face->edge);
        }
      }
    }



    template<unsigned ndim>
    MeshSet<ndim> *MeshSet<ndim>::clone() const {
      std::vector<vertex_t> r_vertex_storage = vertex_storage;
      std::vector<mesh_t *> r_meshes;
      for (size_t i = 0; i < meshes.size(); ++i) {
        r_meshes.push_back(meshes[i]->clone(&vertex_storage[0], &r_vertex_storage[0]));
      }

      return new MeshSet(r_vertex_storage, r_meshes);
    }



    template<unsigned ndim>
    MeshSet<ndim>::~MeshSet() {
      for (size_t i = 0; i < meshes.size(); ++i) {
        delete meshes[i];
      }
    }



    template<unsigned ndim>
    template<typename face_type>
    MeshSet<ndim>::FaceIter<face_type>::FaceIter(const MeshSet<ndim> *_obj, size_t _mesh, size_t _face) : obj(_obj), mesh(_mesh), face(_face) {
    }



    template<unsigned ndim>
    template<typename face_type>
    void MeshSet<ndim>::FaceIter<face_type>::fwd(size_t n) {
      if (mesh < obj->meshes.size()) {
        face += n;
        while (face >= obj->meshes[mesh]->faces.size()) {
          face -= obj->meshes[mesh++]->faces.size();
          if (mesh == obj->meshes.size()) { face = 0; break; }
        }
      }
    }



    template<unsigned ndim>
    template<typename face_type>
    void MeshSet<ndim>::FaceIter<face_type>::rev(size_t n) {
      while (n > face) {
        n -= face;
        if (mesh == 0) { face = 0; return; }
        face = obj->meshes[--mesh]->faces.size() - 1;
      }
      face -= n;
    }



    template<unsigned ndim>
    template<typename face_type>
    void MeshSet<ndim>::FaceIter<face_type>::adv(int n) {
      if (n > 0) {
        fwd((size_t)n);
      } else if (n < 0) {
        rev((size_t)-n);
      }
    }



    template<unsigned ndim>
    template<typename face_type>
    typename MeshSet<ndim>::template FaceIter<face_type>::difference_type
    MeshSet<ndim>::FaceIter<face_type>::operator-(const FaceIter &other) const {
      CARVE_ASSERT(obj == other.obj);
      if (mesh == other.mesh) return face - other.face;

      size_t m = 0;
      for (size_t i = std::min(mesh, other.mesh) + 1; i < std::max(mesh, other.mesh); ++i) {
        m += obj->meshes[i]->faces.size();
      }

      if (mesh < other.mesh) {
        return -(difference_type)((obj->meshes[mesh]->faces.size() - face) + m + other.face);
      } else {
        return +(difference_type)((obj->meshes[other.mesh]->faces.size() - other.face) + m + face);
      }
    }



    template<typename order_t>
    struct VPtrSort {
      order_t order;

      VPtrSort(const order_t &_order = order_t()) : order(_order) {}

      template<unsigned ndim>
      bool operator()(carve::mesh::Vertex<ndim> *a,
                      carve::mesh::Vertex<ndim> *b) const {
        return order(a->v, b->v);
      }
    };



    template<unsigned ndim>
    void MeshSet<ndim>::collectVertices() {
      std::unordered_map<vertex_t *, size_t> vert_idx;

      for (size_t m = 0; m < meshes.size(); ++m) {
        mesh_t *mesh = meshes[m];

        for (size_t f = 0; f < mesh->faces.size(); ++f) {
          face_t *face = mesh->faces[f];
          edge_t *edge = face->edge;
          do {
            vert_idx[edge->vert] = 0;
            edge = edge->next;
          } while (edge != face->edge);
        }
      }

      std::vector<vertex_t> new_vertex_storage;
      new_vertex_storage.reserve(vert_idx.size());
      for (typename std::unordered_map<vertex_t *, size_t>::iterator
             i = vert_idx.begin(); i != vert_idx.end(); ++i) {
        (*i).second = new_vertex_storage.size();
        new_vertex_storage.push_back(*(*i).first);
      }

      for (size_t m = 0; m < meshes.size(); ++m) {
        mesh_t *mesh = meshes[m];
        for (size_t f = 0; f < mesh->faces.size(); ++f) {
          face_t *face = mesh->faces[f];
          edge_t *edge = face->edge;
          do {
            size_t i = vert_idx[edge->vert];
            edge->vert = &new_vertex_storage[i];
            edge = edge->next;
          } while (edge != face->edge);
        }
      }

      std::swap(vertex_storage, new_vertex_storage);
    }



    template<unsigned ndim>
    void MeshSet<ndim>::canonicalize() {
      std::vector<vertex_t *> vptr;
      std::vector<vertex_t *> vmap;
      std::vector<vertex_t> vout;
      const size_t N = vertex_storage.size();

      vptr.reserve(N);
      vout.reserve(N);
      vmap.resize(N);

      for (size_t i = 0; i != N; ++i) {
        vptr.push_back(&vertex_storage[i]);
      }
      std::sort(vptr.begin(), vptr.end(), VPtrSort<std::less<typename vertex_t::vector_t> >());

      for (size_t i = 0; i != N; ++i) {
        vout.push_back(*vptr[i]);
        vmap[(size_t)(vptr[i] - &vertex_storage[0])] = &vout[i];
      }

      for (face_iter i = faceBegin(); i != faceEnd(); ++i) {
        for (typename face_t::edge_iter_t j = (*i)->begin(); j != (*i)->end(); ++j) {
          (*j).vert = vmap[(size_t)((*j).vert - &vertex_storage[0])];
        }
        (*i)->canonicalize();
      }

      vertex_storage.swap(vout);
    }



    template<unsigned ndim>
    void MeshSet<ndim>::separateMeshes() {
      size_t n;
      typedef std::unordered_map<std::pair<mesh_t *, vertex_t *>, vertex_t *> vmap_t;
      vmap_t vmap;
      typename vmap_t::iterator vmap_iter;

      for (face_iter i = faceBegin(); i != faceEnd(); ++i) {
        face_t *f = *i;
        for (typename face_t::edge_iter_t j = f->begin(); j != f->end(); ++j) {
          edge_t &e = *j;
          vmap[std::make_pair(f->mesh, e.vert)] = e.vert;
        }
      }

      std::vector<vertex_t> vout;
      vout.reserve(vmap.size());

      for (n = 0, vmap_iter = vmap.begin(); vmap_iter != vmap.end(); ++vmap_iter, ++n) {
        vout.push_back(*(*vmap_iter).second);
        (*vmap_iter).second = & vout.back();
      }

      for (face_iter i = faceBegin(); i != faceEnd(); ++i) {
        face_t *f = *i;
        for (typename face_t::edge_iter_t j = f->begin(); j != f->end(); ++j) {
          edge_t &e = *j;
          e.vert = vmap[std::make_pair(f->mesh, e.vert)];
        }
      }

      vertex_storage.swap(vout);
    }

  }
}
