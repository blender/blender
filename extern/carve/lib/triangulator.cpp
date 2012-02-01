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


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/csg.hpp>
#include <carve/triangulator.hpp>

#include <fstream>
#include <sstream>

#include <algorithm>


namespace {
  // private code related to hole patching.

  class order_h_loops_2d {
    order_h_loops_2d &operator=(const order_h_loops_2d &);

    const std::vector<std::vector<carve::geom2d::P2> > &poly;
    int axis;
    public:

    order_h_loops_2d(const std::vector<std::vector<carve::geom2d::P2> > &_poly, int _axis) :
      poly(_poly), axis(_axis) {
      }

    bool operator()(const std::pair<size_t, size_t> &a,
        const std::pair<size_t, size_t> &b) const {
      return carve::triangulate::detail::axisOrdering(poly[a.first][a.second], poly[b.first][b.second], axis);
    }
  };

  class heap_ordering_2d {
    heap_ordering_2d &operator=(const heap_ordering_2d &);

    const std::vector<std::vector<carve::geom2d::P2> > &poly;
    const std::vector<std::pair<size_t, size_t> > &loop;
    const carve::geom2d::P2 p;
    int axis;

    public:

    heap_ordering_2d(const std::vector<std::vector<carve::geom2d::P2> > &_poly,
        const std::vector<std::pair<size_t, size_t> > &_loop,
        const carve::geom2d::P2 _p,
        int _axis) : poly(_poly), loop(_loop), p(_p), axis(_axis) {
    }

    bool operator()(size_t a, size_t b) const {
      double da = carve::geom::distance2(p, poly[loop[a].first][loop[a].second]);
      double db = carve::geom::distance2(p, poly[loop[b].first][loop[b].second]);
      if (da > db) return true;
      if (da < db) return false;
      return carve::triangulate::detail::axisOrdering(poly[loop[a].first][loop[a].second], poly[loop[b].first][loop[b].second], axis);
    }
  };

  static inline void patchHoleIntoPolygon_2d(std::vector<std::pair<size_t, size_t> > &f_loop,
      size_t f_loop_attach,
      size_t h_loop,
      size_t h_loop_attach,
      size_t h_loop_size) {
    f_loop.insert(f_loop.begin() + f_loop_attach + 1, h_loop_size + 2, std::make_pair(h_loop, 0));
    size_t f = f_loop_attach + 1;

    for (size_t h = h_loop_attach; h != h_loop_size; ++h) {
      f_loop[f++].second = h;
    }

    for (size_t h = 0; h <= h_loop_attach; ++h) {
      f_loop[f++].second = h;
    }

    f_loop[f] = f_loop[f_loop_attach];
  }

  static inline const carve::geom2d::P2 &pvert(const std::vector<std::vector<carve::geom2d::P2> > &poly, const std::pair<size_t, size_t> &idx) {
    return poly[idx.first][idx.second];
  }
}


namespace {
  // private code related to triangulation.

  using carve::triangulate::detail::vertex_info;

  struct vertex_info_ordering {
    bool operator()(const vertex_info *a, const vertex_info *b) const {
      return a->score < b->score;
    }
  };

  struct vertex_info_l2norm_inc_ordering {
    const vertex_info *v;
    vertex_info_l2norm_inc_ordering(const vertex_info *_v) : v(_v) {
    }
    bool operator()(const vertex_info *a, const vertex_info *b) const {
      return carve::geom::distance2(v->p, a->p) > carve::geom::distance2(v->p, b->p);
    }
  };

  class EarQueue {
    std::vector<vertex_info *> queue;

    void checkheap() {
#ifdef __GNUC__
      CARVE_ASSERT(std::__is_heap(queue.begin(), queue.end(), vertex_info_ordering()));
#endif
    }

  public:
    EarQueue() {
    }

    size_t size() const {
      return queue.size();
    }

    void push(vertex_info *v) {
#if defined(CARVE_DEBUG)
      checkheap();
#endif
      queue.push_back(v);
      std::push_heap(queue.begin(), queue.end(), vertex_info_ordering());
    }

    vertex_info *pop() {
#if defined(CARVE_DEBUG)
      checkheap();
#endif
      std::pop_heap(queue.begin(), queue.end(), vertex_info_ordering());
      vertex_info *v = queue.back();
      queue.pop_back();
      return v;
    }

