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

#include <carve/mesh.hpp>

#include <iostream>
#include <fstream>

namespace carve {
  namespace mesh {
    namespace detail {
      // make a triangle out of three edges.
      template<unsigned ndim>
      void link(Edge<ndim> *e1, Edge<ndim> *e2, Edge<ndim> *e3, Face<ndim> *f = NULL) {
        e1->next = e2; e2->next = e3; e3->next = e1;
        e3->prev = e2; e2->prev = e1; e1->prev = e3;
        e1->face = e2->face = e3->face = f;
        if (f) {
          f->edge = e1;
          f->recalc();
        }
      }



      template<unsigned ndim, typename proj_t>
      double loopArea(carve::mesh::Edge<ndim> *edge, proj_t proj) {
        double A = 0.0;
        carve::mesh::Edge<3> *e = edge;
        do {
          carve::geom2d::P2 p1 = proj(e->vert->v);
          carve::geom2d::P2 p2 = proj(e->next->vert->v);
          A += (p2.y + p1.y) * (p2.x - p1.x);
          e = e->next;
        } while (e != edge);
        return A / 2.0;
      }


      
      template<unsigned ndim, typename proj_t>
      struct TriangulationData {
        typedef Edge<ndim> edge_t;

        struct VertexInfo {
          double score;
          carve::geom2d::P2 p;
          bool convex;
          bool failed;
          VertexInfo *next, *prev;
          edge_t *edge;

          VertexInfo(edge_t *_edge,
                     const carve::geom2d::P2 &_p) :
              score(0.0), p(_p), convex(false), failed(false), next(NULL), prev(NULL), edge(_edge) {
          }

          bool isCandidate() const {
            return convex && !failed;
          }

          void fail() {
            failed = true;
          }

          static bool isLeft(const VertexInfo *a, const VertexInfo *b, const geom2d::P2 &p) {
            if (a < b) {
              return carve::geom2d::orient2d(a->p, b->p, p) > 0.0;
            } else {
              return carve::geom2d::orient2d(b->p, a->p, p) < 0.0;
            }
          }

          // is the ear prev->edge->next convex?
          bool testForConvexVertex() const {
            return isLeft(next, prev, p);
          }

          static double triScore(const geom2d::P2 &a, const geom2d::P2 &b, const geom2d::P2 &c) {
            // score is in the range: [0, 1]
            // equilateral triangles score 1
            // sliver triangles score 0
            double dab = (a - b).length();
            double dbc = (b - c).length();
            double dca = (c - a).length();

            if (dab < 1e-10 || dbc < 1e-10 || dca < 1e-10) return 0.0;

            return std::max(std::min((dab + dbc) / dca, std::min((dab + dca) / dbc, (dbc + dca) / dab)) - 1.0, 0.0);
          }

          // calculate a score for the ear edge.
          double calcScore() const {
            double this_tri = triScore(prev->p, p, next->p);
            double next_tri = triScore(prev->p, next->p, next->next->p);
            double prev_tri = triScore(prev->prev->p, prev->p, next->p);

            return this_tri + std::max(next_tri, prev_tri) * .2;
          }

          void recompute() {
            convex = testForConvexVertex();
            failed = false;
            if (convex) {
              score = calcScore();
            } else {
              score = -1e-5;
            }
          }

          static bool inTriangle(const VertexInfo *a,
                                 const VertexInfo *b,
                                 const VertexInfo *c,
                                 const geom2d::P2 &e) {
            return !isLeft(b, a, e) && !isLeft(c, b, e) && !isLeft(a, c, e);
          }


          bool isClipable() const {
            for (const VertexInfo *v_test = next->next; v_test != prev; v_test = v_test->next) {
              if (v_test->convex) {
                continue;
              }

              if (v_test->p == prev->p || v_test->p == next->p) {
                continue;
              }

              if (v_test->p == p) {
                if (v_test->next->p == prev->p && v_test->prev->p == next->p) {
                  return false;
                }
              
                if (v_test->next->p == prev->p || v_test->prev->p == next->p) {
                  continue;
                }
              }

              if (inTriangle(prev, this, next, v_test->p)) {
                return false;
              }
            }
            return true;
          }
        };

