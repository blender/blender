/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#include <iostream>

#include <lemon/planarity.h>

#include <lemon/smart_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/connectivity.h>
#include <lemon/dim2.h>

#include "test_tools.h"

using namespace lemon;
using namespace lemon::dim2;

const int lgfn = 4;
const std::string lgf[lgfn] = {
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "@edges\n"
  "     label\n"
  "0 1  0\n"
  "0 2  0\n"
  "0 3  0\n"
  "0 4  0\n"
  "1 2  0\n"
  "1 3  0\n"
  "1 4  0\n"
  "2 3  0\n"
  "2 4  0\n"
  "3 4  0\n",

  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "@edges\n"
  "     label\n"
  "0 1  0\n"
  "0 2  0\n"
  "0 3  0\n"
  "0 4  0\n"
  "1 2  0\n"
  "1 3  0\n"
  "2 3  0\n"
  "2 4  0\n"
  "3 4  0\n",

  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "@edges\n"
  "     label\n"
  "0 3  0\n"
  "0 4  0\n"
  "0 5  0\n"
  "1 3  0\n"
  "1 4  0\n"
  "1 5  0\n"
  "2 3  0\n"
  "2 4  0\n"
  "2 5  0\n",

  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "@edges\n"
  "     label\n"
  "0 3  0\n"
  "0 4  0\n"
  "0 5  0\n"
  "1 3  0\n"
  "1 4  0\n"
  "1 5  0\n"
  "2 3  0\n"
  "2 5  0\n"
};



typedef SmartGraph Graph;
GRAPH_TYPEDEFS(Graph);

typedef PlanarEmbedding<SmartGraph> PE;
typedef PlanarDrawing<SmartGraph> PD;
typedef PlanarColoring<SmartGraph> PC;

void checkEmbedding(const Graph& graph, PE& pe) {
  int face_num = 0;

  Graph::ArcMap<int> face(graph, -1);

  for (ArcIt a(graph); a != INVALID; ++a) {
    if (face[a] == -1) {
      Arc b = a;
      while (face[b] == -1) {
        face[b] = face_num;
        b = pe.next(graph.oppositeArc(b));
      }
      check(face[b] == face_num, "Wrong face");
      ++face_num;
    }
  }
  check(face_num + countNodes(graph) - countConnectedComponents(graph) ==
        countEdges(graph) + 1, "Euler test does not passed");
}

void checkKuratowski(const Graph& graph, PE& pe) {
  std::map<int, int> degs;
  for (NodeIt n(graph); n != INVALID; ++n) {
    int deg = 0;
    for (IncEdgeIt e(graph, n); e != INVALID; ++e) {
      if (pe.kuratowski(e)) {
        ++deg;
      }
    }
    ++degs[deg];
  }
  for (std::map<int, int>::iterator it = degs.begin(); it != degs.end(); ++it) {
    check(it->first == 0 || it->first == 2 ||
          (it->first == 3 && it->second == 6) ||
          (it->first == 4 && it->second == 5),
          "Wrong degree in Kuratowski graph");
  }

  // Not full test
  check((degs[3] == 0) != (degs[4] == 0), "Wrong Kuratowski graph");
}

bool intersect(Point<int> e1, Point<int> e2, Point<int> f1, Point<int> f2) {
  int l, r;
  if (std::min(e1.x, e2.x) > std::max(f1.x, f2.x)) return false;
  if (std::max(e1.x, e2.x) < std::min(f1.x, f2.x)) return false;
  if (std::min(e1.y, e2.y) > std::max(f1.y, f2.y)) return false;
  if (std::max(e1.y, e2.y) < std::min(f1.y, f2.y)) return false;

  l = (e2.x - e1.x) * (f1.y - e1.y) - (e2.y - e1.y) * (f1.x - e1.x);
  r = (e2.x - e1.x) * (f2.y - e1.y) - (e2.y - e1.y) * (f2.x - e1.x);
  if (!((l >= 0 && r <= 0) || (l <= 0 && r >= 0))) return false;
  l = (f2.x - f1.x) * (e1.y - f1.y) - (f2.y - f1.y) * (e1.x - f1.x);
  r = (f2.x - f1.x) * (e2.y - f1.y) - (f2.y - f1.y) * (e2.x - f1.x);
  if (!((l >= 0 && r <= 0) || (l <= 0 && r >= 0))) return false;
  return true;
}

bool collinear(Point<int> p, Point<int> q, Point<int> r) {
  int v;
  v = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
  if (v != 0) return false;
  v = (q.x - p.x) * (r.x - p.x) + (q.y - p.y) * (r.y - p.y);
  if (v < 0) return false;
  return true;
}

void checkDrawing(const Graph& graph, PD& pd) {
  for (Graph::NodeIt n(graph); n != INVALID; ++n) {
    Graph::NodeIt m(n);
    for (++m; m != INVALID; ++m) {
      check(pd[m] != pd[n], "Two nodes with identical coordinates");
    }
  }

  for (Graph::EdgeIt e(graph); e != INVALID; ++e) {
    for (Graph::EdgeIt f(e); f != e; ++f) {
      Point<int> e1 = pd[graph.u(e)];
      Point<int> e2 = pd[graph.v(e)];
      Point<int> f1 = pd[graph.u(f)];
      Point<int> f2 = pd[graph.v(f)];

      if (graph.u(e) == graph.u(f)) {
        check(!collinear(e1, e2, f2), "Wrong drawing");
      } else if (graph.u(e) == graph.v(f)) {
        check(!collinear(e1, e2, f1), "Wrong drawing");
      } else if (graph.v(e) == graph.u(f)) {
        check(!collinear(e2, e1, f2), "Wrong drawing");
      } else if (graph.v(e) == graph.v(f)) {
        check(!collinear(e2, e1, f1), "Wrong drawing");
      } else {
        check(!intersect(e1, e2, f1, f2), "Wrong drawing");
      }
    }
  }
}

void checkColoring(const Graph& graph, PC& pc, int num) {
  for (NodeIt n(graph); n != INVALID; ++n) {
    check(pc.colorIndex(n) >= 0 && pc.colorIndex(n) < num,
          "Wrong coloring");
  }
  for (EdgeIt e(graph); e != INVALID; ++e) {
    check(pc.colorIndex(graph.u(e)) != pc.colorIndex(graph.v(e)),
          "Wrong coloring");
  }
}

int main() {

  for (int i = 0; i < lgfn; ++i) {
    std::istringstream lgfs(lgf[i]);

    SmartGraph graph;
    graphReader(graph, lgfs).run();

    check(simpleGraph(graph), "Test graphs must be simple");

    PE pe(graph);
    bool planar = pe.run();
    check(checkPlanarity(graph) == planar, "Planarity checking failed");

    if (planar) {
      checkEmbedding(graph, pe);

      PlanarDrawing<Graph> pd(graph);
      pd.run(pe.embeddingMap());
      checkDrawing(graph, pd);

      PlanarColoring<Graph> pc(graph);
      pc.runFiveColoring(pe.embeddingMap());
      checkColoring(graph, pc, 5);

    } else {
      checkKuratowski(graph, pe);
    }
  }

  return 0;
}
