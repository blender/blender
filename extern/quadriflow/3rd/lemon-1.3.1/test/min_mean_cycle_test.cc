/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
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
#include <sstream>

#include <lemon/smart_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/path.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concept_check.h>

#include <lemon/karp_mmc.h>
#include <lemon/hartmann_orlin_mmc.h>
#include <lemon/howard_mmc.h>

#include "test_tools.h"

using namespace lemon;

char test_lgf[] =
  "@nodes\n"
  "label\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "6\n"
  "7\n"
  "@arcs\n"
  "    len1 len2 len3 len4  c1 c2 c3 c4\n"
  "1 2    1    1    1    1   0  0  0  0\n"
  "2 4    5    5    5    5   1  0  0  0\n"
  "2 3    8    8    8    8   0  0  0  0\n"
  "3 2   -2    0    0    0   1  0  0  0\n"
  "3 4    4    4    4    4   0  0  0  0\n"
  "3 7   -4   -4   -4   -4   0  0  0  0\n"
  "4 1    2    2    2    2   0  0  0  0\n"
  "4 3    3    3    3    3   1  0  0  0\n"
  "4 4    3    3    0    0   0  0  1  0\n"
  "5 2    4    4    4    4   0  0  0  0\n"
  "5 6    3    3    3    3   0  1  0  0\n"
  "6 5    2    2    2    2   0  1  0  0\n"
  "6 4   -1   -1   -1   -1   0  0  0  0\n"
  "6 7    1    1    1    1   0  0  0  0\n"
  "7 7    4    4    4   -1   0  0  0  1\n";


// Check the interface of an MMC algorithm
template <typename GR, typename Cost>
struct MmcClassConcept
{
  template <typename MMC>
  struct Constraints {
    void constraints() {
      const Constraints& me = *this;

      typedef typename MMC
        ::template SetPath<ListPath<GR> >
        ::template SetLargeCost<Cost>
        ::Create MmcAlg;
      MmcAlg mmc(me.g, me.cost);
      const MmcAlg& const_mmc = mmc;

      typename MmcAlg::Tolerance tol = const_mmc.tolerance();
      mmc.tolerance(tol);

      b = mmc.cycle(p).run();
      b = mmc.findCycleMean();
      b = mmc.findCycle();

      v = const_mmc.cycleCost();
      i = const_mmc.cycleSize();
      d = const_mmc.cycleMean();
      p = const_mmc.cycle();
    }

    typedef concepts::ReadMap<typename GR::Arc, Cost> CM;

    GR g;
    CM cost;
    ListPath<GR> p;
    Cost v;
    int i;
    double d;
    bool b;
  };
};

// Perform a test with the given parameters
template <typename MMC>
void checkMmcAlg(const SmartDigraph& gr,
                 const SmartDigraph::ArcMap<int>& lm,
                 const SmartDigraph::ArcMap<int>& cm,
                 int cost, int size) {
  MMC alg(gr, lm);
  check(alg.findCycleMean(), "Wrong result");
  check(alg.cycleMean() == static_cast<double>(cost) / size,
        "Wrong cycle mean");
  alg.findCycle();
  check(alg.cycleCost() == cost && alg.cycleSize() == size,
        "Wrong path");
  SmartDigraph::ArcMap<int> cycle(gr, 0);
  for (typename MMC::Path::ArcIt a(alg.cycle()); a != INVALID; ++a) {
    ++cycle[a];
  }
  for (SmartDigraph::ArcIt a(gr); a != INVALID; ++a) {
    check(cm[a] == cycle[a], "Wrong path");
  }
}

// Class for comparing types
template <typename T1, typename T2>
struct IsSameType {
  static const int result = 0;
};

template <typename T>
struct IsSameType<T,T> {
  static const int result = 1;
};


