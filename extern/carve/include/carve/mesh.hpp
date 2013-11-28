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

#include <carve/geom.hpp>
#include <carve/geom3d.hpp>
#include <carve/tag.hpp>
#include <carve/djset.hpp>
#include <carve/aabb.hpp>
#include <carve/rtree.hpp>

#include <iostream>

namespace carve {
  namespace poly {
    class Polyhedron;
  }

  namespace mesh {


    template<unsigned ndim> class Edge;
    template<unsigned ndim> class Face;
    template<unsigned ndim> class Mesh;
    template<unsigned ndim> class MeshSet;



    // A Vertex may participate in several meshes. If the Mesh belongs
    // to a MeshSet, then the vertices come from the vertex_storage
    // member of the MeshSet. This allows one to construct one or more
    // Meshes out of sets of connected faces (possibly using vertices
    // from a variety of MeshSets, and other storage), then create a
    // MeshSet from the Mesh(es), causing the vertices to be
    // collected, cloned and repointed into the MeshSet.

    // Normally, in a half-edge structure, Vertex would have a member
    // pointing to an incident edge, allowing the enumeration of
    // adjacent faces and edges. Because we want to support vertex
    // sharing between Meshes and groups of Faces, this is made more
    // complex. If Vertex contained a list of incident edges, one from
    // each disjoint face set, then this could be done (with the
    // caveat that you'd need to pass in a Mesh pointer to the
    // adjacency queries). However, it seems that this would
    // unavoidably complicate the process of incorporating or removing
    // a vertex into an edge.

    // In most cases it is expected that a vertex will be arrived at
    // via an edge or face in the mesh (implicit or explicit) of
    // interest, so not storing this information will not hurt,
    // overly.
    template<unsigned ndim>
    class Vertex : public tagable {
    public:
      typedef carve::geom::vector<ndim> vector_t;
      typedef MeshSet<ndim> owner_t;
      typedef carve::geom::aabb<ndim> aabb_t;

      carve::geom::vector<ndim> v;

      Vertex(const vector_t &_v) : tagable(), v(_v) {
      }

      Vertex() : tagable(), v() {
      }

      aabb_t getAABB() const {
        return aabb_t(v, carve::geom::vector<ndim>::ZERO());
      }
    };



    struct hash_vertex_pair {
      template<unsigned ndim>
      size_t operator()(const std::pair<Vertex<ndim> *, Vertex<ndim> *> &pair) const {
        size_t r = (size_t)pair.first;
        size_t s = (size_t)pair.second;
        return r ^ ((s >> 16) | (s << 16));
      }
      template<unsigned ndim>
      size_t operator()(const std::pair<const Vertex<ndim> *, const Vertex<ndim> *> &pair) const {
        size_t r = (size_t)pair.first;
        size_t s = (size_t)pair.second;
        return r ^ ((s >> 16) | (s << 16));
      }
    };



    struct vertex_distance {
      template<unsigned ndim>
      double operator()(const Vertex<ndim> &a, const Vertex<ndim> &b) const {
        return carve::geom::distance(a.v, b.v);
      }

      template<unsigned ndim>
      double operator()(const Vertex<ndim> *a, const Vertex<ndim> *b) const {
        return carve::geom::distance(a->v, b->v);
      }
    };



    namespace detail {
      template<typename list_t> struct list_iter_t;
      template<typename list_t, typename mapping_t> struct mapped_list_iter_t;
    }



    // The half-edge structure proper (Edge) is maintained by Face
    // instances. Together with Face instances, the half-edge
    // structure defines a simple mesh (either one or two faces
    // incident on each edge).
    template<unsigned ndim>
    class Edge : public tagable {
    public:
      typedef Vertex<ndim> vertex_t;
      typedef Face<ndim> face_t;

      vertex_t *vert;
      face_t *face;
      Edge *prev, *next, *rev;

