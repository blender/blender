

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
 * Common grid kernels
 *
 ******************************************************************************/

#ifndef _COMMONKERNELS_H
#define _COMMONKERNELS_H

#include "general.h"
#include "kernel.h"
#include "grid.h"

namespace Manta {

//! Kernel: Invert real values, if positive and fluid

struct InvertCheckFluid : public KernelBase {
  InvertCheckFluid(const FlagGrid &flags, Grid<Real> &grid)
      : KernelBase(&flags, 0), flags(flags), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const FlagGrid &flags, Grid<Real> &grid) const
  {
    if (flags.isFluid(idx) && grid[idx] > 0)
      grid[idx] = 1.0 / grid[idx];
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel InvertCheckFluid ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, grid);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &grid;
};

//! Kernel: Squared sum over grid

struct GridSumSqr : public KernelBase {
  GridSumSqr(const Grid<Real> &grid) : KernelBase(&grid, 0), grid(grid), sum(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &grid, double &sum)
  {
    sum += square((double)grid[idx]);
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
    return grid;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel GridSumSqr ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, grid, sum);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  GridSumSqr(GridSumSqr &o, tbb::split) : KernelBase(o), grid(o.grid), sum(0)
  {
  }
  void join(const GridSumSqr &o)
  {
    sum += o.sum;
  }
  const Grid<Real> &grid;
  double sum;
};

//! Kernel: rotation operator \nabla x v for centered vector fields

struct CurlOp : public KernelBase {
  CurlOp(const Grid<Vec3> &grid, Grid<Vec3> &dst) : KernelBase(&grid, 1), grid(grid), dst(dst)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<Vec3> &grid, Grid<Vec3> &dst) const
  {
    Vec3 v = Vec3(0.,
                  0.,
                  0.5 * ((grid(i + 1, j, k).y - grid(i - 1, j, k).y) -
                         (grid(i, j + 1, k).x - grid(i, j - 1, k).x)));
    if (dst.is3D()) {
      v[0] = 0.5 * ((grid(i, j + 1, k).z - grid(i, j - 1, k).z) -
                    (grid(i, j, k + 1).y - grid(i, j, k - 1).y));
      v[1] = 0.5 * ((grid(i, j, k + 1).x - grid(i, j, k - 1).x) -
                    (grid(i + 1, j, k).z - grid(i - 1, j, k).z));
    }
    dst(i, j, k) = v;
  }
  inline const Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Grid<Vec3> &getArg1()
  {
    return dst;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel CurlOp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, grid, dst);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, grid, dst);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const Grid<Vec3> &grid;
  Grid<Vec3> &dst;
};
;

//! Kernel: divergence operator (from MAC grid)

struct DivergenceOpMAC : public KernelBase {
  DivergenceOpMAC(Grid<Real> &div, const MACGrid &grid) : KernelBase(&div, 1), div(div), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &div, const MACGrid &grid) const
  {
    Vec3 del = Vec3(grid(i + 1, j, k).x, grid(i, j + 1, k).y, 0.) - grid(i, j, k);
    if (grid.is3D())
      del[2] += grid(i, j, k + 1).z;
    else
      del[2] = 0.;
    div(i, j, k) = del.x + del.y + del.z;
  }
  inline Grid<Real> &getArg0()
  {
    return div;
  }
  typedef Grid<Real> type0;
  inline const MACGrid &getArg1()
  {
    return grid;
  }
  typedef MACGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel DivergenceOpMAC ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, div, grid);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, div, grid);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &div;
  const MACGrid &grid;
};

//! Kernel: gradient operator for MAC grid
struct GradientOpMAC : public KernelBase {
  GradientOpMAC(MACGrid &gradient, const Grid<Real> &grid)
      : KernelBase(&gradient, 1), gradient(gradient), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &gradient, const Grid<Real> &grid) const
  {
    Vec3 grad = (Vec3(grid(i, j, k)) - Vec3(grid(i - 1, j, k), grid(i, j - 1, k), 0.));
    if (grid.is3D())
      grad[2] -= grid(i, j, k - 1);
    else
      grad[2] = 0.;
    gradient(i, j, k) = grad;
  }
  inline MACGrid &getArg0()
  {
    return gradient;
  }
  typedef MACGrid type0;
  inline const Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel GradientOpMAC ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, gradient, grid);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, gradient, grid);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  MACGrid &gradient;
  const Grid<Real> &grid;
};