int main() {
  #ifdef LEMON_HAVE_LONG_LONG
    typedef long long long_int;
  #else
    typedef long long_int;
  #endif

  // Check the interface
  {
    typedef concepts::Digraph GR;

    // KarpMmc
    checkConcept< MmcClassConcept<GR, int>,
                  KarpMmc<GR, concepts::ReadMap<GR::Arc, int> > >();
    checkConcept< MmcClassConcept<GR, float>,
                  KarpMmc<GR, concepts::ReadMap<GR::Arc, float> > >();

    // HartmannOrlinMmc
    checkConcept< MmcClassConcept<GR, int>,
                  HartmannOrlinMmc<GR, concepts::ReadMap<GR::Arc, int> > >();
    checkConcept< MmcClassConcept<GR, float>,
                  HartmannOrlinMmc<GR, concepts::ReadMap<GR::Arc, float> > >();

    // HowardMmc
    checkConcept< MmcClassConcept<GR, int>,
                  HowardMmc<GR, concepts::ReadMap<GR::Arc, int> > >();
    checkConcept< MmcClassConcept<GR, float>,
                  HowardMmc<GR, concepts::ReadMap<GR::Arc, float> > >();

    check((IsSameType<HowardMmc<GR, concepts::ReadMap<GR::Arc, int> >
           ::LargeCost, long_int>::result == 1), "Wrong LargeCost type");
    check((IsSameType<HowardMmc<GR, concepts::ReadMap<GR::Arc, float> >
           ::LargeCost, double>::result == 1), "Wrong LargeCost type");
  }

  // Run various tests
  {
    typedef SmartDigraph GR;
    DIGRAPH_TYPEDEFS(GR);

    GR gr;
    IntArcMap l1(gr), l2(gr), l3(gr), l4(gr);
    IntArcMap c1(gr), c2(gr), c3(gr), c4(gr);

    std::istringstream input(test_lgf);
    digraphReader(gr, input).
      arcMap("len1", l1).
      arcMap("len2", l2).
      arcMap("len3", l3).
      arcMap("len4", l4).
      arcMap("c1", c1).
      arcMap("c2", c2).
      arcMap("c3", c3).
      arcMap("c4", c4).
      run();

    // Karp
    checkMmcAlg<KarpMmc<GR, IntArcMap> >(gr, l1, c1,  6, 3);
    checkMmcAlg<KarpMmc<GR, IntArcMap> >(gr, l2, c2,  5, 2);
    checkMmcAlg<KarpMmc<GR, IntArcMap> >(gr, l3, c3,  0, 1);
    checkMmcAlg<KarpMmc<GR, IntArcMap> >(gr, l4, c4, -1, 1);

    // HartmannOrlin
    checkMmcAlg<HartmannOrlinMmc<GR, IntArcMap> >(gr, l1, c1,  6, 3);
    checkMmcAlg<HartmannOrlinMmc<GR, IntArcMap> >(gr, l2, c2,  5, 2);
    checkMmcAlg<HartmannOrlinMmc<GR, IntArcMap> >(gr, l3, c3,  0, 1);
    checkMmcAlg<HartmannOrlinMmc<GR, IntArcMap> >(gr, l4, c4, -1, 1);

    // Howard
    checkMmcAlg<HowardMmc<GR, IntArcMap> >(gr, l1, c1,  6, 3);
    checkMmcAlg<HowardMmc<GR, IntArcMap> >(gr, l2, c2,  5, 2);
    checkMmcAlg<HowardMmc<GR, IntArcMap> >(gr, l3, c3,  0, 1);
    checkMmcAlg<HowardMmc<GR, IntArcMap> >(gr, l4, c4, -1, 1);

    // Howard with iteration limit
    HowardMmc<GR, IntArcMap> mmc(gr, l1);
    check((mmc.findCycleMean(2) == HowardMmc<GR, IntArcMap>::ITERATION_LIMIT),
      "Wrong termination cause");
    check((mmc.findCycleMean(4) == HowardMmc<GR, IntArcMap>::OPTIMAL),
      "Wrong termination cause");
  }

  return 0;
}