    private:
      static void _link(Edge *a, Edge *b) {
        a->next = b; b->prev = a;
      }

      static void _freeloop(Edge *s) {
        Edge *e = s;
        do {
          Edge *n = e->next;
          delete e;
          e = n;
        } while (e != s);
      }

      static void _setloopface(Edge *s, face_t *f) {
        Edge *e = s;
        do {
          e->face = f;
          e = e->next;
        } while (e != s);
      }

      static size_t _looplen(Edge *s) {
        Edge *e = s;
        face_t *f = s->face;
        size_t c = 0;
        do {
          ++c;
          CARVE_ASSERT(e->rev->rev == e);
          CARVE_ASSERT(e->next->prev == e);
          CARVE_ASSERT(e->face == f);
          e = e->next;
        } while (e != s);
        return c;
      }

    public:
      void validateLoop() {
        Edge *e = this;
        face_t *f = face;
        size_t c = 0;
        do {
          ++c;
          CARVE_ASSERT(e->rev == NULL || e->rev->rev == e);
          CARVE_ASSERT(e->next == e || e->next->vert != e->vert);
          CARVE_ASSERT(e->prev == e || e->prev->vert != e->vert);
          CARVE_ASSERT(e->next->prev == e);
          CARVE_ASSERT(e->prev->next == e);
          CARVE_ASSERT(e->face == f);
          e = e->next;
        } while (e != this);
        CARVE_ASSERT(f == NULL || c == f->n_edges);
      }

      size_t loopLen() {
        return _looplen(this);
      }

      Edge *mergeFaces();

      Edge *removeHalfEdge();

      // Remove and delete this edge.
      Edge *removeEdge();

      // Unlink this edge from its containing edge loop. disconnect
      // rev links. The rev links of the previous edge also change, as
      // its successor vertex changes.
      void unlink();

      // Insert this edge into a loop before other. If edge was
      // already in a loop, it needs to be removed first.
      void insertBefore(Edge *other);

      // Insert this edge into a loop after other. If edge was
      // already in a loop, it needs to be removed first.
      void insertAfter(Edge *other);

      size_t loopSize() const;

      vertex_t *v1() { return vert; }
      vertex_t *v2() { return next->vert; }

      const vertex_t *v1() const { return vert; }
      const vertex_t *v2() const { return next->vert; }

      Edge *perimNext() const;
      Edge *perimPrev() const;

      double length2() const {
        return (v1()->v - v2()->v).length2();
      }

      double length() const {
        return (v1()->v - v2()->v).length();
      }

      Edge(vertex_t *_vert, face_t *_face);

      ~Edge();
    };



    // A Face contains a pointer to the beginning of the half-edge
    // circular list that defines its boundary.
    template<unsigned ndim>
    class Face : public tagable {
    public:
      typedef Vertex<ndim> vertex_t;
      typedef Edge<ndim> edge_t;
      typedef Mesh<ndim> mesh_t;

      typedef typename Vertex<ndim>::vector_t vector_t;
      typedef carve::geom::aabb<ndim> aabb_t;
      typedef carve::geom::plane<ndim> plane_t;
      typedef carve::geom::vector<2> (*project_t)(const vector_t &);
      typedef vector_t (*unproject_t)(const carve::geom::vector<2> &, const plane_t &);

      struct vector_mapping {
        typedef typename vertex_t::vector_t value_type;

        value_type operator()(const carve::geom::vector<ndim> &v) const { return v; }
        value_type operator()(const carve::geom::vector<ndim> *v) const { return *v; }
        value_type operator()(const Edge<ndim> &e) const { return e.vert->v; }
        value_type operator()(const Edge<ndim> *e) const { return e->vert->v; }
        value_type operator()(const Vertex<ndim> &v) const { return v.v; }
        value_type operator()(const Vertex<ndim> *v) const { return v->v; }
      };

