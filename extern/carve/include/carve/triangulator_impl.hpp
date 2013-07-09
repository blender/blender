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

#include <carve/geom2d.hpp>

#if defined(CARVE_DEBUG)
#  include <iostream>
#endif

namespace carve {
  namespace triangulate {
    namespace detail {



      static inline bool axisOrdering(const carve::geom2d::P2 &a,
                                      const carve::geom2d::P2 &b,
                                      int axis) {
          return a.v[axis] < b.v[axis] || (a.v[axis] == b.v[axis] && a.v[1-axis] < b.v[1-axis]);
      }



      /**
       * \class order_h_loops
       * \brief Provides an ordering of hole loops based upon a single
       * projected axis.
       *
       * @tparam project_t A functor which converts vertices to a 2d
       *                   projection.
       * @tparam hole_t A collection of vertices.
       */
      template<typename project_t, typename vert_t>
      class order_h_loops {
        const project_t &project;
        int axis;
      public:

        /**
         * 
         * @param _project The projection functor.
         * @param _axis The axis of the 2d projection upon which hole
         *              loops are ordered.
         */
        order_h_loops(const project_t &_project, int _axis) : project(_project), axis(_axis) { }

        bool operator()(const vert_t &a,
                        const vert_t &b) const {
          return axisOrdering(project(a), project(b), axis);
        }

        bool operator()(
            const std::pair<const typename std::vector<vert_t> *, typename std::vector<vert_t>::const_iterator> &a,
            const std::pair<const typename std::vector<vert_t> *, typename std::vector<vert_t>::const_iterator> &b) {
          return axisOrdering(project(*(a.second)), project(*(b.second)), axis);
        }
      };



      /**
       * \class heap_ordering
       * \brief Provides an ordering of vertex indicies in a polygon
       * loop according to proximity to a vertex.
       *
       * @tparam project_t A functor which converts vertices to a 2d
       *                   projection.
       * @tparam vert_t A vertex type.
       */
      template<typename project_t, typename vert_t>
      class heap_ordering {
        const project_t &project;
        const std::vector<vert_t> &loop;
        const carve::geom2d::P2 p;
        int axis;

      public:
        /** 
         * 
         * @param _project A functor which converts vertices to a 2d
         *                 projection.
         * @param _loop The polygon loop which indices address.
         * @param _vert The vertex from which distance is measured.
         * 
         */
        heap_ordering(const project_t &_project,
                      const std::vector<vert_t> &_loop,
                      vert_t _vert,
                      int _axis) :
            project(_project),
            loop(_loop),
            p(_project(_vert)),
            axis(_axis) {
        }

        bool operator()(size_t a, size_t b) const {
          carve::geom2d::P2 pa = project(loop[a]);
          carve::geom2d::P2 pb = project(loop[b]);
          double da = carve::geom::distance2(p, pa);
          double db = carve::geom::distance2(p, pb);
          if (da > db) return true;
          if (da < db) return false;
          return axisOrdering(pa, pb, axis);
        }
      };



      /** 
       * \brief Given a polygon loop and a hole loop, and attachment
       * points, insert the hole loop vertices into the polygon loop.
       * 
       * @param[in,out] f_loop The polygon loop to incorporate the
       *                       hole into.
       * @param f_loop_attach[in] The index of the vertex of the
       *                          polygon loop that the hole is to be
       *                          attached to.
       * @param hole_attach[in] A pair consisting of a pointer to a
       *                        hole container and an iterator into
       *                        that container reflecting the point of
       *                        attachment of the hole.
       */
      template<typename vert_t>
      void patchHoleIntoPolygon(std::vector<vert_t> &f_loop,
                             unsigned f_loop_attach,
                             const std::pair<const std::vector<vert_t> *,
                                             typename std::vector<vert_t>::const_iterator> &hole_attach) {
        // join the vertex curr of the polygon loop to the hole at
        // h_loop_connect
        f_loop.insert(f_loop.begin() + f_loop_attach + 1, hole_attach.first->size() + 2, NULL);
        typename std::vector<vert_t>::iterator f = f_loop.begin() + f_loop_attach;

        typename std::vector<vert_t>::const_iterator h = hole_attach.second;

        while (h != hole_attach.first->end()) {
          *++f = *h++;
        }

        h = hole_attach.first->begin();
        typename std::vector<vert_t>::const_iterator he = hole_attach.second; ++he;
        while (h != he) {
          *++f = *h++;
        }

        *++f = f_loop[f_loop_attach];
      }