        struct order_by_score {
          bool operator()(const VertexInfo *a, const VertexInfo *b) const {
            return a->score < b->score;
          }
        };

        typedef std::pair<VertexInfo *, VertexInfo *> diag_t;

        proj_t proj;

        geom2d::P2 P(const VertexInfo *vi) const {
          return vi->p;
        }

        geom2d::P2 P(const edge_t *edge) const {
          return proj(edge->vert->v);
        }

        bool isLeft(const edge_t *a, const edge_t *b, const geom2d::P2 &p) const {
          if (a < b) {
            return carve::geom2d::orient2d(P(a), P(b), p) > 0.0;
          } else {
            return carve::geom2d::orient2d(P(b), P(a), p) < 0.0;
          }
        }

        bool testForConvexVertex(const edge_t *vert) const {
          return isLeft(vert->next, vert->prev, P(vert));
        }

        bool inCone(const VertexInfo *vert, const geom2d::P2 &p) const {
          return geom2d::internalToAngle(P(vert->next), P(vert), P(vert->prev), p);
        }

        int windingNumber(VertexInfo *vert, const carve::geom2d::P2 &point) const {
          int wn = 0;

          VertexInfo *v = vert;
          geom2d::P2 v_p = P(vert);
          do {
            geom2d::P2 n_p = P(v->next);
            
            if (v_p.y <= point.y) {
              if (n_p.y > point.y && carve::geom2d::orient2d(v_p, n_p, point) > 0.0) {
                ++wn;
              }
            } else {
              if (n_p.y <= point.y && carve::geom2d::orient2d(v_p, n_p, point) < 0.0) {
                --wn;
              }
            }
            v = v->next;
            v_p = n_p;
          } while (v != vert);

          return wn;
        }

        bool diagonalIsCandidate(diag_t diag) const {
          VertexInfo *v1 = diag.first;
          VertexInfo *v2 = diag.second;
          return (inCone(v1, P(v2)) && inCone(v2, P(v1)));
        }

        bool testDiagonal(diag_t diag) const {
          // test whether v1-v2 is a valid diagonal.
          VertexInfo *v1 = diag.first;
          VertexInfo *v2 = diag.second;
          geom2d::P2 v1p = P(v1);
          geom2d::P2 v2p = P(v2);

          bool intersected = false;

          for (VertexInfo *t = v1->next; !intersected && t != v1->prev; t = t->next) {
            VertexInfo *u = t->next;
            if (t == v2 || u == v2) continue;

            geom2d::P2 tp = P(t);
            geom2d::P2 up = P(u);

            double l_a1 = carve::geom2d::orient2d(v1p, v2p, tp);
            double l_a2 = carve::geom2d::orient2d(v1p, v2p, up);

            double l_b1 = carve::geom2d::orient2d(tp, up, v1p);
            double l_b2 = carve::geom2d::orient2d(tp, up, v2p);

            if (l_a1 > l_a2) std::swap(l_a1, l_a2);
            if (l_b1 > l_b2) std::swap(l_b1, l_b2);

            if (l_a1 == 0.0 && l_a2 == 0.0 &&
                l_b1 == 0.0 && l_b2 == 0.0) {
              // colinear
              if (std::max(tp.x, up.x) >= std::min(v1p.x, v2p.x) && std::min(tp.x, up.x) <= std::max(v1p.x, v2p.x)) {
                // colinear and intersecting
                intersected = true;
              }
              continue;
            }

            if (l_a2 <= 0.0 || l_a1 >= 0.0  || l_b2 <= 0.0 || l_b1 >= 0.0) {
              // no intersection
              continue;
            }

            intersected = true;
          }

          if (!intersected) {
            // test whether midpoint winding == 1

            carve::geom2d::P2 mid = (v1p + v2p) / 2;
            if (windingNumber(v1, mid) == 1) {
              // this diagonal is ok
              return true;
            }
          }
          return false;
        }

