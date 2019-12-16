

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Use this file to test new functionality
 *
 ******************************************************************************/

#include "levelset.h"
#include "commonkernels.h"
#include "particle.h"
#include <cmath>

using namespace std;

namespace Manta {

// two simple example kernels

struct reductionTest : public KernelBase {
  reductionTest(const Grid<Real> &v) : KernelBase(&v, 0), v(v), sum(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &v, double &sum)
  {
    sum += v[idx];
  }
  inline operator double()
  {
    return sum;
  }
  inline double &getRet()
  {
    return sum;
  }
  inline const Grid<Real> &getArg0()
  {
    return v;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel reductionTest ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, v, sum);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  reductionTest(reductionTest &o, tbb::split) : KernelBase(o), v(o.v), sum(0)
  {
  }
  void join(const reductionTest &o)
  {
    sum += o.sum;
  }
  const Grid<Real> &v;
  double sum;
};

struct minReduction : public KernelBase {
  minReduction(const Grid<Real> &v) : KernelBase(&v, 0), v(v), sum(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &v, double &sum)
  {
    if (sum < v[idx])
      sum = v[idx];
  }
  inline operator double()
  {
    return sum;
  }
  inline double &getRet()
  {
    return sum;
  }
  inline const Grid<Real> &getArg0()
  {
    return v;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel minReduction ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, v, sum);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  minReduction(minReduction &o, tbb::split) : KernelBase(o), v(o.v), sum(0)
  {
  }
  void join(const minReduction &o)
  {
    sum = min(sum, o.sum);
  }
  const Grid<Real> &v;
  double sum;
};

// ... add more test code here if necessary ...

}  // namespace Manta