      struct projection_mapping {
        typedef carve::geom::vector<2> value_type;
        project_t proj;
        projection_mapping(project_t _proj) : proj(_proj) { }
        value_type operator()(const carve::geom::vector<ndim> &v) const { return proj(v); }
        value_type operator()(const carve::geom::vector<ndim> *v) const { return proj(*v); }
        value_type operator()(const Edge<ndim> &e) const { return proj(e.vert->v); }
        value_type operator()(const Edge<ndim> *e) const { return proj(e->vert->v); }
        value_type operator()(const Vertex<ndim> &v) const { return proj(v.v); }
        value_type operator()(const Vertex<ndim> *v) const { return proj(v->v); }
      };

      edge_t *edge;
      size_t n_edges;
      mesh_t *mesh;
      size_t id;

      plane_t plane;
      project_t project;
      unproject_t unproject;

    private:
      Face &operator=(const Face &other);

    protected:
      Face() : edge(NULL), n_edges(0), mesh(NULL), id(0), plane(), project(NULL), unproject(NULL) {
      }

      Face(const Face &other) :
        edge(NULL), n_edges(other.n_edges), mesh(NULL), id(other.id),
        plane(other.plane), project(other.project), unproject(other.unproject) {
      }

      project_t getProjector(bool positive_facing, int axis) const;
      unproject_t getUnprojector(bool positive_facing, int axis) const;

    public:
      typedef detail::list_iter_t<Edge<ndim> > edge_iter_t;
      typedef detail::list_iter_t<const Edge<ndim> > const_edge_iter_t;

      edge_iter_t begin() { return edge_iter_t(edge, 0); }
      edge_iter_t end() { return edge_iter_t(edge, n_edges); }

      const_edge_iter_t begin() const { return const_edge_iter_t(edge, 0); }
      const_edge_iter_t end() const { return const_edge_iter_t(edge, n_edges); }

      bool containsPoint(const vector_t &p) const;
      bool containsPointInProjection(const vector_t &p) const;
      bool simpleLineSegmentIntersection(
          const carve::geom::linesegment<ndim> &line,
          vector_t &intersection) const;
      IntersectionClass lineSegmentIntersection(
          const carve::geom::linesegment<ndim> &line,
          vector_t &intersection) const;

      aabb_t getAABB() const;

      bool recalc();

      void clearEdges();

      // build an edge loop in forward orientation from an iterator pair
      template<typename iter_t>
      void loopFwd(iter_t vbegin, iter_t vend);

      // build an edge loop in reverse orientation from an iterator pair
      template<typename iter_t>
      void loopRev(iter_t vbegin, iter_t vend);

      // initialize a face from an ordered list of vertices.
      template<typename iter_t>
      void init(iter_t begin, iter_t end);

      // initialization of a triangular face.
      void init(vertex_t *a, vertex_t *b, vertex_t *c);

      // initialization of a quad face.
      void init(vertex_t *a, vertex_t *b, vertex_t *c, vertex_t *d);

      void getVertices(std::vector<vertex_t *> &verts) const;
      void getProjectedVertices(std::vector<carve::geom::vector<2> > &verts) const;

      projection_mapping projector() const {
        return projection_mapping(project);
      }

      std::pair<double, double> rangeInDirection(const vector_t &v, const vector_t &b) const {
        edge_t *e = edge;
        double lo, hi;
        lo = hi = carve::geom::dot(v, e->vert->v - b);
        e = e->next;
        for (; e != edge; e = e->next) {
          double d = carve::geom::dot(v, e->vert->v - b);
          lo = std::min(lo, d);
          hi = std::max(hi, d);
        }
        return std::make_pair(lo, hi);
      }

      size_t nVertices() const {
        return n_edges;
      }

      size_t nEdges() const {
        return n_edges;
      }

      vector_t centroid() const;

      static Face *closeLoop(edge_t *open_edge);