        // Find the vertex half way around the loop (rounds upwards).
        VertexInfo *findMidpoint(VertexInfo *vert) const {
          VertexInfo *v = vert;
          VertexInfo *r = vert;
          while (1) {
            r = r->next;
            v = v->next; if (v == vert) return r;
            v = v->next; if (v == vert) return r;
          }
        }

        // Test all diagonals with a separation of a-b by walking both
        // pointers around the loop. In the case where a-b divides the
        // loop exactly in half, this will test each diagonal twice,
        // but avoiding this case is not worth the extra effort
        // required.
        diag_t scanDiagonals(VertexInfo *a, VertexInfo *b) const {
          VertexInfo *v1 = a;
          VertexInfo *v2 = b;

          do {
            diag_t d(v1, v2);
            if (diagonalIsCandidate(d) && testDiagonal(d)) {
              return d;
            }
            v1 = v1->next;
            v2 = v2->next;
          } while (v1 != a);

          return diag_t(NULL, NULL);
        }

        diag_t scanAllDiagonals(VertexInfo *a) const {
          // Rationale: We want to find a diagonal that splits the
          // loop into two as evenly as possible, to reduce the number
          // of times that diagonal splitting is required. Start by
          // scanning all diagonals separated by loop_len / 2, then
          // decrease the separation until we find something.

          // loops of length 2 or 3 have no possible diagonal.
          if (a->next == a || a->next->next == a) return diag_t(NULL, NULL);

          VertexInfo *b = findMidpoint(a);
          while (b != a->next) {
            diag_t d = scanDiagonals(a, b);
            if (d != diag_t(NULL, NULL)) return d;
            b = b->prev;
          }

          return diag_t(NULL, NULL);
        }

        diag_t findDiagonal(VertexInfo *vert) const {
          return scanAllDiagonals(vert);
        }

        diag_t findHighScoringDiagonal(VertexInfo *vert) const {
          typedef std::pair<double, diag_t> heap_entry_t;
          VertexInfo *v1, *v2;
          std::vector<heap_entry_t> heap;
          size_t loop_len = 0;

          v1 = vert;
          do {
            ++loop_len;
            v1 = v1->next;
          } while (v1 != vert);

          v1 = vert;
          do {
            v2 = v1->next->next;
            size_t dist = 2;
            do {
              if (diagonalIsCandidate(diag_t(v1, v2))) {
                double score = std::min(dist, loop_len - dist);
                // double score = (v1->edge->vert->v - v2->edge->vert->v).length2();
                heap.push_back(heap_entry_t(score, diag_t(v1, v2)));
              }
              v2 = v2->next;
              ++dist;
            } while (v2 != vert && v2 != v1->prev);
            v1 = v1->next;
          } while (v1->next->next != vert);

          std::make_heap(heap.begin(), heap.end());

          while (heap.size()) {
            std::pop_heap(heap.begin(), heap.end());
            heap_entry_t h = heap.back();
            heap.pop_back();

            if (testDiagonal(h.second)) return h.second;
          }

          // couldn't find a diagonal that was ok.
          return diag_t(NULL, NULL);
        }

