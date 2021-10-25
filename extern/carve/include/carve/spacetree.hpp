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

#include <carve/geom.hpp>
#include <carve/aabb.hpp>
#include <carve/vertex_decl.hpp>
#include <carve/edge_decl.hpp>
#include <carve/face_decl.hpp>

namespace carve {

  namespace space {

    static inline bool intersection_test(const carve::geom::aabb<3> &aabb, const carve::poly::Face<3> *face) {
      if (face->nVertices() == 3) {
        return aabb.intersects(carve::geom::tri<3>(face->vertex(0)->v, face->vertex(1)->v, face->vertex(2)->v));
      } else {
        // partial, conservative SAT.
        return aabb.intersects(face->aabb) && aabb.intersects(face->plane_eqn);
      }
    }

    static inline bool intersection_test(const carve::geom::aabb<3> &aabb, const carve::poly::Edge<3> *edge) {
      return aabb.intersectsLineSegment(edge->v1->v, edge->v2->v);
    }

    static inline bool intersection_test(const carve::geom::aabb<3> &aabb, const carve::poly::Vertex<3> *vertex) {
      return aabb.intersects(vertex->v);
    }



    struct nodedata_FaceEdge {
      std::vector<const carve::poly::Face<3> *> faces;
      std::vector<const carve::poly::Edge<3> *> edges;

      void add(const carve::poly::Face<3> *face) {
        faces.push_back(face);
      }

      void add(const carve::poly::Edge<3> *edge) {
        edges.push_back(edge);
      }

      template<typename iter_t>
      void _fetch(iter_t &iter, const carve::poly::Edge<3> *) {
        std::copy(edges.begin(), edges.end(), iter);
      }

      template<typename iter_t>
      void _fetch(iter_t &iter, const carve::poly::Face<3> *) {
        std::copy(faces.begin(), faces.end(), iter);
      }

      template<typename node_t>
      void propagate(node_t *node) {
      }

      template<typename iter_t>
      void fetch(iter_t &iter) {
        return _fetch(iter, std::iterator_traits<iter_t>::value_type);
      }
    };



    const static double SLACK_FACTOR = 1.0009765625;
    const static unsigned MAX_SPLIT_DEPTH = 32;



    template<unsigned n_dim, typename nodedata_t>
    class SpatialSubdivTree {

      typedef carve::geom::aabb<n_dim> aabb_t;
      typedef carve::geom::vector<n_dim> vector_t;

    public:

      class Node {
        enum {
          n_children = 1 << n_dim
        };

      public:
        Node *parent;
        Node *children;

        vector_t min;
        vector_t max;

        aabb_t aabb;

        nodedata_t data;

      private:
        Node(const Node &node); // undefined.
        Node &operator=(const Node &node); // undefined.

        Node() {
        }

        inline aabb_t makeAABB() const {
          vector_t centre = 0.5 * (min + max);
          vector_t size = SLACK_FACTOR * 0.5 * (max - min);
          return aabb_t(centre, size);
        }

        void setup(Node *_parent, const vector_t &_min, const vector_t &_max) {
          parent = _parent;
          min = _min;
          max = _max;
          aabb = makeAABB();
        }

        void alloc_children() {
          vector_t mid = 0.5 * (min + max);
          children = new Node[n_children];
          for (size_t i = 0; i < (n_children); ++i) {
            vector_t new_min, new_max;
            for (size_t c = 0; c < n_dim; ++c) {
              if (i & (1 << c)) {
                new_min.v[c] = min.v[c];
                new_max.v[c] = mid.v[c];
              } else {
                new_min.v[c] = mid.v[c];
                new_max.v[c] = max.v[c];
              }
            }
            children[i].setup(this, new_min, new_max);
          }
        }

        void dealloc_children() {
          delete [] children;
        }

      public:

        inline bool isLeaf() const { return children == NULL; }

        Node(Node *_parent, const vector_t &_min, const vector_t &_max) : parent(_parent), children(NULL), min(_min), max(_max) {
          aabb = makeAABB();
        }

        ~Node() {
          dealloc_children();
        }

        bool split() {
          if (isLeaf()) {
            alloc_children();
            data.propagate(this);
          }
          return isLeaf();
        }

        template<typename obj_t>
        void insert(const obj_t &object) {
          if (!isLeaf()) {
            for (size_t i = 0; i < n_children; ++i) {
              if (intersection_test(children[i].aabb, object)) {
                children[i].insert(object);
              }
            }
          } else {
            data.add(object);
          }
        }

        template<typename obj_t>
        void insertVector(typename std::vector<obj_t>::iterator beg, typename std::vector<obj_t>::iterator end) {
          if (isLeaf()) {
            while (beg != end) {
              data.add(*beg);
            }
          } else {
            for (size_t i = 0; i < n_children; ++i) {
              typename std::vector<obj_t>::iterator mid = std::partition(beg, end, std::bind1st(intersection_test, children[i].aabb));
              children[i].insertVector(beg, mid);
            }
          }
        }

        template<typename iter_t>
        void insertMany(iter_t begin, iter_t end) {
          if (isLeaf()) {
          }
        }

        template<typename obj_t, typename iter_t, typename filter_t>
        void findObjectsNear(const obj_t &object, iter_t &output, filter_t filter) {
          if (!isLeaf()) {
            for (size_t i = 0; i < n_children; ++i) {
              if (intersection_test(children[i].aabb, object)) {
                children[i].findObjectsNear(object, output, filter);
              }
            }
            return;
          }
          data.fetch(output);
        }

        // bool hasGeometry();

        // template <class T>
        // void putInside(const T &input, Node *child, T &output);

      };



      Node *root;

      SpatialSubdivTree(const vector_t &_min, const vector_t &_max) : root(new Node(NULL, _min, _max)) {
      }

      ~SpatialSubdivTree() {
        delete root;
      }

      struct no_filter {
        template<typename obj_t>
        bool operator()(const obj_t &obj) const {
          return true;
        }
      };

      struct tag_filter {
        template<typename obj_t>
        bool operator()(const obj_t &obj) const {
          return obj.tag_once();
        }
      };

      // in order to be used as an input, aabb_t::intersect(const obj_t &) must exist.
      template<typename obj_t, typename iter_t, typename filter_t>
      void findObjectsNear(const obj_t &object, iter_t output, filter_t filter) {
        if (!intersection_test(root->aabb, object)) return;
        root->findObjectsNear(root, object, output, filter);
      }

    };

  }
}