    void remove(vertex_info *v) {
#if defined(CARVE_DEBUG)
      checkheap();
#endif
      CARVE_ASSERT(std::find(queue.begin(), queue.end(), v) != queue.end());
      double score = v->score;
      if (v != queue[0]) {
        v->score = queue[0]->score + 1;
        std::make_heap(queue.begin(), queue.end(), vertex_info_ordering());
      }
      CARVE_ASSERT(v == queue[0]);
      std::pop_heap(queue.begin(), queue.end(), vertex_info_ordering());
      CARVE_ASSERT(queue.back() == v);
      queue.pop_back();
      v->score = score;
    }

    void changeScore(vertex_info *v, double score) {
#if defined(CARVE_DEBUG)
      checkheap();
#endif
      CARVE_ASSERT(std::find(queue.begin(), queue.end(), v) != queue.end());
      if (v->score != score) {
        v->score = score;
        std::make_heap(queue.begin(), queue.end(), vertex_info_ordering());
      }
    }

    // 39% of execution time
    void updateVertex(vertex_info *v) {
      double spre = v->score;
      bool qpre = v->isCandidate();
      v->recompute();
      bool qpost = v->isCandidate();
      double spost = v->score;

      v->score = spre;

      if (qpre) {
        if (qpost) {
          if (v->score != spre) {
            changeScore(v, spost);
          }
        } else {
          remove(v);
        }
      } else {
        if (qpost) {
          push(v);
        }
      }
    }
  };



  int windingNumber(vertex_info *begin, const carve::geom2d::P2 &point) {
    int wn = 0;

    vertex_info *v = begin;
    do {
      if (v->p.y <= point.y) {
        if (v->next->p.y > point.y && carve::geom2d::orient2d(v->p, v->next->p, point) > 0.0) {
          ++wn;
        }
      } else {
        if (v->next->p.y <= point.y && carve::geom2d::orient2d(v->p, v->next->p, point) < 0.0) {
          --wn;
        }
      }
      v = v->next;
    } while (v != begin);

    return wn;
  }



  bool internalToAngle(const vertex_info *a,
                       const vertex_info *b,
                       const vertex_info *c,
                       const carve::geom2d::P2 &p) {
    return carve::geom2d::internalToAngle(a->p, b->p, c->p, p);
  }