        void splitEdgeLoop(VertexInfo *v1, VertexInfo *v2) {
          VertexInfo *v1_copy = new VertexInfo(new Edge<ndim>(v1->edge->vert, NULL), v1->p);
          VertexInfo *v2_copy = new VertexInfo(new Edge<ndim>(v2->edge->vert, NULL), v2->p);

          v1_copy->edge->rev = v2_copy->edge;
          v2_copy->edge->rev = v1_copy->edge;

          v1_copy->edge->prev = v1->edge->prev;
          v1_copy->edge->next = v2->edge;

          v2_copy->edge->prev = v2->edge->prev;
          v2_copy->edge->next = v1->edge;

          v1->edge->prev->next = v1_copy->edge;
          v1->edge->prev = v2_copy->edge;

          v2->edge->prev->next = v2_copy->edge;
          v2->edge->prev = v1_copy->edge;

          v1_copy->prev = v1->prev;
          v1_copy->next = v2;

          v2_copy->prev = v2->prev;
          v2_copy->next = v1;

          v1->prev->next = v1_copy;
          v1->prev = v2_copy;

          v2->prev->next = v2_copy;
          v2->prev = v1_copy;
        }

        VertexInfo *findDegenerateEar(VertexInfo *edge) {
          VertexInfo *v = edge;

          if (v->next == v || v->next->next == v) return NULL;

          do {
            if (P(v) == P(v->next)) {
              return v;
            } else if (P(v) == P(v->next->next)) {
              if (P(v->next) == P(v->next->next->next)) {
                // a 'z' in the loop: z (a) b a b c -> remove a-b-a -> z (a) a b c -> remove a-a-b (next loop) -> z a b c
                // z --(a)-- b
                //         /
                //        /
                //      a -- b -- d
                return v->next;
              } else {
                // a 'shard' in the loop: z (a) b a c d -> remove a-b-a -> z (a) a b c d -> remove a-a-b (next loop) -> z a b c d
                // z --(a)-- b
                //         /
                //        /
                //      a -- c -- d
                // n.b. can only do this if the shard is pointing out of the polygon. i.e. b is outside z-a-c
                if (!carve::geom2d::internalToAngle(P(v->next->next->next), P(v), P(v->prev), P(v->next))) {
                  return v->next;
                }
              }
            }
            v = v->next;
          } while (v != edge);

          return NULL;
        }

        // Clip off a vertex at vert, producing a triangle (with appropriate rev pointers)
        template<typename out_iter_t>
        VertexInfo *clipEar(VertexInfo *vert, out_iter_t out) {
          CARVE_ASSERT(testForConvexVertex(vert->edge));

          edge_t *p_edge = vert->edge->prev;
          edge_t *n_edge = vert->edge->next;

          edge_t *p_copy = new edge_t(p_edge->vert, NULL);
          edge_t *n_copy = new edge_t(n_edge->vert, NULL);

          n_copy->next = p_copy;
          n_copy->prev = vert->edge;

          p_copy->next = vert->edge;
          p_copy->prev = n_copy;

          vert->edge->next = n_copy;
          vert->edge->prev = p_copy;

          p_edge->next = n_edge;
          n_edge->prev = p_edge;

          if (p_edge->rev) {
            p_edge->rev->rev = p_copy;
          }
          p_copy->rev = p_edge->rev;

          p_edge->rev = n_copy;
          n_copy->rev = p_edge;

          *out++ = vert->edge;

          if (vert->edge->face) {
            if (vert->edge->face->edge == vert->edge) {
              vert->edge->face->edge = n_edge;
            }
            vert->edge->face->n_edges--;
            vert->edge->face = NULL;
          }

          vert->next->prev = vert->prev;
          vert->prev->next = vert->next;

          VertexInfo *n = vert->next;
          delete vert;
          return n;
        }

        template<typename out_iter_t>
        size_t removeDegeneracies(VertexInfo *&begin, out_iter_t out) {
          VertexInfo *v;
          size_t count = 0;

          while ((v = findDegenerateEar(begin)) != NULL) {
            begin = clipEar(v, out);
            ++count;
          }
          return count;
        }