      struct vertex_info;



      /** 
       * \brief Determine whether c is to the left of a->b.
       */
      static inline bool isLeft(const vertex_info *a,
                                const vertex_info *b,
                                const vertex_info *c);



      /** 
       * \brief Determine whether d is contained in the triangle abc.
       */
      static inline bool pointInTriangle(const vertex_info *a,
                                         const vertex_info *b,
                                         const vertex_info *c,
                                         const vertex_info *d);



      /**
       * \class vertex_info
       * \brief Maintains a linked list of untriangulated vertices
       * during a triangulation operation.
       */

      struct vertex_info {
        vertex_info *prev;
        vertex_info *next;
        carve::geom2d::P2 p;
        size_t idx;
        double score;
        bool convex;
        bool failed;

        vertex_info(const carve::geom2d::P2 &_p, size_t _idx) :
          prev(NULL), next(NULL),
          p(_p), idx(_idx),
          score(0.0), convex(false) {
        }

        static double triScore(const vertex_info *p, const vertex_info *v, const vertex_info *n);

        double calcScore() const;

        void recompute() {
          score = calcScore();
          convex = isLeft(prev, this, next);
          failed = false;
        }

        bool isCandidate() const {
          return convex && !failed;
        }

        void remove() {
          next->prev = prev;
          prev->next = next;
        }
    
        bool isClipable() const;
      };



      static inline bool isLeft(const vertex_info *a,
                                const vertex_info *b,
                                const vertex_info *c) {
        if (a->idx < b->idx && b->idx < c->idx) {
          return carve::geom2d::orient2d(a->p, b->p, c->p) > 0.0;
        } else if (a->idx < c->idx && c->idx < b->idx) {
          return carve::geom2d::orient2d(a->p, c->p, b->p) < 0.0;
        } else if (b->idx < a->idx && a->idx < c->idx) {
          return carve::geom2d::orient2d(b->p, a->p, c->p) < 0.0;
        } else if (b->idx < c->idx && c->idx < a->idx) {
          return carve::geom2d::orient2d(b->p, c->p, a->p) > 0.0;
        } else if (c->idx < a->idx && a->idx < b->idx) {
          return carve::geom2d::orient2d(c->p, a->p, b->p) > 0.0;
        } else {
          return carve::geom2d::orient2d(c->p, b->p, a->p) < 0.0;
        }
      }



      static inline bool pointInTriangle(const vertex_info *a,
                                         const vertex_info *b,
                                         const vertex_info *c,
                                         const vertex_info *d) {
        return !isLeft(a, c, d) && !isLeft(b, a, d) && !isLeft(c, b, d);
      }



      size_t removeDegeneracies(vertex_info *&begin, std::vector<carve::triangulate::tri_idx> &result);

      bool splitAndResume(vertex_info *begin, std::vector<carve::triangulate::tri_idx> &result);

      bool doTriangulate(vertex_info *begin, std::vector<carve::triangulate::tri_idx> &result);



      typedef std::pair<unsigned, unsigned> vert_edge_t;



      struct hash_vert_edge_t {
        size_t operator()(const vert_edge_t &e) const {
          size_t r = (size_t)e.first;
          size_t s = (size_t)e.second;
          return r ^ ((s >> 16) | (s << 16));
        }
      };



      static inline vert_edge_t ordered_vert_edge_t(unsigned a, unsigned b) {
        return (a < b) ? vert_edge_t(a, b) : vert_edge_t(b, a);
      }



