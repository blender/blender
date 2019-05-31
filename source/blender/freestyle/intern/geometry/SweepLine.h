/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SWEEPLINE_H__
#define __SWEEPLINE_H__

/** \file
 * \ingroup freestyle
 * \brief Class to define a Sweep Line
 */

#include <list>
#include <vector>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/*! Class to define the intersection berween two segments*/
template<class Edge> class Intersection {
 public:
  template<class EdgeClass> Intersection(EdgeClass *eA, real ta, EdgeClass *eB, real tb)
  {
    EdgeA = eA;
    EdgeB = eB;
    tA = ta;
    tB = tb;
    userdata = 0;
  }

  Intersection(const Intersection &iBrother)
  {
    EdgeA = iBrother.EdgeA;
    EdgeB = iBrother.EdgeB;
    tA = iBrother.tA;
    tB = iBrother.tB;
    userdata = 0;
  }

  /*! returns the parameter giving the intersection, for the edge iEdge */
  real getParameter(Edge *iEdge)
  {
    if (iEdge == EdgeA) {
      return tA;
    }
    if (iEdge == EdgeB) {
      return tB;
    }
    return 0;
  }

 public:
  void *userdata;  // FIXME

  Edge *EdgeA;  // first segment
  Edge *EdgeB;  // second segment
  real tA;      // parameter defining the intersection point with respect to the segment EdgeA.
  real tB;      // parameter defining the intersection point with respect to the segment EdgeB.

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Intersection")
#endif
};

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4521)  // disable warning C4521: multiple copy constructors specified
#endif

template<class T, class Point> class Segment {
 public:
  Segment()
  {
  }

  Segment(T &s, const Point &iA, const Point &iB)
  {
    _edge = s;
    if (iA < iB) {
      A = iA;
      B = iB;
      _order = true;
    }
    else {
      A = iB;
      B = iA;
      _order = false;
    }
  }

  Segment(Segment<T, Point> &iBrother)
  {
    _edge = iBrother.edge();
    A = iBrother.A;
    B = iBrother.B;
    _Intersections = iBrother._Intersections;
    _order = iBrother._order;
  }

  Segment(const Segment<T, Point> &iBrother)
  {
    _edge = iBrother._edge;
    A = iBrother.A;
    B = iBrother.B;
    _Intersections = iBrother._Intersections;
    _order = iBrother._order;
  }

  ~Segment()
  {
    _Intersections.clear();
  }

  inline Point operator[](const unsigned short int &i) const
  {
    return (i % 2 == 0) ? A : B;
  }

  inline bool operator==(const Segment<T, Point> &iBrother)
  {
    if (_edge == iBrother._edge) {
      return true;
    }
    return false;
  }

  /* Adds an intersection for this segment */
  inline void AddIntersection(Intersection<Segment<T, Point>> *i)
  {
    _Intersections.push_back(i);
  }

  /*! Checks for a common vertex with another edge */
  inline bool CommonVertex(const Segment<T, Point> &S, Point &CP)
  {
    if ((A == S[0]) || (A == S[1])) {
      CP = A;
      return true;
    }
    if ((B == S[0]) || (B == S[1])) {
      CP = B;
      return true;
    }
    return false;
  }

  inline vector<Intersection<Segment<T, Point>> *> &intersections()
  {
    return _Intersections;
  }

  inline bool order()
  {
    return _order;
  }

  inline T &edge()
  {
    return _edge;
  }

 private:
  T _edge;
  Point A;
  Point B;
  std::vector<Intersection<Segment<T, Point>> *>
      _Intersections;  // list of intersections parameters
  bool _order;  // true if A and B are in the same order than _edge.A and _edge.B. false otherwise.

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Segment")
#endif
};

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

/*! defines a binary function that can be overload by the user to specify at each condition the
 * intersection between 2 edges must be computed
 */
template<class T1, class T2> struct binary_rule {
  binary_rule()
  {
  }
  template<class T3, class T4> binary_rule(const binary_rule<T3, T4> &brother)
  {
  }
  virtual ~binary_rule()
  {
  }

  virtual bool operator()(T1 &, T2 &)
  {
    return true;
  }
};