        template<typename out_iter_t>
        bool splitAndResume(VertexInfo *begin, out_iter_t out) {
          diag_t diag;

          diag = findDiagonal(begin);
          if (diag == diag_t(NULL, NULL)) {
            std::cerr << "failed to find diagonal" << std::endl;
            return false;
          }

          // add a splitting edge between v1 and v2.
          VertexInfo *v1 = diag.first;
          VertexInfo *v2 = diag.second;

          splitEdgeLoop(v1, v2);

          v1->recompute();
          v1->next->recompute();

          v2->recompute();
          v2->next->recompute();

#if defined(CARVE_DEBUG)
          dumpPoly(v1->edge, v2->edge);
#endif

#if defined(CARVE_DEBUG)
          CARVE_ASSERT(!checkSelfIntersection(v1));
          CARVE_ASSERT(!checkSelfIntersection(v2));
#endif

          bool r1 = doTriangulate(v1, out);
          bool r2 =  doTriangulate(v2, out);

          return r1 && r2;
        }
        
        template<typename out_iter_t>
        bool doTriangulate(VertexInfo *begin, out_iter_t out);

        TriangulationData(proj_t _proj) : proj(_proj) {
        }

        VertexInfo *init(edge_t *begin) {
          edge_t *e = begin;
          VertexInfo *head = NULL, *tail = NULL, *v;
          do {
            VertexInfo *v = new VertexInfo(e, proj(e->vert->v));
            if (tail != NULL) {
              tail->next = v;
              v->prev = tail;
            } else {
              head = v;
            }
            tail = v;

            e = e->next;
          } while (e != begin);
          tail->next = head;
          head->prev = tail;

          v = head;
          do {
            v->recompute();
            v = v->next;
          } while (v != head);
          return head;
        }

        class EarQueue {
          TriangulationData &data;
          std::vector<VertexInfo *> queue;

          void checkheap() {
#if defined(HAVE_IS_HEAP)
            CARVE_ASSERT(std::__is_heap(queue.begin(), queue.end(), order_by_score()));
#endif
          }

        public:
          EarQueue(TriangulationData &_data) : data(_data), queue() {
          }

          size_t size() const {
            return queue.size();
          }

          void push(VertexInfo *v) {
#if defined(CARVE_DEBUG)
            checkheap();
#endif
            queue.push_back(v);
            std::push_heap(queue.begin(), queue.end(), order_by_score());
          }

          VertexInfo *pop() {
#if defined(CARVE_DEBUG)
            checkheap();
#endif
            std::pop_heap(queue.begin(), queue.end(), order_by_score());
            VertexInfo *v = queue.back();
            queue.pop_back();
            return v;
          }

          void remove(VertexInfo *v) {
#if defined(CARVE_DEBUG)
            checkheap();
#endif
            CARVE_ASSERT(std::find(queue.begin(), queue.end(), v) != queue.end());
            double score = v->score;
            if (v != queue[0]) {
              v->score = queue[0]->score + 1;
              std::make_heap(queue.begin(), queue.end(), order_by_score());
            }
            CARVE_ASSERT(v == queue[0]);
            std::pop_heap(queue.begin(), queue.end(), order_by_score());
            CARVE_ASSERT(queue.back() == v);
            queue.pop_back();
            v->score = score;
          }

          void changeScore(VertexInfo *v, double s_from, double s_to) {
#if defined(CARVE_DEBUG)
            checkheap();
#endif
            CARVE_ASSERT(std::find(queue.begin(), queue.end(), v) != queue.end());
            if (s_from != s_to) {
              v->score = s_to;
              std::make_heap(queue.begin(), queue.end(), order_by_score());
            }
          }

          void update(VertexInfo *v) {
            VertexInfo pre = *v;
            v->recompute();
            VertexInfo post = *v;

            if (pre.isCandidate()) {
              if (post.isCandidate()) {
                changeScore(v, pre.score, post.score);
              } else {
                remove(v);
              }
            } else {
              if (post.isCandidate()) {
                push(v);
              }
            }
          }
        };