//! Kernel: centered gradient operator
struct GradientOp : public KernelBase {
  GradientOp(Grid<Vec3> &gradient, const Grid<Real> &grid)
      : KernelBase(&gradient, 1), gradient(gradient), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &gradient, const Grid<Real> &grid) const
  {
    Vec3 grad = 0.5 * Vec3(grid(i + 1, j, k) - grid(i - 1, j, k),
                           grid(i, j + 1, k) - grid(i, j - 1, k),
                           0.);
    if (grid.is3D())
      grad[2] = 0.5 * (grid(i, j, k + 1) - grid(i, j, k - 1));
    gradient(i, j, k) = grad;
  }
  inline Grid<Vec3> &getArg0()
  {
    return gradient;
  }
  typedef Grid<Vec3> type0;
  inline const Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel GradientOp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, gradient, grid);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, gradient, grid);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Vec3> &gradient;
  const Grid<Real> &grid;
};

//! Kernel: Laplace operator
struct LaplaceOp : public KernelBase {
  LaplaceOp(Grid<Real> &laplace, const Grid<Real> &grid)
      : KernelBase(&laplace, 1), laplace(laplace), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &laplace, const Grid<Real> &grid) const
  {
    laplace(i, j, k) = grid(i + 1, j, k) - 2.0 * grid(i, j, k) + grid(i - 1, j, k);
    laplace(i, j, k) += grid(i, j + 1, k) - 2.0 * grid(i, j, k) + grid(i, j - 1, k);
    if (grid.is3D()) {
      laplace(i, j, k) += grid(i, j, k + 1) - 2.0 * grid(i, j, k) + grid(i, j, k - 1);
    }
  }
  inline Grid<Real> &getArg0()
  {
    return laplace;
  }
  typedef Grid<Real> type0;
  inline const Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel LaplaceOp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, laplace, grid);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, laplace, grid);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &laplace;
  const Grid<Real> &grid;
};

//! Kernel: Curvature operator
struct CurvatureOp : public KernelBase {
  CurvatureOp(Grid<Real> &curv, const Grid<Real> &grid, const Real h)
      : KernelBase(&curv, 1), curv(curv), grid(grid), h(h)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &curv, const Grid<Real> &grid, const Real h) const
  {
    const Real over_h = 1.0 / h;
    const Real x = 0.5 * (grid(i + 1, j, k) - grid(i - 1, j, k)) * over_h;
    const Real y = 0.5 * (grid(i, j + 1, k) - grid(i, j - 1, k)) * over_h;
    const Real xx = (grid(i + 1, j, k) - 2.0 * grid(i, j, k) + grid(i - 1, j, k)) * over_h *
                    over_h;
    const Real yy = (grid(i, j + 1, k) - 2.0 * grid(i, j, k) + grid(i, j - 1, k)) * over_h *
                    over_h;
    const Real xy = 0.25 *
                    (grid(i + 1, j + 1, k) + grid(i - 1, j - 1, k) - grid(i - 1, j + 1, k) -
                     grid(i + 1, j - 1, k)) *
                    over_h * over_h;
    curv(i, j, k) = x * x * yy + y * y * xx - 2.0 * x * y * xy;
    Real denom = x * x + y * y;
    if (grid.is3D()) {
      const Real z = 0.5 * (grid(i, j, k + 1) - grid(i, j, k - 1)) * over_h;
      const Real zz = (grid(i, j, k + 1) - 2.0 * grid(i, j, k) + grid(i, j, k - 1)) * over_h *
                      over_h;
      const Real xz = 0.25 *
                      (grid(i + 1, j, k + 1) + grid(i - 1, j, k - 1) - grid(i - 1, j, k + 1) -
                       grid(i + 1, j, k - 1)) *
                      over_h * over_h;
      const Real yz = 0.25 *
                      (grid(i, j + 1, k + 1) + grid(i, j - 1, k - 1) - grid(i, j + 1, k - 1) -
                       grid(i, j - 1, k + 1)) *
                      over_h * over_h;
      curv(i, j, k) += x * x * zz + z * z * xx + y * y * zz + z * z * yy -
                       2.0 * (x * z * xz + y * z * yz);
      denom += z * z;
    }
    curv(i, j, k) /= std::pow(std::max(denom, VECTOR_EPSILON), 1.5);
  }
  inline Grid<Real> &getArg0()
  {
    return curv;
  }
  typedef Grid<Real> type0;
  inline const Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  inline const Real &getArg2()
  {
    return h;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel CurvatureOp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, curv, grid, h);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, curv, grid, h);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &curv;
  const Grid<Real> &grid;
  const Real h;
};