  bool findDiagonal(vertex_info *begin, vertex_info *&v1, vertex_info *&v2) {
    vertex_info *t;
    std::vector<vertex_info *> heap;

    v1 = begin;
    do {
      heap.clear();

      for (v2 = v1->next->next; v2 != v1->prev; v2 = v2->next) {
        if (!internalToAngle(v1->next, v1, v1->prev, v2->p) ||
            !internalToAngle(v2->next, v2, v2->prev, v1->p)) continue;

        heap.push_back(v2);
        std::push_heap(heap.begin(), heap.end(), vertex_info_l2norm_inc_ordering(v1));
      }

      while (heap.size()) {
        std::pop_heap(heap.begin(), heap.end(), vertex_info_l2norm_inc_ordering(v1));
        v2 = heap.back(); heap.pop_back();

#if defined(CARVE_DEBUG)
        std::cerr << "testing: " << v1 << " - " << v2 << std::endl;
        std::cerr << "  length = " << (v2->p - v1->p).length() << std::endl;
        std::cerr << "  pos: " << v1->p << " - " << v2->p << std::endl;
#endif
        // test whether v1-v2 is a valid diagonal.
        double v_min_x = std::min(v1->p.x, v2->p.x);
        double v_max_x = std::max(v1->p.x, v2->p.x);

        bool intersected = false;

        for (t = v1->next; !intersected && t != v1->prev; t = t->next) {
          vertex_info *u = t->next;
          if (t == v2 || u == v2) continue;

          double l1 = carve::geom2d::orient2d(v1->p, v2->p, t->p);
          double l2 = carve::geom2d::orient2d(v1->p, v2->p, u->p);

          if ((l1 > 0.0 && l2 > 0.0) || (l1 < 0.0 && l2 < 0.0)) {
            // both on the same side; no intersection
            continue;
          }

          double dx13 = v1->p.x - t->p.x;
          double dy13 = v1->p.y - t->p.y;
          double dx43 = u->p.x - t->p.x;
          double dy43 = u->p.y - t->p.y;
          double dx21 = v2->p.x - v1->p.x;
          double dy21 = v2->p.y - v1->p.y;
          double ua_n = dx43 * dy13 - dy43 * dx13;
          double ub_n = dx21 * dy13 - dy21 * dx13;
          double u_d  = dy43 * dx21 - dx43 * dy21;

          if (carve::math::ZERO(u_d)) {
            // parallel
            if (carve::math::ZERO(ua_n)) {
              // colinear
              if (std::max(t->p.x, u->p.x) >= v_min_x && std::min(t->p.x, u->p.x) <= v_max_x) {
                // colinear and intersecting
                intersected = true;
              }
            }
          } else {
            // not parallel
            double ua = ua_n / u_d;
            double ub = ub_n / u_d;

            if (0.0 <= ua && ua <= 1.0 && 0.0 <= ub && ub <= 1.0) {
              intersected = true;
            }
          }
#if defined(CARVE_DEBUG)
          if (intersected) {
            std::cerr << "  failed on edge: " << t << " - " << u << std::endl;
            std::cerr << "    pos: " << t->p << " - " << u->p << std::endl;
          }
#endif
        }

        if (!intersected) {
          // test whether midpoint winding == 1

          carve::geom2d::P2 mid = (v1->p + v2->p) / 2;
          if (windingNumber(begin, mid) == 1) {
            // this diagonal is ok
            return true;
          }
        }
      }

      // couldn't find a diagonal from v1 that was ok.
      v1 = v1->next;
    } while (v1 != begin);
    return false;
  }



#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  void dumpPoly(const std::vector<carve::geom2d::P2> &points,
      const std::vector<carve::triangulate::tri_idx> &result) {
    static int step = 0;
    std::ostringstream filename;
    filename << "poly_" << step++ << ".svg";
    std::cerr << "dumping to " << filename.str() << std::endl;
    std::ofstream out(filename.str().c_str());

    double minx = points[0].x, maxx = points[0].x;
    double miny = points[0].y, maxy = points[0].y;

    for (size_t i = 1; i < points.size(); ++i) {
      minx = std::min(points[i].x, minx); maxx = std::max(points[i].x, maxx);
      miny = std::min(points[i].y, miny); maxy = std::max(points[i].y, maxy);
    }
    double scale = 100 / std::max(maxx-minx, maxy-miny);

    maxx *= scale; minx *= scale;
    maxy *= scale; miny *= scale;

    double width = maxx - minx + 10;
    double height = maxy - miny + 10;

    out << "\
<?xml version=\"1.0\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\
<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" << width << "\" height=\"" << height << "\">\n";

    out << "<polygon fill=\"rgb(0,0,0)\" stroke=\"blue\" stroke-width=\"0.1\" points=\"";
    for (size_t i = 0; i < points.size(); ++i) {
      if (i) out << ' ';
      double x, y;
      x = scale * (points[i].x) - minx + 5;
      y = scale * (points[i].y) - miny + 5;
      out << x << ',' << y;
    }
    out << "\" />" << std::endl;

    for (size_t i = 0; i < result.size(); ++i) {
      out << "<polygon fill=\"rgb(255,255,255)\" stroke=\"black\" stroke-width=\"0.1\" points=\"";
      double x, y;
      x = scale * (points[result[i].a].x) - minx + 5;
      y = scale * (points[result[i].a].y) - miny + 5;
      out << x << ',' << y << ' ';        
      x = scale * (points[result[i].b].x) - minx + 5;
      y = scale * (points[result[i].b].y) - miny + 5;
      out << x << ',' << y << ' ';        
      x = scale * (points[result[i].c].x) - minx + 5;
      y = scale * (points[result[i].c].y) - miny + 5;
      out << x << ',' << y;
      out << "\" />" << std::endl;
    }

    out << "</svg>" << std::endl;
  }
#endif
}



double carve::triangulate::detail::vertex_info::triScore(const vertex_info *p, const vertex_info *v, const vertex_info *n) {

  // different scoring functions.
#if 0
  bool convex = isLeft(p, v, n);
  if (!convex) return -1e-5;

  double a1 = carve::geom2d::atan2(p->p - v->p) - carve::geom2d::atan2(n->p - v->p);
  double a2 = carve::geom2d::atan2(v->p - n->p) - carve::geom2d::atan2(p->p - n->p);
  if (a1 < 0) a1 += M_PI * 2;
  if (a2 < 0) a2 += M_PI * 2;

  return std::min(a1, std::min(a2, M_PI - a1 - a2)) / (M_PI / 3);
#endif

#if 1
  // range: 0 - 1
  double a, b, c;

  bool convex = isLeft(p, v, n);
  if (!convex) return -1e-5;

  a = (n->p - v->p).length();
  b = (p->p - n->p).length();
  c = (v->p - p->p).length();

  if (a < 1e-10 || b < 1e-10 || c < 1e-10) return 0.0;

  return std::max(std::min((a+b)/c, std::min((a+c)/b, (b+c)/a)) - 1.0, 0.0);
#endif
}