        bool checkSelfIntersection(const VertexInfo *vert) {
          const VertexInfo *v1 = vert;
          do {
            const VertexInfo *v2 = vert->next->next;
            do {
              carve::geom2d::P2 a = v1->p;
              carve::geom2d::P2 b = v1->next->p;
              CARVE_ASSERT(a == proj(v1->edge->vert->v));
              CARVE_ASSERT(b == proj(v1->edge->next->vert->v));

              carve::geom2d::P2 c = v2->p;
              carve::geom2d::P2 d = v2->next->p;
              CARVE_ASSERT(c == proj(v2->edge->vert->v));
              CARVE_ASSERT(d == proj(v2->edge->next->vert->v));

              bool intersected = false;
              if (a == c || a == d || b == c || b == d) {
              } else {
                intersected = true;

                double l_a1 = carve::geom2d::orient2d(a, b, c);
                double l_a2 = carve::geom2d::orient2d(a, b, d);
                if (l_a1 > l_a2) std::swap(l_a1, l_a2);
                if (l_a2 <= 0.0 || l_a1 >= 0.0) {
                  intersected = false;
                }

                double l_b1 = carve::geom2d::orient2d(c, d, a);
                double l_b2 = carve::geom2d::orient2d(c, d, b);
                if (l_b1 > l_b2) std::swap(l_b1, l_b2);
                if (l_b2 <= 0.0 || l_b1 >= 0.0) {
                  intersected = false;
                }

                if (l_a1 == 0.0 && l_a2 == 0.0 && l_b1 == 0.0 && l_b2 == 0.0) {
                  if (std::max(a.x, b.x) >= std::min(c.x, d.x) && std::min(a.x, b.x) <= std::max(c.x, d.x)) {
                    // colinear and intersecting.
                  } else {
                    // colinear but not intersecting.
                    intersected = false;
                  }
                }
              }
              if (intersected) {
                carve::geom2d::P2 p[4] = { a, b, c, d };
                carve::geom::aabb<2> A(p, p+4);
                A.expand(5);

                std::cerr << "\
<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\
<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n\
  x=\"" << A.min().x << "px\" y=\"" << A.min().y << "\"\n\
  width=\"" << A.extent.x * 2 << "\" height=\"" << A.extent.y * 2 << "\"\n\
  viewBox=\"" << A.min().x << " " << A.min().y << " " << A.max().x << " " << A.max().y << "\"\n\
  enable-background=\"new " << A.min().x << " " << A.min().y << " " << A.max().x << " " << A.max().y << "\"\n\
  xml:space=\"preserve\">\n\
<line fill=\"none\" stroke=\"#000000\" x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>\n\
<line fill=\"none\" stroke=\"#000000\" x1=\"" << c.x << "\" y1=\"" << c.y << "\" x2=\"" << d.x << "\" y2=\"" << d.y << "\"/>\n\
</svg>\n";
                return true;
              }
              v2 = v2->next;
            } while (v2 != vert);
            v1 = v1->next;
          } while (v1 != vert);
          return false;
        }

        carve::geom::aabb<2> make2d(const edge_t *edge, std::vector<geom2d::P2> &points) {
          const edge_t *e = edge;
          do {
            points.push_back(P(e));
            e = e->next;
          } while(e != edge);
          return carve::geom::aabb<2>(points.begin(), points.end());
        }

        void dumpLoop(std::ostream &out,
                      const std::vector<carve::geom2d::P2> &points,
                      const char *fill,
                      const char *stroke,
                      double stroke_width,
                      double offx,
                      double offy,
                      double scale
                      ) {
          out << "<polygon fill=\"" << fill << "\" stroke=\"" << stroke << "\" stroke-width=\"" << stroke_width << "\" points=\"";
          for (size_t i = 0; i < points.size(); ++i) {
            if (i) out << ' ';
            double x, y;
            x = scale * (points[i].x - offx) + 5;
            y = scale * (points[i].y - offy) + 5;
            out << x << ',' << y;
          }
          out << "\" />" << std::endl;
        }