      struct tri_pair_t {
        carve::triangulate::tri_idx *a, *b;
        double score;
        size_t idx;

        tri_pair_t() : a(NULL), b(NULL), score(0.0) {
        }

        static inline unsigned N(unsigned i) { return (i+1)%3; }
        static inline unsigned P(unsigned i) { return (i+2)%3; }

        void findSharedEdge(unsigned &ai, unsigned &bi) const {
          if (a->v[1] == b->v[0]) { if (a->v[0] == b->v[1]) { ai = 0; bi = 0; } else { ai = 1; bi = 2; } return; }
          if (a->v[1] == b->v[1]) { if (a->v[0] == b->v[2]) { ai = 0; bi = 1; } else { ai = 1; bi = 0; } return; }
          if (a->v[1] == b->v[2]) { if (a->v[0] == b->v[0]) { ai = 0; bi = 2; } else { ai = 1; bi = 1; } return; }
          if (a->v[2] == b->v[0]) { ai = 2; bi = 2; return; }
          if (a->v[2] == b->v[1]) { ai = 2; bi = 0; return; }
          if (a->v[2] == b->v[2]) { ai = 2; bi = 1; return; }
          CARVE_FAIL("should not be reached");
        }

        void flip(vert_edge_t &old_edge,
                  vert_edge_t &new_edge,
                  vert_edge_t perim[4]);

        template<typename project_t, typename vert_t, typename distance_calc_t>
        double calc(const project_t &project,
                    const std::vector<vert_t> &poly,
                    distance_calc_t dist) {
          unsigned ai, bi;
          unsigned cross_ai, cross_bi;
          unsigned ea, eb;

          findSharedEdge(ai, bi);

#if defined(CARVE_DEBUG)
          if (carve::geom2d::signedArea(project(poly[a->v[0]]), project(poly[a->v[1]]), project(poly[a->v[2]])) > 0.0 ||
              carve::geom2d::signedArea(project(poly[b->v[0]]), project(poly[b->v[1]]), project(poly[b->v[2]])) > 0.0) {
            std::cerr << "warning: triangle pair " << this << " contains triangles with incorrect orientation" << std::endl;
          }
#endif

          cross_ai = P(ai);
          cross_bi = P(bi);

          ea = a->v[cross_ai];
          eb = b->v[cross_bi];

          double side_1 = carve::geom2d::orient2d(project(poly[ea]), project(poly[eb]), project(poly[a->v[ai]]));
          double side_2 = carve::geom2d::orient2d(project(poly[ea]), project(poly[eb]), project(poly[a->v[N(ai)]]));

          bool can_flip = (side_1 < 0.0 && side_2 > 0.0) || (side_1 > 0.0 && side_2 < 0.0);

          if (!can_flip) {
            score = -1;
          } else {
            score =
              dist(poly[a->v[ai]], poly[b->v[bi]]) -
              dist(poly[a->v[cross_ai]], poly[b->v[cross_bi]]);
          }
          return score;
        }

        template<typename project_t, typename vert_t, typename distance_calc_t>
        double edgeLen(const project_t &project,
                       const std::vector<vert_t> &poly,
                       distance_calc_t dist) const {
          unsigned ai, bi;
          findSharedEdge(ai, bi);
          return dist(poly[a->v[ai]], poly[b->v[bi]]);
        }
      };



      struct max_score {
        bool operator()(const tri_pair_t *a, const tri_pair_t *b) const { return a->score < b->score; }
      };



      struct tri_pairs_t {
        typedef std::unordered_map<vert_edge_t, tri_pair_t *, hash_vert_edge_t> storage_t;
        storage_t storage;

        tri_pairs_t() : storage() {
        };

        ~tri_pairs_t() {
          for (storage_t::iterator i = storage.begin(); i != storage.end(); ++i) {
            if ((*i).second) delete (*i).second;
          }
        }

        void insert(unsigned a, unsigned b, carve::triangulate::tri_idx *t);