//! Kernel: get component at MAC positions
struct GetShiftedComponent : public KernelBase {
  GetShiftedComponent(const Grid<Vec3> &grid, Grid<Real> &comp, int dim)
      : KernelBase(&grid, 1), grid(grid), comp(comp), dim(dim)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<Vec3> &grid, Grid<Real> &comp, int dim) const
  {
    Vec3i ishift(i, j, k);
    ishift[dim]--;
    comp(i, j, k) = 0.5 * (grid(i, j, k)[dim] + grid(ishift)[dim]);
  }
  inline const Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Grid<Real> &getArg1()
  {
    return comp;
  }
  typedef Grid<Real> type1;
  inline int &getArg2()
  {
    return dim;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel GetShiftedComponent ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, grid, comp, dim);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, grid, comp, dim);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const Grid<Vec3> &grid;
  Grid<Real> &comp;
  int dim;
};
;

//! Kernel: get component (not shifted)
struct GetComponent : public KernelBase {
  GetComponent(const Grid<Vec3> &grid, Grid<Real> &comp, int dim)
      : KernelBase(&grid, 0), grid(grid), comp(comp), dim(dim)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Vec3> &grid, Grid<Real> &comp, int dim) const
  {
    comp[idx] = grid[idx][dim];
  }
  inline const Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Grid<Real> &getArg1()
  {
    return comp;
  }
  typedef Grid<Real> type1;
  inline int &getArg2()
  {
    return dim;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel GetComponent ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, grid, comp, dim);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const Grid<Vec3> &grid;
  Grid<Real> &comp;
  int dim;
};
;

//! Kernel: get norm of centered grid
struct GridNorm : public KernelBase {
  GridNorm(Grid<Real> &n, const Grid<Vec3> &grid) : KernelBase(&n, 0), n(n), grid(grid)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Real> &n, const Grid<Vec3> &grid) const
  {
    n[idx] = norm(grid[idx]);
  }
  inline Grid<Real> &getArg0()
  {
    return n;
  }
  typedef Grid<Real> type0;
  inline const Grid<Vec3> &getArg1()
  {
    return grid;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel GridNorm ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, n, grid);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &n;
  const Grid<Vec3> &grid;
};
;

//! Kernel: set component (not shifted)
struct SetComponent : public KernelBase {
  SetComponent(Grid<Vec3> &grid, const Grid<Real> &comp, int dim)
      : KernelBase(&grid, 0), grid(grid), comp(comp), dim(dim)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Vec3> &grid, const Grid<Real> &comp, int dim) const
  {
    grid[idx][dim] = comp[idx];
  }
  inline Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline const Grid<Real> &getArg1()
  {
    return comp;
  }
  typedef Grid<Real> type1;
  inline int &getArg2()
  {
    return dim;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel SetComponent ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, grid, comp, dim);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Vec3> &grid;
  const Grid<Real> &comp;
  int dim;
};
;

//! Kernel: compute centered velocity field from MAC
struct GetCentered : public KernelBase {
  GetCentered(Grid<Vec3> &center, const MACGrid &vel)
      : KernelBase(&center, 1), center(center), vel(vel)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &center, const MACGrid &vel) const
  {
    Vec3 v = 0.5 * (vel(i, j, k) + Vec3(vel(i + 1, j, k).x, vel(i, j + 1, k).y, 0.));
    if (vel.is3D())
      v[2] += 0.5 * vel(i, j, k + 1).z;
    else
      v[2] = 0.;
    center(i, j, k) = v;
  }
  inline Grid<Vec3> &getArg0()
  {
    return center;
  }
  typedef Grid<Vec3> type0;
  inline const MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel GetCentered ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, center, vel);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, center, vel);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Vec3> &center;
  const MACGrid &vel;
};
;