double carve::triangulate::detail::vertex_info::calcScore() const {

#if 0
  // examine only this triangle.
  double this_tri = triScore(prev, this, next);
  return this_tri;
#endif

#if 1
  // attempt to look ahead in the neighbourhood to attempt to clip ears that have good neighbours.
  double this_tri = triScore(prev, this, next);
  double next_tri = triScore(prev, next, next->next);
  double prev_tri = triScore(prev->prev, prev, next);

  return this_tri + std::max(next_tri, prev_tri) * .2;
#endif

#if 0
  // attempt to penalise ears that will require producing a sliver triangle.
  double score = triScore(prev, this, next);

  double a1, a2;
  a1 = carve::geom2d::atan2(prev->p - next->p);
  a2 = carve::geom2d::atan2(next->next->p - next->p);
  if (fabs(a1 - a2) < 1e-5) score -= .5;

  a1 = carve::geom2d::atan2(next->p - prev->p);
  a2 = carve::geom2d::atan2(prev->prev->p - prev->p);
  if (fabs(a1 - a2) < 1e-5) score -= .5;

  return score;
#endif
}



bool carve::triangulate::detail::vertex_info::isClipable() const {
  for (const vertex_info *v_test = next->next; v_test != prev; v_test = v_test->next) {
    if (v_test->convex) {
      continue;
    }

    if (v_test->p == prev->p ||
        v_test->p == next->p) {
      continue;
    }

    if (v_test->p == p) {
      if (v_test->next->p == prev->p &&
          v_test->prev->p == next->p) {
        return false;
      }
      if (v_test->next->p == prev->p ||
          v_test->prev->p == next->p) {
        continue;
      }
    }

    if (pointInTriangle(prev, this, next, v_test)) {
      return false;
    }
  }
  return true;
}



size_t carve::triangulate::detail::removeDegeneracies(vertex_info *&begin, std::vector<carve::triangulate::tri_idx> &result) {
  vertex_info *v;
  vertex_info *n;
  size_t count = 0;
  size_t remain = 0;

  v = begin;
  do {
    v = v->next;
    ++remain;
  } while (v != begin);

  v = begin;
  do {
    if (remain < 4) break;

    bool remove = false;
    if (v->p == v->next->p) {
      remove = true;
    } else if (v->p == v->next->next->p) {
      if (v->next->p == v->next->next->next->p) {
        // a 'z' in the loop: z (a) b a b c -> remove a-b-a -> z (a) a b c -> remove a-a-b (next loop) -> z a b c
        // z --(a)-- b
        //         /
        //        /
        //      a -- b -- d
        remove = true;
      } else {
        // a 'shard' in the loop: z (a) b a c d -> remove a-b-a -> z (a) a b c d -> remove a-a-b (next loop) -> z a b c d
        // z --(a)-- b
        //         /
        //        /
        //      a -- c -- d
        // n.b. can only do this if the shard is pointing out of the polygon. i.e. b is outside z-a-c
        remove = !internalToAngle(v->next->next->next, v, v->prev, v->next->p);
      }
    }

    if (remove) {
      result.push_back(carve::triangulate::tri_idx(v->idx, v->next->idx, v->next->next->idx));
      n = v->next;
      if (n == begin) begin = n->next;
      n->remove();
      count++;
      remain--;
      delete n;
    } else {
      v = v->next;
    }
  } while (v != begin);
  return count;
}



bool carve::triangulate::detail::splitAndResume(vertex_info *begin, std::vector<carve::triangulate::tri_idx> &result) {
  vertex_info *v1, *v2;

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  {
    std::vector<carve::triangulate::tri_idx> dummy;
    std::vector<carve::geom2d::P2> dummy_p;
    vertex_info *v = begin;
    do {
      dummy_p.push_back(v->p);
      v = v->next;
    } while (v != begin);
    std::cerr << "input to splitAndResume:" << std::endl;
    dumpPoly(dummy_p, dummy);
  }
#endif


  if (!findDiagonal(begin, v1, v2)) return false;

  vertex_info *v1_copy = new vertex_info(*v1);
  vertex_info *v2_copy = new vertex_info(*v2);

  v1->next = v2;
  v2->prev = v1;

  v1_copy->next->prev = v1_copy;
  v2_copy->prev->next = v2_copy;

  v1_copy->prev = v2_copy;
  v2_copy->next = v1_copy;

  bool r1 = doTriangulate(v1, result);
  bool r2 =  doTriangulate(v1_copy, result);
  return r1 && r2;
}