      Face(edge_t *e) : edge(e), n_edges(0), mesh(NULL) {
        do {
          e->face = this;
          n_edges++;
          e = e->next;
        } while (e != edge);
        recalc();
      }

      Face(vertex_t *a, vertex_t *b, vertex_t *c) : edge(NULL), n_edges(0), mesh(NULL) {
        init(a, b, c);
        recalc();
      }

      Face(vertex_t *a, vertex_t *b, vertex_t *c, vertex_t *d) : edge(NULL), n_edges(0), mesh(NULL) {
        init(a, b, c, d);
        recalc();
      }

      template<typename iter_t>
      Face(iter_t begin, iter_t end) : edge(NULL), n_edges(0), mesh(NULL) {
        init(begin, end);
        recalc();
      }

      template<typename iter_t>
      Face *create(iter_t beg, iter_t end, bool reversed) const;

      Face *clone(const vertex_t *old_base, vertex_t *new_base, std::unordered_map<const edge_t *, edge_t *> &edge_map) const;

      void remove() {
        edge_t *e = edge;
        do {
          if (e->rev) e->rev->rev = NULL;
          e = e->next;
        } while (e != edge);
      }

      void invert() {
        // We invert the direction of the edges of the face in this
        // way so that the edge rev pointers (if any) are still
        // correct. It is expected that invert() will be called on
        // every other face in the mesh, too, otherwise everything
        // will get messed up.

        {
          // advance vertices.
          edge_t *e = edge;
          vertex_t *va = e->vert;
          do {
            e->vert = e->next->vert;
            e = e->next;
          } while (e != edge);
          edge->prev->vert = va;
        }

        {
          // swap prev and next pointers.
          edge_t *e = edge;
          do {
            edge_t *n = e->next;
            std::swap(e->prev, e->next);
            e = n;
          } while (e != edge);
        }

        plane.negate();

        int da = carve::geom::largestAxis(plane.N);

        project = getProjector(plane.N.v[da] > 0, da);
        unproject = getUnprojector(plane.N.v[da] > 0, da);
      }

      void canonicalize();

      ~Face() {
        clearEdges();
      }
    };



    struct MeshOptions {
      bool opt_avoid_cavities;

      MeshOptions() :
        opt_avoid_cavities(false) {
      }

      MeshOptions &avoid_cavities(bool val) {
        opt_avoid_cavities = val;
        return *this;
      }
    };



    namespace detail {
      class FaceStitcher {
        FaceStitcher();
        FaceStitcher(const FaceStitcher &);
        FaceStitcher &operator=(const FaceStitcher &);

        typedef Vertex<3> vertex_t;
        typedef Edge<3> edge_t;
        typedef Face<3> face_t;

        typedef std::pair<const vertex_t *, const vertex_t *> vpair_t;
        typedef std::list<edge_t *> edgelist_t;
        typedef std::unordered_map<vpair_t, edgelist_t, carve::mesh::hash_vertex_pair> edge_map_t;
        typedef std::unordered_map<const vertex_t *, std::set<const vertex_t *> > edge_graph_t;

        MeshOptions opts;

        edge_map_t edges;
        edge_map_t complex_edges;

        carve::djset::djset face_groups;
        std::vector<bool> is_open;

        edge_graph_t edge_graph;

        struct EdgeOrderData {
          size_t group_id;
          bool is_reversed;
          carve::geom::vector<3> face_dir;
          edge_t *edge;

          EdgeOrderData(edge_t *_edge, size_t _group_id, bool _is_reversed) :
            group_id(_group_id),
            is_reversed(_is_reversed) {
            if (is_reversed) {
              face_dir = -(_edge->face->plane.N);
            } else {
              face_dir =  (_edge->face->plane.N);
            }
            edge = _edge;
          }

          struct TestGroups {
            size_t fwd, rev;

            TestGroups(size_t _fwd, size_t _rev) : fwd(_fwd), rev(_rev) {
            }