        template<typename project_t, typename vert_t, typename distance_calc_t>
        void updateEdge(tri_pair_t *tp,
                        const project_t &project,
                        const std::vector<vert_t> &poly,
                        distance_calc_t dist,
                        std::vector<tri_pair_t *> &edges,
                        size_t &n) {
          double old_score = tp->score;
          double new_score = tp->calc(project, poly, dist);
#if defined(CARVE_DEBUG)
          std::cerr << "tp:" << tp << " old_score: " << old_score << " new_score: " << new_score << std::endl;
#endif
          if (new_score > 0.0 && old_score <= 0.0) {
            tp->idx = n;
            edges[n++] = tp;
          } else if (new_score <= 0.0 && old_score > 0.0) {
            std::swap(edges[tp->idx], edges[--n]);
            edges[tp->idx]->idx = tp->idx;
          }
        }

        tri_pair_t *get(vert_edge_t &e) {
          storage_t::iterator i;
          i = storage.find(e);
          if (i == storage.end()) return NULL;
          return (*i).second;
        }

        template<typename project_t, typename vert_t, typename distance_calc_t>
        void flip(const project_t &project,
                  const std::vector<vert_t> &poly,
                  distance_calc_t dist,
                  std::vector<tri_pair_t *> &edges,
                  size_t &n) {
          vert_edge_t old_e, new_e;
          vert_edge_t perim[4];

#if defined(CARVE_DEBUG)
          std::cerr << "improvable edges: " << n << std::endl;
#endif

          tri_pair_t *tp = *std::max_element(edges.begin(), edges.begin() + n, max_score());

#if defined(CARVE_DEBUG)
          std::cerr << "improving tri-pair: " << tp << " with score: " << tp->score << std::endl;
#endif

          tp->flip(old_e, new_e, perim);

#if defined(CARVE_DEBUG)
          std::cerr << "old_e: " << old_e.first << "," << old_e.second << " -> new_e: " << new_e.first << "," << new_e.second << std::endl;
#endif

          CARVE_ASSERT(storage.find(old_e) != storage.end());
          storage.erase(old_e);
          storage[new_e] = tp;

          std::swap(edges[tp->idx], edges[--n]);
          edges[tp->idx]->idx = tp->idx;

          tri_pair_t *tp2;

          tp2 = get(perim[0]);
          if (tp2 != NULL) {
            updateEdge(tp2, project, poly, dist, edges, n);
          }

          tp2 = get(perim[1]);
          if (tp2 != NULL) {
            CARVE_ASSERT(tp2->a == tp->b || tp2->b == tp->b);
            if (tp2->a == tp->b) { tp2->a = tp->a; } else { tp2->b = tp->a; }
            updateEdge(tp2, project, poly, dist, edges, n);
          }

          tp2 = get(perim[2]);
          if (tp2 != NULL) {
            updateEdge(tp2, project, poly, dist, edges, n);
          }

          tp2 = get(perim[3]);
          if (tp2 != NULL) {
            CARVE_ASSERT(tp2->a == tp->a || tp2->b == tp->a);
            if (tp2->a == tp->a) { tp2->a = tp->b; } else { tp2->b = tp->b; }
            updateEdge(tp2, project, poly, dist, edges, n);
          }
        }

        template<typename project_t, typename vert_t, typename distance_calc_t>
        size_t getInternalEdges(const project_t &project,
                                const std::vector<vert_t> &poly,
                                distance_calc_t dist,
                                std::vector<tri_pair_t *> &edges) {
          size_t count = 0;

          for (storage_t::iterator i = storage.begin(); i != storage.end();) {
            tri_pair_t *tp = (*i).second;
            if (tp->a && tp->b) {
              tp->calc(project, poly, dist);
              count++;
#if defined(CARVE_DEBUG)
              std::cerr << "internal edge: " << (*i).first.first << "," << (*i).first.second << " -> " << tp << " " << tp->score << std::endl;
#endif
              ++i;
            } else {
              delete (*i).second;
              storage.erase(i++);
            }
          }

          edges.resize(count);

          size_t fwd = 0;
          size_t rev = count;
          for (storage_t::iterator i = storage.begin(); i != storage.end(); ++i) {
            tri_pair_t *tp = (*i).second;
            if (tp && tp->a && tp->b) {
              if (tp->score > 0.0) {
                edges[fwd++] = tp;
              } else {
                edges[--rev] = tp;
              }
            }
          }

          CARVE_ASSERT(fwd == rev);

          return fwd;
        }
      };