bool carve::triangulate::detail::doTriangulate(vertex_info *begin, std::vector<carve::triangulate::tri_idx> &result) {
#if defined(CARVE_DEBUG)
  std::cerr << "entering doTriangulate" << std::endl;
#endif

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  {
    std::vector<carve::triangulate::tri_idx> dummy;
    std::vector<carve::geom2d::P2> dummy_p;
    vertex_info *v = begin;
    do {
      dummy_p.push_back(v->p);
      v = v->next;
    } while (v != begin);
    dumpPoly(dummy_p, dummy);
  }
#endif

  EarQueue vq;

  vertex_info *v = begin;
  size_t remain = 0;
  do {
    if (v->isCandidate()) vq.push(v);
    v = v->next;
    remain++;
  } while (v != begin);

#if defined(CARVE_DEBUG)
  std::cerr << "remain = " << remain << std::endl;
#endif

  while (remain > 3 && vq.size()) {
    vertex_info *v = vq.pop();
    if (!v->isClipable()) {
      v->failed = true;
      continue;
    }

  continue_clipping:
    vertex_info *n = v->next;
    vertex_info *p = v->prev;

    result.push_back(carve::triangulate::tri_idx(v->prev->idx, v->idx, v->next->idx));

#if defined(CARVE_DEBUG)
    {
      std::vector<carve::geom2d::P2> temp;
      temp.push_back(v->prev->p);
      temp.push_back(v->p);
      temp.push_back(v->next->p);
      std::cerr << "clip " << v << " idx = " << v->idx << " score = " << v->score << " area = " << carve::geom2d::signedArea(temp) << " " << temp[0] << " " << temp[1] << " " << temp[2] << std::endl;
    }
#endif

    v->remove();
    if (v == begin) begin = v->next;
    delete v;

    if (--remain == 3) break;

    vq.updateVertex(n);
    vq.updateVertex(p);

    if (n->score < p->score) { std::swap(n, p); }

    if (n->score > 0.25 && n->isCandidate() && n->isClipable()) {
      vq.remove(n);
      v = n;
#if defined(CARVE_DEBUG)
      std::cerr << " continue clipping (n), score = " << n->score << std::endl;
#endif
      goto continue_clipping;
    }

    if (p->score > 0.25 && p->isCandidate() && p->isClipable()) {
      vq.remove(p);
      v = p;
#if defined(CARVE_DEBUG)
      std::cerr << " continue clipping (p), score = " << n->score << std::endl;
#endif
      goto continue_clipping;
    }

#if defined(CARVE_DEBUG)
    std::cerr << "looking for new start point" << std::endl;
    std::cerr << "remain = " << remain << std::endl;
#endif
  }

#if defined(CARVE_DEBUG)
  std::cerr << "doTriangulate complete; remain=" << remain << std::endl;
#endif

  if (remain > 3) {
#if defined(CARVE_DEBUG)
    std::cerr << "before removeDegeneracies: remain=" << remain << std::endl;
#endif
    remain -= removeDegeneracies(begin, result);
#if defined(CARVE_DEBUG)
    std::cerr << "after removeDegeneracies: remain=" << remain << std::endl;
#endif

    if (remain > 3) {
      return splitAndResume(begin, result);
    }
  }

  if (remain == 3) {
    result.push_back(carve::triangulate::tri_idx(begin->idx, begin->next->idx, begin->next->next->idx));
  }

  vertex_info *d = begin;
  do {
    vertex_info *n = d->next;
    delete d;
    d = n;
  } while (d != begin);

  return true;
}