            bool operator()(const EdgeOrderData &eo) const {
              return eo.group_id == (eo.is_reversed ? rev : fwd);
            }
          };

          struct Cmp {
            carve::geom::vector<3> edge_dir;
            carve::geom::vector<3> base_dir;

            Cmp(const carve::geom::vector<3> &_edge_dir,
                const carve::geom::vector<3> &_base_dir) :
              edge_dir(_edge_dir),
              base_dir(_base_dir) {
            }
            bool operator()(const EdgeOrderData &a, const EdgeOrderData &b) const;
          };
        };

        void extractConnectedEdges(std::vector<const vertex_t *>::iterator begin,
                                   std::vector<const vertex_t *>::iterator end,
                                   std::vector<std::vector<Edge<3> *> > &efwd,
                                   std::vector<std::vector<Edge<3> *> > &erev);

        size_t faceGroupID(const Face<3> *face);
        size_t faceGroupID(const Edge<3> *edge);

        void resolveOpenEdges();

        void fuseEdges(std::vector<Edge<3> *> &fwd,
                       std::vector<Edge<3> *> &rev);

        void joinGroups(std::vector<std::vector<Edge<3> *> > &efwd,
                        std::vector<std::vector<Edge<3> *> > &erev,
                        size_t fwd_grp,
                        size_t rev_grp);

        void matchOrderedEdges(const std::vector<std::vector<EdgeOrderData> >::iterator begin,
                               const std::vector<std::vector<EdgeOrderData> >::iterator end,
                               std::vector<std::vector<Edge<3> *> > &efwd,
                               std::vector<std::vector<Edge<3> *> > &erev);

        void reorder(std::vector<EdgeOrderData> &ordering, size_t fwd_grp);

        void orderForwardAndReverseEdges(std::vector<std::vector<Edge<3> *> > &efwd,
                                         std::vector<std::vector<Edge<3> *> > &erev,
                                         std::vector<std::vector<EdgeOrderData> > &result);

        void edgeIncidentGroups(const vpair_t &e,
                                const edge_map_t &all_edges,
                                std::pair<std::set<size_t>, std::set<size_t> > &groups);

        void buildEdgeGraph(const edge_map_t &all_edges);
        void extractPath(std::vector<const vertex_t *> &path);
        void removePath(const std::vector<const vertex_t *> &path);
        void matchSimpleEdges();
        void construct();

        template<typename iter_t>
        void initEdges(iter_t begin, iter_t end);

        template<typename iter_t>
        void build(iter_t begin, iter_t end, std::vector<Mesh<3> *> &meshes);

      public:
        FaceStitcher(const MeshOptions &_opts);

        template<typename iter_t>
        void create(iter_t begin, iter_t end, std::vector<Mesh<3> *> &meshes);
      };
    }



    // A Mesh is a connected set of faces. It may be open (some edges
    // have NULL rev members), or closed. On destruction, a Mesh
    // should free its Faces (which will in turn free Edges, but not
    // Vertices).  A Mesh is edge-connected, which is to say that each
    // face in the mesh shares an edge with at least one other face in
    // the mesh. Touching at a vertex is not sufficient. This means
    // that the perimeter of an open mesh visits each vertex no more
    // than once.
    template<unsigned ndim>
    class Mesh {
    public:
      typedef Vertex<ndim> vertex_t;
      typedef Edge<ndim> edge_t;
      typedef Face<ndim> face_t;
      typedef carve::geom::aabb<ndim> aabb_t;
      typedef MeshSet<ndim> meshset_t;

      std::vector<face_t *> faces;

      // open_edges is a vector of all the edges in the mesh that
      // don't have a matching edge in the opposite direction.
      std::vector<edge_t *> open_edges;

      // closed_edges is a vector of all the edges in the mesh that
      // have a matching edge in the opposite direction, and whose
      // address is lower than their counterpart. (i.e. for each pair
      // of adjoining faces, one of the two half edges is stored in
      // closed_edges).
      std::vector<edge_t *> closed_edges;