        void dumpPoly(const edge_t *edge, const edge_t *edge2 = NULL, const char *pfx = "poly_") {
          static int step = 0;
          std::ostringstream filename;
          filename << pfx << step++ << ".svg";
          std::cerr << "dumping to " << filename.str() << std::endl;
          std::ofstream out(filename.str().c_str());

          std::vector <geom2d::P2> points, points2;

          carve::geom::aabb<2> A = make2d(edge, points);
          if (edge2) {
            A.unionAABB(make2d(edge2, points2));
          }
          A.expand(5);

          out << "\
<?xml version=\"1.0\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\
<svg\n\
  x=\"" << A.min().x << "px\" y=\"" << A.min().y << "\"\n\
  width=\"" << A.extent.x * 2 << "\" height=\"" << A.extent.y * 2 << "\"\n\
  viewBox=\"" << A.min().x << " " << A.min().y << " " << A.max().x << " " << A.max().y << "\"\n\
  enable-background=\"new " << A.min().x << " " << A.min().y << " " << A.max().x << " " << A.max().y << "\"\n\
  xml:space=\"preserve\">\n";

          dumpLoop(out, points, "rgb(0,0,0)", "blue", 0.1, 0, 0, 1);
          if (points2.size()) dumpLoop(out, points2, "rgb(255,0,0)", "blue", 0.1, 0, 0, 1);

          out << "</svg>" << std::endl;
        }
      };

      template<unsigned ndim, typename proj_t>
      template<typename out_iter_t>
      bool TriangulationData<ndim, proj_t>::doTriangulate(VertexInfo *begin, out_iter_t out) {
        EarQueue vq(*this);

#if defined(CARVE_DEBUG)
        dumpPoly(begin->edge, NULL, "input_");
        CARVE_ASSERT(!checkSelfIntersection(begin));
#endif

        VertexInfo *v = begin, *n, *p;
        size_t remain = 0;
        do {
          if (v->isCandidate()) vq.push(v);
          v = v->next;
          remain++;
        } while (v != begin);

        while (remain > 3 && vq.size()) {
          { static int __c = 0; if (++__c % 50 == 0) { break; } }
          v = vq.pop();
          if (!v->isClipable()) {
            v->fail();
            continue;
          }

        continue_clipping:
          n = clipEar(v, out);
          p = n->prev;
          begin = n;
          if (--remain == 3) break;
          // if (checkSelfIntersection(begin)) {
          //   dumpPoly(begin->edge, NULL, "badclip_");
          //   CARVE_ASSERT(!!!"clip created self intersection");
          // }

          vq.update(n);
          vq.update(p);

          if (n->score < p->score) { std::swap(n, p); }
          if (n->score > 0.25 && n->isCandidate() && n->isClipable()) {
            vq.remove(n);
            v = n;
            goto continue_clipping;
          }
          if (p->score > 0.25 && p->isCandidate() && p->isClipable()) {
            vq.remove(p);
            v = p;
            goto continue_clipping;
          }
        }

        bool ret = false;

#if defined(CARVE_DEBUG)
        dumpPoly(begin->edge, NULL, "remainder_");
#endif

        if (remain > 3) {
          std::vector<carve::geom2d::P2> temp;
          temp.reserve(remain);
          VertexInfo *v = begin;
          do {
            temp.push_back(P(v));
            v = v->next;
          } while (v != begin);

          if (carve::geom2d::signedArea(temp) == 0) {
            // XXX: this test will fail in cases where the boundary is
            // twisted so that a negative area balances a positive area.
            std::cerr << "got to here" << std::endl;
            dumpPoly(begin->edge, NULL, "interesting_case_");
            goto done;
          }
        }

        if (remain > 3) {
          remain -= removeDegeneracies(begin, out);
        }

        if (remain > 3) {
          return splitAndResume(begin, out);
        }

        { double a = loopArea(begin->edge, proj); CARVE_ASSERT(a <= 0.0); }
        *out++ = begin->edge;

        v = begin;
        do {
          n = v->next;
          delete v;
          v = n;
        } while (v != begin);

        ret = true;

      done:
        return ret;
      }
    }