//! Kernel: compute MAC from centered velocity field
struct GetMAC : public KernelBase {
  GetMAC(MACGrid &vel, const Grid<Vec3> &center) : KernelBase(&vel, 1), vel(vel), center(center)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &vel, const Grid<Vec3> &center) const
  {
    Vec3 v = 0.5 * (center(i, j, k) + Vec3(center(i - 1, j, k).x, center(i, j - 1, k).y, 0.));
    if (vel.is3D())
      v[2] += 0.5 * center(i, j, k - 1).z;
    else
      v[2] = 0.;
    vel(i, j, k) = v;
  }
  inline MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline const Grid<Vec3> &getArg1()
  {
    return center;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel GetMAC ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i, j, k, vel, center);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, vel, center);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  MACGrid &vel;
  const Grid<Vec3> &center;
};
;

//! Fill in the domain boundary cells (i,j,k=0/size-1) from the neighboring cells
struct FillInBoundary : public KernelBase {
  FillInBoundary(Grid<Vec3> &grid, int g) : KernelBase(&grid, 0), grid(grid), g(g)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &grid, int g) const
  {
    if (i == 0)
      grid(i, j, k) = grid(i + 1, j, k);
    if (j == 0)
      grid(i, j, k) = grid(i, j + 1, k);
    if (k == 0)
      grid(i, j, k) = grid(i, j, k + 1);
    if (i == grid.getSizeX() - 1)
      grid(i, j, k) = grid(i - 1, j, k);
    if (j == grid.getSizeY() - 1)
      grid(i, j, k) = grid(i, j - 1, k);
    if (k == grid.getSizeZ() - 1)
      grid(i, j, k) = grid(i, j, k - 1);
  }
  inline Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline int &getArg1()
  {
    return g;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel FillInBoundary ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, grid, g);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, g);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Vec3> &grid;
  int g;
};

// ****************************************************************************

// helper functions for converting mex data to manta grids and back (for matlab integration)

// MAC grids

struct kn_conv_mex_in_to_MAC : public KernelBase {
  kn_conv_mex_in_to_MAC(const double *p_lin_array, MACGrid *p_result)
      : KernelBase(p_result, 0), p_lin_array(p_lin_array), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const double *p_lin_array, MACGrid *p_result) const
  {
    int ijk = i + j * p_result->getSizeX() + k * p_result->getSizeX() * p_result->getSizeY();
    const int n = p_result->getSizeX() * p_result->getSizeY() * p_result->getSizeZ();

    p_result->get(i, j, k).x = p_lin_array[ijk];
    p_result->get(i, j, k).y = p_lin_array[ijk + n];
    p_result->get(i, j, k).z = p_lin_array[ijk + 2 * n];
  }
  inline const double *getArg0()
  {
    return p_lin_array;
  }
  typedef double type0;
  inline MACGrid *getArg1()
  {
    return p_result;
  }
  typedef MACGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_mex_in_to_MAC ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_lin_array, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_lin_array, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const double *p_lin_array;
  MACGrid *p_result;
};

struct kn_conv_MAC_to_mex_out : public KernelBase {
  kn_conv_MAC_to_mex_out(const MACGrid *p_mac, double *p_result)
      : KernelBase(p_mac, 0), p_mac(p_mac), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const MACGrid *p_mac, double *p_result) const
  {
    int ijk = i + j * p_mac->getSizeX() + k * p_mac->getSizeX() * p_mac->getSizeY();
    const int n = p_mac->getSizeX() * p_mac->getSizeY() * p_mac->getSizeZ();

    p_result[ijk] = p_mac->get(i, j, k).x;
    p_result[ijk + n] = p_mac->get(i, j, k).y;
    p_result[ijk + 2 * n] = p_mac->get(i, j, k).z;
  }
  inline const MACGrid *getArg0()
  {
    return p_mac;
  }
  typedef MACGrid type0;
  inline double *getArg1()
  {
    return p_result;
  }
  typedef double type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_MAC_to_mex_out ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_mac, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_mac, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const MACGrid *p_mac;
  double *p_result;
};