      bool is_negative;

      meshset_t *meshset;

    protected:
      Mesh(std::vector<face_t *> &_faces,
           std::vector<edge_t *> &_open_edges,
           std::vector<edge_t *> &_closed_edges,
           bool _is_negative);

    public:
      Mesh(std::vector<face_t *> &_faces);

      ~Mesh();

      template<typename iter_t>
      static void create(iter_t begin, iter_t end, std::vector<Mesh<ndim> *> &meshes, const MeshOptions &opts);

      aabb_t getAABB() const {
        return aabb_t(faces.begin(), faces.end());
      }

      bool isClosed() const {
        return open_edges.size() == 0;
      }

      bool isNegative() const {
        return is_negative;
      }

      double volume() const {
        if (is_negative || !faces.size()) return 0.0;

        double vol = 0.0;
        typename vertex_t::vector_t origin = faces[0]->edge->vert->v;

        for (size_t f = 0; f < faces.size(); ++f) {
          face_t *face = faces[f];
          edge_t *e1 = face->edge;
          for (edge_t *e2 = e1->next ;e2->next != e1; e2 = e2->next) {
            vol += carve::geom3d::tetrahedronVolume(e1->vert->v, e2->vert->v, e2->next->vert->v, origin);
          }
        }
        return vol;
      }

      struct IsClosed {
        bool operator()(const Mesh &mesh) const { return mesh.isClosed(); }
        bool operator()(const Mesh *mesh) const { return mesh->isClosed(); }
      };

      struct IsNegative {
        bool operator()(const Mesh &mesh) const { return mesh.isNegative(); }
        bool operator()(const Mesh *mesh) const { return mesh->isNegative(); }
      };

      void cacheEdges();

      int orientationAtVertex(edge_t *);
      void calcOrientation();

      void recalc() {
        for (size_t i = 0; i < faces.size(); ++i) faces[i]->recalc();
        calcOrientation();
      }

      void invert() {
        for (size_t i = 0; i < faces.size(); ++i) {
          faces[i]->invert();
        }
        if (isClosed()) is_negative = !is_negative;
      }

      Mesh *clone(const vertex_t *old_base, vertex_t *new_base) const;
    };

    // A MeshSet manages vertex storage, and a collection of meshes.
    // It should be easy to turn a vertex pointer into its index in
    // its MeshSet vertex_storage.
    template<unsigned ndim>
    class MeshSet {
      MeshSet();
      MeshSet(const MeshSet &);
      MeshSet &operator=(const MeshSet &);

      template<typename iter_t>
      void _init_from_faces(iter_t begin, iter_t end,  const MeshOptions &opts);

    public:
      typedef Vertex<ndim> vertex_t;
      typedef Edge<ndim> edge_t;
      typedef Face<ndim> face_t;
      typedef Mesh<ndim> mesh_t;
      typedef carve::geom::aabb<ndim> aabb_t;

      std::vector<vertex_t> vertex_storage;
      std::vector<mesh_t *> meshes;

    public:
      template<typename face_type>
      struct FaceIter : public std::iterator<std::random_access_iterator_tag, face_type> {
        typedef std::iterator<std::random_access_iterator_tag, face_type> super;
        typedef typename super::difference_type difference_type;

        const MeshSet<ndim> *obj;
        size_t mesh, face;

        FaceIter(const MeshSet<ndim> *_obj, size_t _mesh, size_t _face);

        void fwd(size_t n);
        void rev(size_t n);
        void adv(int n);

        FaceIter operator++(int) { FaceIter tmp = *this; tmp.fwd(1); return tmp; }
        FaceIter operator+(int v) { FaceIter tmp = *this; tmp.adv(v); return tmp; }
        FaceIter &operator++() { fwd(1); return *this; }
        FaceIter &operator+=(int v) { adv(v); return *this; }