template<class T, class Point> class SweepLine {
 public:
  SweepLine()
  {
  }
  ~SweepLine()
  {
    for (typename vector<Intersection<Segment<T, Point>> *>::iterator i = _Intersections.begin(),
                                                                      iend = _Intersections.end();
         i != iend;
         i++) {
      delete (*i);
    }
  }

  inline void process(Point &p,
                      vector<Segment<T, Point> *> &segments,
#if 0
                      binary_rule<Segment<T, Point>, Segment<T, Point>> &binrule =
                          binary_rule<Segment<T, Point>, Segment<T, Point>>(),
#else
                      binary_rule<Segment<T, Point>, Segment<T, Point>> &binrule,
#endif
                      real epsilon = M_EPSILON)
  {
    // first we remove the segments that need to be removed and then we add the segments to add
    vector<Segment<T, Point> *> toadd;
    typename vector<Segment<T, Point> *>::iterator s, send;
    for (s = segments.begin(), send = segments.end(); s != send; s++) {
      if (p == (*(*s))[0]) {
        toadd.push_back((*s));
      }
      else {
        remove((*s));
      }
    }
    for (s = toadd.begin(), send = toadd.end(); s != send; s++) {
      add((*s), binrule, epsilon);
    }
  }

  inline void add(Segment<T, Point> *S,
#if 0
                  binary_rule<Segment<T, Point>, Segment<T, Point>> &binrule =
                      binary_rule<Segment<T, Point>, Segment<T, Point>>(),
#else
                  binary_rule<Segment<T, Point>, Segment<T, Point>> &binrule,
#endif
                  real epsilon)
  {
    real t, u;
    Point CP;
    Vec2r v0, v1, v2, v3;
    if (true == S->order()) {
      v0[0] = ((*S)[0])[0];
      v0[1] = ((*S)[0])[1];
      v1[0] = ((*S)[1])[0];
      v1[1] = ((*S)[1])[1];
    }
    else {
      v1[0] = ((*S)[0])[0];
      v1[1] = ((*S)[0])[1];
      v0[0] = ((*S)[1])[0];
      v0[1] = ((*S)[1])[1];
    }
    for (typename std::list<Segment<T, Point> *>::iterator s = _set.begin(), send = _set.end();
         s != send;
         s++) {
      Segment<T, Point> *currentS = (*s);
      if (true != binrule(*S, *currentS)) {
        continue;
      }

      if (true == currentS->order()) {
        v2[0] = ((*currentS)[0])[0];
        v2[1] = ((*currentS)[0])[1];
        v3[0] = ((*currentS)[1])[0];
        v3[1] = ((*currentS)[1])[1];
      }
      else {
        v3[0] = ((*currentS)[0])[0];
        v3[1] = ((*currentS)[0])[1];
        v2[0] = ((*currentS)[1])[0];
        v2[1] = ((*currentS)[1])[1];
      }
      if (S->CommonVertex(*currentS, CP)) {
        continue;  // the two edges have a common vertex->no need to check
      }

      if (GeomUtils::intersect2dSeg2dSegParametric(v0, v1, v2, v3, t, u, epsilon) ==
          GeomUtils::DO_INTERSECT) {
        // create the intersection
        Intersection<Segment<T, Point>> *inter = new Intersection<Segment<T, Point>>(
            S, t, currentS, u);
        // add it to the intersections list
        _Intersections.push_back(inter);
        // add this intersection to the first edge intersections list
        S->AddIntersection(inter);
        // add this intersection to the second edge intersections list
        currentS->AddIntersection(inter);
      }
    }
    // add the added segment to the list of active segments
    _set.push_back(S);
  }

  inline void remove(Segment<T, Point> *s)
  {
    if (s->intersections().size() > 0) {
      _IntersectedEdges.push_back(s);
    }
    _set.remove(s);
  }

  vector<Segment<T, Point> *> &intersectedEdges()
  {
    return _IntersectedEdges;
  }

  vector<Intersection<Segment<T, Point>> *> &intersections()
  {
    return _Intersections;
  }

 private:
  std::list<Segment<T, Point> *>
      _set;  // set of active edges for a given position of the sweep line
  std::vector<Segment<T, Point> *> _IntersectedEdges;             // the list of intersected edges
  std::vector<Intersection<Segment<T, Point>> *> _Intersections;  // the list of all intersections.

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SweepLine")
#endif
};

} /* namespace Freestyle */

#endif  // __SWEEPLINE_H__