// Vec3 Grids

struct kn_conv_mex_in_to_Vec3 : public KernelBase {
  kn_conv_mex_in_to_Vec3(const double *p_lin_array, Grid<Vec3> *p_result)
      : KernelBase(p_result, 0), p_lin_array(p_lin_array), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const double *p_lin_array, Grid<Vec3> *p_result) const
  {
    int ijk = i + j * p_result->getSizeX() + k * p_result->getSizeX() * p_result->getSizeY();
    const int n = p_result->getSizeX() * p_result->getSizeY() * p_result->getSizeZ();

    p_result->get(i, j, k).x = p_lin_array[ijk];
    p_result->get(i, j, k).y = p_lin_array[ijk + n];
    p_result->get(i, j, k).z = p_lin_array[ijk + 2 * n];
  }
  inline const double *getArg0()
  {
    return p_lin_array;
  }
  typedef double type0;
  inline Grid<Vec3> *getArg1()
  {
    return p_result;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_mex_in_to_Vec3 ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_lin_array, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_lin_array, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const double *p_lin_array;
  Grid<Vec3> *p_result;
};

struct kn_conv_Vec3_to_mex_out : public KernelBase {
  kn_conv_Vec3_to_mex_out(const Grid<Vec3> *p_Vec3, double *p_result)
      : KernelBase(p_Vec3, 0), p_Vec3(p_Vec3), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<Vec3> *p_Vec3, double *p_result) const
  {
    int ijk = i + j * p_Vec3->getSizeX() + k * p_Vec3->getSizeX() * p_Vec3->getSizeY();
    const int n = p_Vec3->getSizeX() * p_Vec3->getSizeY() * p_Vec3->getSizeZ();

    p_result[ijk] = p_Vec3->get(i, j, k).x;
    p_result[ijk + n] = p_Vec3->get(i, j, k).y;
    p_result[ijk + 2 * n] = p_Vec3->get(i, j, k).z;
  }
  inline const Grid<Vec3> *getArg0()
  {
    return p_Vec3;
  }
  typedef Grid<Vec3> type0;
  inline double *getArg1()
  {
    return p_result;
  }
  typedef double type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_Vec3_to_mex_out ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_Vec3, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_Vec3, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const Grid<Vec3> *p_Vec3;
  double *p_result;
};

// Real Grids

struct kn_conv_mex_in_to_Real : public KernelBase {
  kn_conv_mex_in_to_Real(const double *p_lin_array, Grid<Real> *p_result)
      : KernelBase(p_result, 0), p_lin_array(p_lin_array), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const double *p_lin_array, Grid<Real> *p_result) const
  {
    int ijk = i + j * p_result->getSizeX() + k * p_result->getSizeX() * p_result->getSizeY();

    p_result->get(i, j, k) = p_lin_array[ijk];
  }
  inline const double *getArg0()
  {
    return p_lin_array;
  }
  typedef double type0;
  inline Grid<Real> *getArg1()
  {
    return p_result;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_mex_in_to_Real ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_lin_array, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_lin_array, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const double *p_lin_array;
  Grid<Real> *p_result;
};

struct kn_conv_Real_to_mex_out : public KernelBase {
  kn_conv_Real_to_mex_out(const Grid<Real> *p_grid, double *p_result)
      : KernelBase(p_grid, 0), p_grid(p_grid), p_result(p_result)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<Real> *p_grid, double *p_result) const
  {
    int ijk = i + j * p_grid->getSizeX() + k * p_grid->getSizeX() * p_grid->getSizeY();

    p_result[ijk] = p_grid->get(i, j, k);
  }
  inline const Grid<Real> *getArg0()
  {
    return p_grid;
  }
  typedef Grid<Real> type0;
  inline double *getArg1()
  {
    return p_result;
  }
  typedef double type1;
  void runMessage()
  {
    debMsg("Executing kernel kn_conv_Real_to_mex_out ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, p_grid, p_result);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, p_grid, p_result);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const Grid<Real> *p_grid;
  double *p_result;
};

}  // namespace Manta
#endif