      template<typename project_t, typename vert_t>
      static bool
      testCandidateAttachment(const project_t &project,
                              std::vector<vert_t> &current_f_loop,
                              size_t curr,
                              carve::geom2d::P2 hole_min) {
        const size_t SZ = current_f_loop.size();

        size_t prev, next;

        if (curr == 0) {
          prev = SZ - 1; next = 1;
        } else if (curr == SZ - 1) {
          prev = curr - 1; next = 0;
        } else {
          prev = curr - 1; next = curr + 1;
        }

        if (!carve::geom2d::internalToAngle(project(current_f_loop[next]),
                                            project(current_f_loop[curr]),
                                            project(current_f_loop[prev]),
                                            hole_min)) {
          return false;
        }

        if (hole_min == project(current_f_loop[curr])) {
          return true;
        }

        carve::geom2d::LineSegment2 test(hole_min, project(current_f_loop[curr]));

        size_t v1 = current_f_loop.size() - 1;
        size_t v2 = 0;
        double v1_side = carve::geom2d::orient2d(test.v1, test.v2, project(current_f_loop[v1]));
        double v2_side = 0;

        while (v2 != current_f_loop.size()) {
          v2_side = carve::geom2d::orient2d(test.v1, test.v2, project(current_f_loop[v2]));

          if (v1_side != v2_side) {
            // XXX: need to test vertices, not indices, because they may
            // be duplicated.
            if (project(current_f_loop[v1]) != project(current_f_loop[curr]) &&
                project(current_f_loop[v2]) != project(current_f_loop[curr])) {
              carve::geom2d::LineSegment2 test2(project(current_f_loop[v1]), project(current_f_loop[v2]));
              if (carve::geom2d::lineSegmentIntersection_simple(test, test2)) {
                // intersection; failed.
                return false;
              }
            }
          }

          v1 = v2;
          v1_side = v2_side;
          ++v2;
        }
        return true;
      }



    }