bool testCandidateAttachment(const std::vector<std::vector<carve::geom2d::P2> > &poly,
                             std::vector<std::pair<size_t, size_t> > &current_f_loop,
                             size_t curr,
                             carve::geom2d::P2 hole_min) {
  const size_t SZ = current_f_loop.size();

  if (!carve::geom2d::internalToAngle(pvert(poly, current_f_loop[(curr+1) % SZ]),
                                      pvert(poly, current_f_loop[curr]),
                                      pvert(poly, current_f_loop[(curr+SZ-1) % SZ]),
                                      hole_min)) {
    return false;
  }

  if (hole_min == pvert(poly, current_f_loop[curr])) {
    return true;
  }

  carve::geom2d::LineSegment2 test(hole_min, pvert(poly, current_f_loop[curr]));

  size_t v1 = current_f_loop.size() - 1;
  size_t v2 = 0;
  double v1_side = carve::geom2d::orient2d(test.v1, test.v2, pvert(poly, current_f_loop[v1]));
  double v2_side = 0;

  while (v2 != current_f_loop.size()) {
    v2_side = carve::geom2d::orient2d(test.v1, test.v2, pvert(poly, current_f_loop[v2]));

    if (v1_side != v2_side) {
      // XXX: need to test vertices, not indices, because they may
      // be duplicated.
      if (pvert(poly, current_f_loop[v1]) != pvert(poly, current_f_loop[curr]) &&
          pvert(poly, current_f_loop[v2]) != pvert(poly, current_f_loop[curr])) {
        carve::geom2d::LineSegment2 test2(pvert(poly, current_f_loop[v1]), pvert(poly, current_f_loop[v2]));
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



void
carve::triangulate::incorporateHolesIntoPolygon(
    const std::vector<std::vector<carve::geom2d::P2> > &poly,
    std::vector<std::pair<size_t, size_t> > &result,
    size_t poly_loop,
    const std::vector<size_t> &hole_loops) {
  typedef std::vector<carve::geom2d::P2> loop_t;

  size_t N = poly[poly_loop].size();

  // work out how much space to reserve for the patched in holes.
  for (size_t i = 0; i < hole_loops.size(); i++) {
    N += 2 + poly[hole_loops[i]].size();
  }

  // this is the vector that we will build the result in.
  result.clear();
  result.reserve(N);

  // this is a heap of result indices that defines the vertex test order.
  std::vector<size_t> f_loop_heap;
  f_loop_heap.reserve(N);

  // add the poly loop to result.
  for (size_t i = 0; i < poly[poly_loop].size(); ++i) {
    result.push_back(std::make_pair((size_t)poly_loop, i));
  }

  if (hole_loops.size() == 0) {
    return;
  }

  std::vector<std::pair<size_t, size_t> > h_loop_min_vertex;

  h_loop_min_vertex.reserve(hole_loops.size());

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
  carve::geom2d::P2 h_min, h_max;
  h_min = h_max = poly[hole_loops[0]][0];
  for (size_t i = 0; i < hole_loops.size(); ++i) {
    const loop_t &hole = poly[hole_loops[i]];
    for (size_t j = 0; j < hole.size(); ++j) {
      assign_op(h_min, h_min, hole[j], carve::util::min_functor());
      assign_op(h_max, h_max, hole[j], carve::util::max_functor());
    }
  }
  // choose the axis for which the bbox is largest.
  int axis = (h_max.x - h_min.x) > (h_max.y - h_min.y) ? 0 : 1;

  // for each hole, find the minimum vertex in the chosen axis.
  for (size_t i = 0; i < hole_loops.size(); ++i) {
    const loop_t &hole = poly[hole_loops[i]];
    size_t best, curr;
    best = 0;
    for (curr = 1; curr != hole.size(); ++curr) {
      if (detail::axisOrdering(hole[curr], hole[best], axis)) {
        best = curr;
      }
    }
    h_loop_min_vertex.push_back(std::make_pair(hole_loops[i], best));
  }

  // sort the holes by the minimum vertex.
  std::sort(h_loop_min_vertex.begin(), h_loop_min_vertex.end(), order_h_loops_2d(poly, axis));

  // now, for each hole, find a vertex in the current polygon loop that it can be joined to.
  for (unsigned i = 0; i < h_loop_min_vertex.size(); ++i) {
    // the index of the vertex in the hole to connect.
    size_t hole_i = h_loop_min_vertex[i].first;
    size_t hole_i_connect = h_loop_min_vertex[i].second;

    carve::geom2d::P2 hole_min = poly[hole_i][hole_i_connect];

    f_loop_heap.clear();
    // we order polygon loop vertices that may be able to be connected
    // to the hole vertex by their distance to the hole vertex
    heap_ordering_2d _heap_ordering(poly, result, hole_min, axis);

    const size_t SZ = result.size();
    for (size_t j = 0; j < SZ; ++j) {
      // it is guaranteed that there exists a polygon vertex with
      // coord < the min hole coord chosen, which can be joined to
      // the min hole coord without crossing the polygon
      // boundary. also, because we merge holes in ascending
      // order, it is also true that this join can never cross
      // another hole (and that doesn't need to be tested for).
      if (pvert(poly, result[j]).v[axis] <= hole_min.v[axis]) {
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
    size_t attachment_point = result.size();

    while (f_loop_heap.size()) {
      std::pop_heap(f_loop_heap.begin(), f_loop_heap.end(), _heap_ordering);
      size_t curr = f_loop_heap.back();
      f_loop_heap.pop_back();
      // test the candidate join from result[curr] to hole_min

      if (!testCandidateAttachment(poly, result, curr, hole_min)) {
        continue;
      }

      attachment_point = curr;
      break;
    }

    if (attachment_point == result.size()) {
      CARVE_FAIL("didn't manage to link up hole!");
    }

    patchHoleIntoPolygon_2d(result, attachment_point, hole_i, hole_i_connect, poly[hole_i].size());
  }
}



std::vector<std::pair<size_t, size_t> >
carve::triangulate::incorporateHolesIntoPolygon(const std::vector<std::vector<carve::geom2d::P2> > &poly) {
#if 1
  std::vector<std::pair<size_t, size_t> > result;
  std::vector<size_t> hole_indices;
  hole_indices.reserve(poly.size() - 1);
  for (size_t i = 1; i < poly.size(); ++i) {
    hole_indices.push_back(i);
  }

  incorporateHolesIntoPolygon(poly, result, 0, hole_indices);

  return result;

#else
  typedef std::vector<carve::geom2d::P2> loop_t;
  size_t N = poly[0].size();
  //
  // work out how much space to reserve for the patched in holes.
  for (size_t i = 0; i < poly.size(); i++) {
    N += 2 + poly[i].size();
  }

  // this is the vector that we will build the result in.
  std::vector<std::pair<size_t, size_t> > current_f_loop;
  current_f_loop.reserve(N);

  // this is a heap of current_f_loop indices that defines the vertex test order.
  std::vector<size_t> f_loop_heap;
  f_loop_heap.reserve(N);

  // add the poly loop to current_f_loop.
  for (size_t i = 0; i < poly[0].size(); ++i) {
    current_f_loop.push_back(std::make_pair((size_t)0, i));
  }

  if (poly.size() == 1) {
    return current_f_loop;
  }

  std::vector<std::pair<size_t, size_t> > h_loop_min_vertex;

  h_loop_min_vertex.reserve(poly.size() - 1);

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
  double min_x, min_y, max_x, max_y;
  min_x = max_x = poly[1][0].x;
  min_y = max_y = poly[1][0].y;
  for (size_t i = 1; i < poly.size(); ++i) {
    const loop_t &hole = poly[i];
    for (size_t j = 0; j < hole.size(); ++j) {
      min_x = std::min(min_x, hole[j].x);
      min_y = std::min(min_y, hole[j].y);
      max_x = std::max(max_x, hole[j].x);
      max_y = std::max(max_y, hole[j].y);
    }
  }

  // choose the axis for which the bbox is largest.
  int axis = (max_x - min_x) > (max_y - min_y) ? 0 : 1;

  // for each hole, find the minimum vertex in the chosen axis.
  for (size_t i = 1; i < poly.size(); ++i) {
    const loop_t &hole = poly[i];
    size_t best, curr;
    best = 0;
    for (curr = 1; curr != hole.size(); ++curr) {
      if (detail::axisOrdering(hole[curr], hole[best], axis)) {
        best = curr;
      }
    }
    h_loop_min_vertex.push_back(std::make_pair(i, best));
  }

  // sort the holes by the minimum vertex.
  std::sort(h_loop_min_vertex.begin(), h_loop_min_vertex.end(), order_h_loops_2d(poly, axis));

  // now, for each hole, find a vertex in the current polygon loop that it can be joined to.
  for (unsigned i = 0; i < h_loop_min_vertex.size(); ++i) {
    // the index of the vertex in the hole to connect.
    size_t hole_i = h_loop_min_vertex[i].first;
    size_t hole_i_connect = h_loop_min_vertex[i].second;

    carve::geom2d::P2 hole_min = poly[hole_i][hole_i_connect];

    f_loop_heap.clear();
    // we order polygon loop vertices that may be able to be connected
    // to the hole vertex by their distance to the hole vertex
    heap_ordering_2d _heap_ordering(poly, current_f_loop, hole_min, axis);

    const size_t SZ = current_f_loop.size();
    for (size_t j = 0; j < SZ; ++j) {
      // it is guaranteed that there exists a polygon vertex with
      // coord < the min hole coord chosen, which can be joined to
      // the min hole coord without crossing the polygon
      // boundary. also, because we merge holes in ascending
      // order, it is also true that this join can never cross
      // another hole (and that doesn't need to be tested for).
      if (pvert(poly, current_f_loop[j]).v[axis] <= hole_min.v[axis]) {
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

      if (!testCandidateAttachment(poly, current_f_loop, curr, hole_min)) {
        continue;
      }

      attachment_point = curr;
      break;
    }

    if (attachment_point == current_f_loop.size()) {
      CARVE_FAIL("didn't manage to link up hole!");
    }

    patchHoleIntoPolygon_2d(current_f_loop, attachment_point, hole_i, hole_i_connect, poly[hole_i].size());
  }

  return current_f_loop;
#endif
}



std::vector<std::vector<std::pair<size_t, size_t> > >
carve::triangulate::mergePolygonsAndHoles(const std::vector<std::vector<carve::geom2d::P2> > &poly) {
  std::vector<size_t> poly_indices, hole_indices;

  poly_indices.reserve(poly.size());
  hole_indices.reserve(poly.size());

  for (size_t i = 0; i < poly.size(); ++i) {
    if (carve::geom2d::signedArea(poly[i]) < 0) {
      poly_indices.push_back(i);
    } else {
      hole_indices.push_back(i);
    }
  }

  std::vector<std::vector<std::pair<size_t, size_t> > > result;
  result.resize(poly_indices.size());

  if (hole_indices.size() == 0) {
    for (size_t i = 0; i < poly.size(); ++i) {
      result[i].resize(poly[i].size());
      for (size_t j = 0; j < poly[i].size(); ++j) {
        result[i].push_back(std::make_pair(i, j));
      }
    }
    return result;
  }

  if (poly_indices.size() == 1) {
    incorporateHolesIntoPolygon(poly, result[0], poly_indices[0], hole_indices);

    return result;
  }
  
  throw carve::exception("not implemented");
}



void carve::triangulate::triangulate(const std::vector<carve::geom2d::P2> &poly,
                                     std::vector<carve::triangulate::tri_idx> &result) {
  std::vector<detail::vertex_info *> vinfo;
  const size_t N = poly.size();

#if defined(CARVE_DEBUG)
  std::cerr << "TRIANGULATION BEGINS" << std::endl;
#endif

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  dumpPoly(poly, result);
#endif

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

  vinfo[0] = new detail::vertex_info(poly[0], 0);
  for (size_t i = 1; i < N-1; ++i) {
    vinfo[i] = new detail::vertex_info(poly[i], i);
    vinfo[i]->prev = vinfo[i-1];
    vinfo[i-1]->next = vinfo[i];
  }
  vinfo[N-1] = new detail::vertex_info(poly[N-1], N-1);
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

#if defined(CARVE_DEBUG)
  std::cerr << "TRIANGULATION ENDS" << std::endl;
#endif

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  dumpPoly(poly, result);
#endif
}



void carve::triangulate::detail::tri_pair_t::flip(vert_edge_t &old_edge,
                                                  vert_edge_t &new_edge,
                                                  vert_edge_t perim[4]) {
  unsigned ai, bi;
  unsigned cross_ai, cross_bi;

  findSharedEdge(ai, bi);
  old_edge = ordered_vert_edge_t(a->v[ai], b->v[bi]);

  cross_ai = P(ai);
  cross_bi = P(bi);
  new_edge = ordered_vert_edge_t(a->v[cross_ai], b->v[cross_bi]);

  score = -score;

  a->v[N(ai)] = b->v[cross_bi];
  b->v[N(bi)] = a->v[cross_ai];

  perim[0] = ordered_vert_edge_t(a->v[P(ai)], a->v[ai]);
  perim[1] = ordered_vert_edge_t(a->v[N(ai)], a->v[ai]); // this edge was a b-edge

  perim[2] = ordered_vert_edge_t(b->v[P(bi)], b->v[bi]);
  perim[3] = ordered_vert_edge_t(b->v[N(bi)], b->v[bi]); // this edge was an a-edge
}



void carve::triangulate::detail::tri_pairs_t::insert(unsigned a, unsigned b, carve::triangulate::tri_idx *t) {
  tri_pair_t *tp;
  if (a < b) {
    tp = storage[vert_edge_t(a,b)];
    if (!tp) {
      tp = storage[vert_edge_t(a,b)] = new tri_pair_t;
    }
    tp->a = t;
  } else {
    tp = storage[vert_edge_t(b,a)];
    if (!tp) {
      tp = storage[vert_edge_t(b,a)] = new tri_pair_t;
    }
    tp->b = t;
  }
}