        FaceIter operator--(int) { FaceIter tmp = *this; tmp.rev(1); return tmp; }
        FaceIter operator-(int v) { FaceIter tmp = *this; tmp.adv(-v); return tmp; }
        FaceIter &operator--() { rev(1); return *this; }
        FaceIter &operator-=(int v) { adv(-v); return *this; }

        difference_type operator-(const FaceIter &other) const;

        bool operator==(const FaceIter &other) const {
          return obj == other.obj && mesh == other.mesh && face == other.face;
        }
        bool operator!=(const FaceIter &other) const {
          return !(*this == other);
        }
        bool operator<(const FaceIter &other) const {
          CARVE_ASSERT(obj == other.obj);
          return mesh < other.mesh || (mesh == other.mesh && face < other.face);
        }
        bool operator>(const FaceIter &other) const {
          return other < *this;
        }
        bool operator<=(const FaceIter &other) const {
          return !(other < *this);
        }
        bool operator>=(const FaceIter &other) const {
          return !(*this < other);
        }

        face_type operator*() const {
          return obj->meshes[mesh]->faces[face];
        }
      };

      typedef FaceIter<const face_t *> const_face_iter;
      typedef FaceIter<face_t *> face_iter;

      face_iter faceBegin() { return face_iter(this, 0, 0); }
      face_iter faceEnd() { return face_iter(this, meshes.size(), 0); }

      const_face_iter faceBegin() const { return const_face_iter(this, 0, 0); }
      const_face_iter faceEnd() const { return const_face_iter(this, meshes.size(), 0); }

      aabb_t getAABB() const {
        return aabb_t(meshes.begin(), meshes.end());
      }

      template<typename func_t>
      void transform(func_t func) {
        for (size_t i = 0; i < vertex_storage.size(); ++i) {
          vertex_storage[i].v = func(vertex_storage[i].v);
        }
        for (size_t i = 0; i < meshes.size(); ++i) {
          meshes[i]->recalc();
        }
      }

      MeshSet(const std::vector<typename vertex_t::vector_t> &points,
              size_t n_faces,
              const std::vector<int> &face_indices,
              const MeshOptions &opts = MeshOptions());

      // Construct a mesh set from a set of disconnected faces. Takes
      // posession of the face pointers.
      MeshSet(std::vector<face_t *> &faces,
              const MeshOptions &opts = MeshOptions());

      MeshSet(std::list<face_t *> &faces,
              const MeshOptions &opts = MeshOptions());

      MeshSet(std::vector<vertex_t> &_vertex_storage,
              std::vector<mesh_t *> &_meshes);

      // This constructor consolidates and rewrites vertex pointers in
      // each mesh, repointing them to local storage.
      MeshSet(std::vector<mesh_t *> &_meshes);

      MeshSet *clone() const;

      ~MeshSet();

      bool isClosed() const {
        for (size_t i = 0; i < meshes.size(); ++i) {
          if (!meshes[i]->isClosed()) return false;
        }
        return true;
      }


      void invert() {
        for (size_t i = 0; i < meshes.size(); ++i) {
          meshes[i]->invert();
        }
      }

      void collectVertices();

      void canonicalize();

      void separateMeshes();
    };



    carve::PointClass classifyPoint(
        const carve::mesh::MeshSet<3> *meshset,
        const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *face_rtree,
        const carve::geom::vector<3> &v,
        bool even_odd = false,
        const carve::mesh::Mesh<3> *mesh = NULL,
        const carve::mesh::Face<3> **hit_face = NULL);



  }



  mesh::MeshSet<3> *meshFromPolyhedron(const poly::Polyhedron *, int manifold_id);
  poly::Polyhedron *polyhedronFromMesh(const mesh::MeshSet<3> *, int manifold_id);



};

#include <carve/mesh_impl.hpp>