    template<typename project_t, typename vert_t>
    static std::vector<vert_t>
    incorporateHolesIntoPolygon(const project_t &project,
                                const std::vector<vert_t> &f_loop,
                                const std::vector<std::vector<vert_t> > &h_loops) {
      typedef std::vector<vert_t> hole_t;
      typedef typename std::vector<vert_t>::const_iterator vert_iter;
      typedef typename std::vector<std::vector<vert_t> >::const_iterator hole_iter;

      size_t N = f_loop.size();

      // work out how much space to reserve for the patched in holes.
      for (hole_iter i = h_loops.begin(); i != h_loops.end(); ++i) {
        N += 2 + (*i).size();
      }
    
      // this is the vector that we will build the result in.
      std::vector<vert_t> current_f_loop;
      current_f_loop.reserve(N);

      std::vector<size_t> f_loop_heap;
      f_loop_heap.reserve(N);

      for (unsigned i = 0; i < f_loop.size(); ++i) {
        current_f_loop.push_back(f_loop[i]);
      }

      std::vector<std::pair<const std::vector<vert_t> *, vert_iter> > h_loop_min_vertex;

      h_loop_min_vertex.reserve(h_loops.size());

      // find the major axis for the holes - this is the axis that we
      // will sort on for finding vertices on the polygon to join
      // holes up to.
      //
      // it might also be nice to also look for whether it is better
      // to sort ascending or descending.
      // 
      // another trick that could be used is to modify the projection
      // by 90 degree rotations or flipping about an axis. just as
      // long as we keep the carve::geom3d::Vector pointers for the
      // real data in sync, everything should be ok. then we wouldn't
      // need to accomodate axes or sort order in the main loop.

      // find the bounding box of all the holes.
      bool first = true;
      double min_x, min_y, max_x, max_y;
      for (hole_iter i = h_loops.begin(); i != h_loops.end(); ++i) {
        const hole_t &hole(*i);
        for (vert_iter j = hole.begin(); j != hole.end(); ++j) {
          carve::geom2d::P2 curr = project(*j);
          if (first) {
            min_x = max_x = curr.x;
            min_y = max_y = curr.y;
            first = false;
          } else {
            min_x = std::min(min_x, curr.x);
            min_y = std::min(min_y, curr.y);
            max_x = std::max(max_x, curr.x);
            max_y = std::max(max_y, curr.y);
          }
        }
      }

      // choose the axis for which the bbox is largest.
      int axis = (max_x - min_x) > (max_y - min_y) ? 0 : 1;

      // for each hole, find the minimum vertex in the chosen axis.
      for (hole_iter i = h_loops.begin(); i != h_loops.end(); ++i) {
        const hole_t &hole = *i;
        vert_iter best_i = std::min_element(hole.begin(), hole.end(), detail::order_h_loops<project_t, vert_t>(project, axis));
        h_loop_min_vertex.push_back(std::make_pair(&hole, best_i));
      }

      // sort the holes by the minimum vertex.
      std::sort(h_loop_min_vertex.begin(), h_loop_min_vertex.end(), detail::order_h_loops<project_t, vert_t>(project, axis));

      // now, for each hole, find a vertex in the current polygon loop that it can be joined to.
      for (unsigned i = 0; i < h_loop_min_vertex.size(); ++i) {
        const size_t N_f_loop = current_f_loop.size();

        // the index of the vertex in the hole to connect.
        vert_iter h_loop_connect = h_loop_min_vertex[i].second;

        carve::geom2d::P2 hole_min = project(*h_loop_connect);

        f_loop_heap.clear();
        // we order polygon loop vertices that may be able to be connected
        // to the hole vertex by their distance to the hole vertex
        detail::heap_ordering<project_t, vert_t> _heap_ordering(project, current_f_loop, *h_loop_connect, axis);

        for (size_t j = 0; j < N_f_loop; ++j) {
          // it is guaranteed that there exists a polygon vertex with
          // coord < the min hole coord chosen, which can be joined to
          // the min hole coord without crossing the polygon
          // boundary. also, because we merge holes in ascending
          // order, it is also true that this join can never cross
          // another hole (and that doesn't need to be tested for).
          if (project(current_f_loop[j]).v[axis] <= hole_min.v[axis]) {
            f_loop_heap.push_back(j);
            std::push_heap(f_loop_heap.begin(), f_loop_heap.end(), _heap_ordering);
          }
        }

        // we are going to test each potential (according to the
        // previous test) polygon vertex as a candidate join. we order
        // by closeness to the hole vertex, so that the join we make
        // is as small as possible. to test, we need to check the
        // joining line segment does not cross any other line segment
        // in the current polygon loop (excluding those that have the
        // vertex that we are attempting to join with as an endpoint).
        size_t attachment_point = current_f_loop.size();

        while (f_loop_heap.size()) {
          std::pop_heap(f_loop_heap.begin(), f_loop_heap.end(), _heap_ordering);
          size_t curr = f_loop_heap.back();
          f_loop_heap.pop_back();
          // test the candidate join from current_f_loop[curr] to hole_min

          if (!detail::testCandidateAttachment(project, current_f_loop, curr, hole_min)) {
            continue;
          }

          attachment_point = curr;
          break;
        }

        if (attachment_point == current_f_loop.size()) {
          CARVE_FAIL("didn't manage to link up hole!");
        }

        detail::patchHoleIntoPolygon(current_f_loop, attachment_point, h_loop_min_vertex[i]);
      }

      return current_f_loop;
    }