    template<unsigned ndim, typename proj_t, typename out_iter_t>
    void triangulate(Edge<ndim> *edge, proj_t proj, out_iter_t out) {
      detail::TriangulationData<ndim, proj_t> triangulator(proj);
      typename detail::TriangulationData<ndim, proj_t>::VertexInfo *v = triangulator.init(edge);
      triangulator.removeDegeneracies(v, out);
      triangulator.doTriangulate(v, out);
    }

    // given edge a-b, part of triangles a-b-c and b-a-d, make triangles c-a-d and b-c-d
    template<unsigned ndim>
    void flipTriEdge(Edge<ndim> *edge) {
      CARVE_ASSERT(edge->rev != NULL);
      CARVE_ASSERT(edge->face->nEdges() == 3);
      CARVE_ASSERT(edge->rev->face->nEdges() == 3);

      CARVE_ASSERT(edge->prev != edge);
      CARVE_ASSERT(edge->next != edge);
      CARVE_ASSERT(edge->rev->prev != edge->rev);
      CARVE_ASSERT(edge->rev->next != edge->rev);

      typedef Edge<ndim> edge_t;
      typedef Face<ndim> face_t;

      edge_t *t1[3], *t2[3];
      face_t *f1, *f2;

      t1[1] = edge; t2[1] = edge->rev;
      t1[0] = t1[1]->prev; t1[2] = t1[1]->next;
      t2[0] = t2[1]->prev; t2[2] = t2[1]->next;

      f1 = t1[1]->face; f2 = t2[1]->face;

      // std::cerr << t1[0]->vert << "->" << t1[1]->vert << "->" << t1[2]->vert << std::endl;
      // std::cerr << t2[0]->vert << "->" << t2[1]->vert << "->" << t2[2]->vert << std::endl;

      t1[1]->vert = t2[0]->vert;
      t2[1]->vert = t1[0]->vert;

      // std::cerr << t1[0]->vert << "->" << t2[2]->vert << "->" << t1[1]->vert << std::endl;
      // std::cerr << t2[0]->vert << "->" << t1[2]->vert << "->" << t2[1]->vert << std::endl;

      detail::link(t1[0], t2[2], t1[1], f1);
      detail::link(t2[0], t1[2], t2[1], f2);

      if (t1[0]->rev) CARVE_ASSERT(t1[0]->v2() == t1[0]->rev->v1());
      if (t2[0]->rev) CARVE_ASSERT(t2[0]->v2() == t2[0]->rev->v1());
      if (t1[2]->rev) CARVE_ASSERT(t1[2]->v2() == t1[2]->rev->v1());
      if (t2[2]->rev) CARVE_ASSERT(t2[2]->v2() == t2[2]->rev->v1());
    }

    template<unsigned ndim>
    void splitEdgeLoop(Edge<ndim> *v1, Edge<ndim> *v2) {
      // v1 and v2 end up on different sides of the split.
      Edge<ndim> *v1_copy = new Edge<ndim>(v1->vert, NULL);
      Edge<ndim> *v2_copy = new Edge<ndim>(v2->vert, NULL);

      v1_copy->rev = v2_copy;
      v2_copy->rev = v1_copy;

      v1_copy->prev = v1->prev;
      v1_copy->next = v2;

      v2_copy->prev = v2->prev;
      v2_copy->next = v1;

      v1->prev->next = v1_copy;
      v1->prev = v2_copy;

      v2->prev->next = v2_copy;
      v2->prev = v1_copy;
    }

    template<unsigned ndim>
    Edge<ndim> *clipVertex(Edge<ndim> *edge) {
      Edge<ndim> *prev = edge->prev;
      Edge<ndim> *next = edge->next;
      splitEdgeLoop(edge->prev, edge->next);
      return next;
    }
  }
}