    template<typename project_t, typename vert_t>
    void triangulate(const project_t &project,
                     const std::vector<vert_t> &poly,
                     std::vector<tri_idx> &result) {
      std::vector<detail::vertex_info *> vinfo;
      const size_t N = poly.size();

      result.clear();
      if (N < 3) {
        return;
      }

      result.reserve(poly.size() - 2);

      if (N == 3) {
        result.push_back(tri_idx(0, 1, 2));
        return;
      }

      vinfo.resize(N);

      vinfo[0] = new detail::vertex_info(project(poly[0]), 0);
      for (size_t i = 1; i < N-1; ++i) {
        vinfo[i] = new detail::vertex_info(project(poly[i]), i);
        vinfo[i]->prev = vinfo[i-1];
        vinfo[i-1]->next = vinfo[i];
      }
      vinfo[N-1] = new detail::vertex_info(project(poly[N-1]), N-1);
      vinfo[N-1]->prev = vinfo[N-2];
      vinfo[N-1]->next = vinfo[0];
      vinfo[0]->prev = vinfo[N-1];
      vinfo[N-2]->next = vinfo[N-1];

      for (size_t i = 0; i < N; ++i) {
        vinfo[i]->recompute();
      }

      detail::vertex_info *begin = vinfo[0];

      removeDegeneracies(begin, result);
      doTriangulate(begin, result);
    }



    template<typename project_t, typename vert_t, typename distance_calc_t>
    void improve(const project_t &project,
                 const std::vector<vert_t> &poly,
                 distance_calc_t dist,
                 std::vector<tri_idx> &result) {
      detail::tri_pairs_t tri_pairs;

#if defined(CARVE_DEBUG)
      bool warn = false;
      for (size_t i = 0; i < result.size(); ++i) {
        tri_idx &t = result[i];
        if (carve::geom2d::signedArea(project(poly[t.a]), project(poly[t.b]), project(poly[t.c])) > 0) {
          warn = true;
        }
      } 
      if (warn) {
        std::cerr << "carve::triangulate::improve(): Some triangles are incorrectly oriented. Results may be incorrect." << std::endl;
      }
#endif

      for (size_t i = 0; i < result.size(); ++i) {
        tri_idx &t = result[i];
        tri_pairs.insert(t.a, t.b, &t);
        tri_pairs.insert(t.b, t.c, &t);
        tri_pairs.insert(t.c, t.a, &t);
      }

      std::vector<detail::tri_pair_t *> edges;
      size_t n = tri_pairs.getInternalEdges(project, poly, dist, edges);
      for (size_t i = 0; i < n; ++i) {
        edges[i]->idx = i;
      }

      // procedure:
      // while a tri pair with a positive score exists:
      //   p = pair with highest positive score
      //   flip p, rewriting its two referenced triangles.
      //   negate p's score
      //   for each q in the up-to-four adjoining tri pairs:
      //     update q's tri ptr, if changed, and its score.

#if defined(CARVE_DEBUG)
      double initial_score = 0;
      for (size_t i = 0; i < edges.size(); ++i) {
        initial_score += edges[i]->edgeLen(project, poly, dist);
      }
      std::cerr << "initial score: " << initial_score << std::endl;
#endif

      while (n) {
        tri_pairs.flip(project, poly, dist, edges, n);
      }

#if defined(CARVE_DEBUG)
      double final_score = 0;
      for (size_t i = 0; i < edges.size(); ++i) {
        final_score += edges[i]->edgeLen(project, poly, dist);
      }
      std::cerr << "final score: " << final_score << std::endl;
#endif

#if defined(CARVE_DEBUG)
      if (!warn) {
        for (size_t i = 0; i < result.size(); ++i) {
          tri_idx &t = result[i];
          CARVE_ASSERT (carve::geom2d::signedArea(project(poly[t.a]), project(poly[t.b]), project(poly[t.c])) <= 0.0);
        } 
      }
#endif
    }



    template<typename project_t, typename vert_t>
    void improve(const project_t &project,
                 const std::vector<vert_t> &poly,
                 std::vector<tri_idx> &result) {
      improve(project, poly, carve::geom::distance_functor(), result);
    }



  }
}
