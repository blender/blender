

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
 * Plugins for pressure correction: solve_pressure, and ghost fluid helpers
 *
 ******************************************************************************/
#include "vectorbase.h"
#include "kernel.h"
#include "conjugategrad.h"
#include "multigrid.h"

using namespace std;
namespace Manta {

//! Preconditioner for CG solver
// - None: Use standard CG
// - MIC: Modified incomplete Cholesky preconditioner
// - MGDynamic: Multigrid preconditioner, rebuilt for each solve
// - MGStatic: Multigrid preconditioner, built only once (faster than
//       MGDynamic, but works only if Poisson equation does not change)
enum Preconditioner { PcNone = 0, PcMIC = 1, PcMGDynamic = 2, PcMGStatic = 3 };

inline static Real surfTensHelper(const IndexInt idx,
                                  const int offset,
                                  const Grid<Real> &phi,
                                  const Grid<Real> &curv,
                                  const Real surfTens,
                                  const Real gfClamp);

//! Kernel: Construct the right-hand side of the poisson equation

struct MakeRhs : public KernelBase {
  MakeRhs(const FlagGrid &flags,
          Grid<Real> &rhs,
          const MACGrid &vel,
          const Grid<Real> *perCellCorr,
          const MACGrid *fractions,
          const MACGrid *obvel,
          const Grid<Real> *phi,
          const Grid<Real> *curv,
          const Real surfTens,
          const Real gfClamp)
      : KernelBase(&flags, 1),
        flags(flags),
        rhs(rhs),
        vel(vel),
        perCellCorr(perCellCorr),
        fractions(fractions),
        obvel(obvel),
        phi(phi),
        curv(curv),
        surfTens(surfTens),
        gfClamp(gfClamp),
        cnt(0),
        sum(0)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &rhs,
                 const MACGrid &vel,
                 const Grid<Real> *perCellCorr,
                 const MACGrid *fractions,
                 const MACGrid *obvel,
                 const Grid<Real> *phi,
                 const Grid<Real> *curv,
                 const Real surfTens,
                 const Real gfClamp,
                 int &cnt,
                 double &sum)
  {
    if (!flags.isFluid(i, j, k)) {
      rhs(i, j, k) = 0;
      return;
    }

    // compute negative divergence
    // no flag checks: assumes vel at obstacle interfaces is set to zero
    Real set(0);
    if (!fractions) {
      set = vel(i, j, k).x - vel(i + 1, j, k).x + vel(i, j, k).y - vel(i, j + 1, k).y;
      if (vel.is3D())
        set += vel(i, j, k).z - vel(i, j, k + 1).z;
    }
    else {
      set = (*fractions)(i, j, k).x * vel(i, j, k).x -
            (*fractions)(i + 1, j, k).x * vel(i + 1, j, k).x +
            (*fractions)(i, j, k).y * vel(i, j, k).y -
            (*fractions)(i, j + 1, k).y * vel(i, j + 1, k).y;
      if (vel.is3D())
        set += (*fractions)(i, j, k).z * vel(i, j, k).z -
               (*fractions)(i, j, k + 1).z * vel(i, j, k + 1).z;

      // compute divergence from obstacle by using obstacle velocity (optional)
      if (obvel) {
        set += (1 - (*fractions)(i, j, k).x) * (*obvel)(i, j, k).x -
               (1 - (*fractions)(i + 1, j, k).x) * (*obvel)(i + 1, j, k).x +
               (1 - (*fractions)(i, j, k).y) * (*obvel)(i, j, k).y -
               (1 - (*fractions)(i, j + 1, k).y) * (*obvel)(i, j + 1, k).y;
        if (obvel->is3D())
          set += (1 - (*fractions)(i, j, k).z) * (*obvel)(i, j, k).z -
                 (1 - (*fractions)(i, j, k + 1).z) * (*obvel)(i, j, k + 1).z;
      }
    }

    // compute surface tension effect (optional)
    if (phi && curv) {
      const IndexInt idx = flags.index(i, j, k);
      const int X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
      if (flags.isEmpty(i - 1, j, k))
        set += surfTensHelper(idx, -X, *phi, *curv, surfTens, gfClamp);
      if (flags.isEmpty(i + 1, j, k))
        set += surfTensHelper(idx, +X, *phi, *curv, surfTens, gfClamp);
      if (flags.isEmpty(i, j - 1, k))
        set += surfTensHelper(idx, -Y, *phi, *curv, surfTens, gfClamp);
      if (flags.isEmpty(i, j + 1, k))
        set += surfTensHelper(idx, +Y, *phi, *curv, surfTens, gfClamp);
      if (vel.is3D()) {
        if (flags.isEmpty(i, j, k - 1))
          set += surfTensHelper(idx, -Z, *phi, *curv, surfTens, gfClamp);
        if (flags.isEmpty(i, j, k + 1))
          set += surfTensHelper(idx, +Z, *phi, *curv, surfTens, gfClamp);
      }
    }

    // per cell divergence correction (optional)
    if (perCellCorr)
      set += perCellCorr->get(i, j, k);

    // obtain sum, cell count
    sum += set;
    cnt++;

    rhs(i, j, k) = set;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return rhs;
  }
  typedef Grid<Real> type1;
  inline const MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline const Grid<Real> *getArg3()
  {
    return perCellCorr;
  }
  typedef Grid<Real> type3;
  inline const MACGrid *getArg4()
  {
    return fractions;
  }
  typedef MACGrid type4;
  inline const MACGrid *getArg5()
  {
    return obvel;
  }
  typedef MACGrid type5;
  inline const Grid<Real> *getArg6()
  {
    return phi;
  }
  typedef Grid<Real> type6;
  inline const Grid<Real> *getArg7()
  {
    return curv;
  }
  typedef Grid<Real> type7;
  inline const Real &getArg8()
  {
    return surfTens;
  }
  typedef Real type8;
  inline const Real &getArg9()
  {
    return gfClamp;
  }
  typedef Real type9;
  void runMessage()
  {
    debMsg("Executing kernel MakeRhs ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    if (maxZ > 1) {
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 1; j < _maxY; j++)
          for (int i = 1; i < _maxX; i++)
            op(i,
               j,
               k,
               flags,
               rhs,
               vel,
               perCellCorr,
               fractions,
               obvel,
               phi,
               curv,
               surfTens,
               gfClamp,
               cnt,
               sum);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i,
             j,
             k,
             flags,
             rhs,
             vel,
             perCellCorr,
             fractions,
             obvel,
             phi,
             curv,
             surfTens,
             gfClamp,
             cnt,
             sum);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  MakeRhs(MakeRhs &o, tbb::split)
      : KernelBase(o),
        flags(o.flags),
        rhs(o.rhs),
        vel(o.vel),
        perCellCorr(o.perCellCorr),
        fractions(o.fractions),
        obvel(o.obvel),
        phi(o.phi),
        curv(o.curv),
        surfTens(o.surfTens),
        gfClamp(o.gfClamp),
        cnt(0),
        sum(0)
  {
  }
  void join(const MakeRhs &o)
  {
    cnt += o.cnt;
    sum += o.sum;
  }
  const FlagGrid &flags;
  Grid<Real> &rhs;
  const MACGrid &vel;
  const Grid<Real> *perCellCorr;
  const MACGrid *fractions;
  const MACGrid *obvel;
  const Grid<Real> *phi;
  const Grid<Real> *curv;
  const Real surfTens;
  const Real gfClamp;
  int cnt;
  double sum;
};

//! Kernel: make velocity divergence free by subtracting pressure gradient

struct knCorrectVelocity : public KernelBase {
  knCorrectVelocity(const FlagGrid &flags, MACGrid &vel, const Grid<Real> &pressure)
      : KernelBase(&flags, 1), flags(flags), vel(vel), pressure(pressure)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const FlagGrid &flags, MACGrid &vel, const Grid<Real> &pressure) const
  {
    const IndexInt idx = flags.index(i, j, k);
    if (flags.isFluid(idx)) {
      if (flags.isFluid(i - 1, j, k))
        vel[idx].x -= (pressure[idx] - pressure(i - 1, j, k));
      if (flags.isFluid(i, j - 1, k))
        vel[idx].y -= (pressure[idx] - pressure(i, j - 1, k));
      if (flags.is3D() && flags.isFluid(i, j, k - 1))
        vel[idx].z -= (pressure[idx] - pressure(i, j, k - 1));

      if (flags.isEmpty(i - 1, j, k))
        vel[idx].x -= pressure[idx];
      if (flags.isEmpty(i, j - 1, k))
        vel[idx].y -= pressure[idx];
      if (flags.is3D() && flags.isEmpty(i, j, k - 1))
        vel[idx].z -= pressure[idx];
    }
    else if (flags.isEmpty(idx) &&
             !flags.isOutflow(idx)) {  // don't change velocities in outflow cells
      if (flags.isFluid(i - 1, j, k))
        vel[idx].x += pressure(i - 1, j, k);
      else
        vel[idx].x = 0.f;
      if (flags.isFluid(i, j - 1, k))
        vel[idx].y += pressure(i, j - 1, k);
      else
        vel[idx].y = 0.f;
      if (flags.is3D()) {
        if (flags.isFluid(i, j, k - 1))
          vel[idx].z += pressure(i, j, k - 1);
        else
          vel[idx].z = 0.f;
      }
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return pressure;
  }
  typedef Grid<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel knCorrectVelocity ", 3);
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
            op(i, j, k, flags, vel, pressure);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, vel, pressure);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const FlagGrid &flags;
  MACGrid &vel;
  const Grid<Real> &pressure;
};

// *****************************************************************************
// Ghost fluid helpers

// calculate fraction filled with liquid (note, assumes inside value is < outside!)
inline static Real thetaHelper(const Real inside, const Real outside)
{
  const Real denom = inside - outside;
  if (denom > -1e-04)
    return 0.5;  // should always be neg, and large enough...
  return std::max(Real(0), std::min(Real(1), inside / denom));
}

// calculate ghost fluid factor, cell at idx should be a fluid cell
inline static Real ghostFluidHelper(const IndexInt idx,
                                    const int offset,
                                    const Grid<Real> &phi,
                                    const Real gfClamp)
{
  Real alpha = thetaHelper(phi[idx], phi[idx + offset]);
  if (alpha < gfClamp)
    return alpha = gfClamp;
  return (1. - (1. / alpha));
}

inline static Real surfTensHelper(const IndexInt idx,
                                  const int offset,
                                  const Grid<Real> &phi,
                                  const Grid<Real> &curv,
                                  const Real surfTens,
                                  const Real gfClamp)
{
  return surfTens * (curv[idx + offset] - ghostFluidHelper(idx, offset, phi, gfClamp) * curv[idx]);
}

//! Kernel: Adapt A0 for ghost fluid

struct ApplyGhostFluidDiagonal : public KernelBase {
  ApplyGhostFluidDiagonal(Grid<Real> &A0,
                          const FlagGrid &flags,
                          const Grid<Real> &phi,
                          const Real gfClamp)
      : KernelBase(&A0, 1), A0(A0), flags(flags), phi(phi), gfClamp(gfClamp)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &A0,
                 const FlagGrid &flags,
                 const Grid<Real> &phi,
                 const Real gfClamp) const
  {
    const int X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
    const IndexInt idx = flags.index(i, j, k);
    if (!flags.isFluid(idx))
      return;

    if (flags.isEmpty(i - 1, j, k))
      A0[idx] -= ghostFluidHelper(idx, -X, phi, gfClamp);
    if (flags.isEmpty(i + 1, j, k))
      A0[idx] -= ghostFluidHelper(idx, +X, phi, gfClamp);
    if (flags.isEmpty(i, j - 1, k))
      A0[idx] -= ghostFluidHelper(idx, -Y, phi, gfClamp);
    if (flags.isEmpty(i, j + 1, k))
      A0[idx] -= ghostFluidHelper(idx, +Y, phi, gfClamp);
    if (flags.is3D()) {
      if (flags.isEmpty(i, j, k - 1))
        A0[idx] -= ghostFluidHelper(idx, -Z, phi, gfClamp);
      if (flags.isEmpty(i, j, k + 1))
        A0[idx] -= ghostFluidHelper(idx, +Z, phi, gfClamp);
    }
  }
  inline Grid<Real> &getArg0()
  {
    return A0;
  }
  typedef Grid<Real> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return phi;
  }
  typedef Grid<Real> type2;
  inline const Real &getArg3()
  {
    return gfClamp;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel ApplyGhostFluidDiagonal ", 3);
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
            op(i, j, k, A0, flags, phi, gfClamp);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, A0, flags, phi, gfClamp);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &A0;
  const FlagGrid &flags;
  const Grid<Real> &phi;
  const Real gfClamp;
};

//! Kernel: Apply velocity update: ghost fluid contribution

struct knCorrectVelocityGhostFluid : public KernelBase {
  knCorrectVelocityGhostFluid(MACGrid &vel,
                              const FlagGrid &flags,
                              const Grid<Real> &pressure,
                              const Grid<Real> &phi,
                              Real gfClamp,
                              const Grid<Real> *curv,
                              const Real surfTens)
      : KernelBase(&vel, 1),
        vel(vel),
        flags(flags),
        pressure(pressure),
        phi(phi),
        gfClamp(gfClamp),
        curv(curv),
        surfTens(surfTens)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &vel,
                 const FlagGrid &flags,
                 const Grid<Real> &pressure,
                 const Grid<Real> &phi,
                 Real gfClamp,
                 const Grid<Real> *curv,
                 const Real surfTens) const
  {
    const IndexInt X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
    const IndexInt idx = flags.index(i, j, k);
    if (flags.isFluid(idx)) {
      if (flags.isEmpty(i - 1, j, k))
        vel[idx][0] += pressure[idx] * ghostFluidHelper(idx, -X, phi, gfClamp);
      if (flags.isEmpty(i, j - 1, k))
        vel[idx][1] += pressure[idx] * ghostFluidHelper(idx, -Y, phi, gfClamp);
      if (flags.is3D() && flags.isEmpty(i, j, k - 1))
        vel[idx][2] += pressure[idx] * ghostFluidHelper(idx, -Z, phi, gfClamp);
    }
    else if (flags.isEmpty(idx) &&
             !flags.isOutflow(idx)) {  // do not change velocities in outflow cells
      if (flags.isFluid(i - 1, j, k))
        vel[idx][0] -= pressure(i - 1, j, k) * ghostFluidHelper(idx - X, +X, phi, gfClamp);
      else
        vel[idx].x = 0.f;
      if (flags.isFluid(i, j - 1, k))
        vel[idx][1] -= pressure(i, j - 1, k) * ghostFluidHelper(idx - Y, +Y, phi, gfClamp);
      else
        vel[idx].y = 0.f;
      if (flags.is3D()) {
        if (flags.isFluid(i, j, k - 1))
          vel[idx][2] -= pressure(i, j, k - 1) * ghostFluidHelper(idx - Z, +Z, phi, gfClamp);
        else
          vel[idx].z = 0.f;
      }
    }

    if (curv) {
      if (flags.isFluid(idx)) {
        if (flags.isEmpty(i - 1, j, k))
          vel[idx].x += surfTensHelper(idx, -X, phi, *curv, surfTens, gfClamp);
        if (flags.isEmpty(i, j - 1, k))
          vel[idx].y += surfTensHelper(idx, -Y, phi, *curv, surfTens, gfClamp);
        if (flags.is3D() && flags.isEmpty(i, j, k - 1))
          vel[idx].z += surfTensHelper(idx, -Z, phi, *curv, surfTens, gfClamp);
      }
      else if (flags.isEmpty(idx) &&
               !flags.isOutflow(idx)) {  // do not change velocities in outflow cells
        vel[idx].x -= (flags.isFluid(i - 1, j, k)) ?
                          surfTensHelper(idx - X, +X, phi, *curv, surfTens, gfClamp) :
                          0.f;
        vel[idx].y -= (flags.isFluid(i, j - 1, k)) ?
                          surfTensHelper(idx - Y, +Y, phi, *curv, surfTens, gfClamp) :
                          0.f;
        if (flags.is3D())
          vel[idx].z -= (flags.isFluid(i, j, k - 1)) ?
                            surfTensHelper(idx - Z, +Z, phi, *curv, surfTens, gfClamp) :
                            0.f;
      }
    }
  }
  inline MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return pressure;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> &getArg3()
  {
    return phi;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return gfClamp;
  }
  typedef Real type4;
  inline const Grid<Real> *getArg5()
  {
    return curv;
  }
  typedef Grid<Real> type5;
  inline const Real &getArg6()
  {
    return surfTens;
  }
  typedef Real type6;
  void runMessage()
  {
    debMsg("Executing kernel knCorrectVelocityGhostFluid ", 3);
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
            op(i, j, k, vel, flags, pressure, phi, gfClamp, curv, surfTens);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, vel, flags, pressure, phi, gfClamp, curv, surfTens);
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
  const FlagGrid &flags;
  const Grid<Real> &pressure;
  const Grid<Real> &phi;
  Real gfClamp;
  const Grid<Real> *curv;
  const Real surfTens;
};

// improve behavior of clamping for large time steps:
inline static Real ghostFluidWasClamped(const IndexInt idx,
                                        const int offset,
                                        const Grid<Real> &phi,
                                        const Real gfClamp)
{
  const Real alpha = thetaHelper(phi[idx], phi[idx + offset]);
  if (alpha < gfClamp)
    return true;
  return false;
}

struct knReplaceClampedGhostFluidVels : public KernelBase {
  knReplaceClampedGhostFluidVels(MACGrid &vel,
                                 const FlagGrid &flags,
                                 const Grid<Real> &pressure,
                                 const Grid<Real> &phi,
                                 Real gfClamp)
      : KernelBase(&vel, 1), vel(vel), flags(flags), pressure(pressure), phi(phi), gfClamp(gfClamp)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &vel,
                 const FlagGrid &flags,
                 const Grid<Real> &pressure,
                 const Grid<Real> &phi,
                 Real gfClamp) const
  {
    const IndexInt idx = flags.index(i, j, k);
    const IndexInt X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
    if (!flags.isEmpty(idx))
      return;

    if (flags.isFluid(i - 1, j, k) && ghostFluidWasClamped(idx - X, +X, phi, gfClamp))
      vel[idx][0] = vel[idx - X][0];
    if (flags.isFluid(i, j - 1, k) && ghostFluidWasClamped(idx - Y, +Y, phi, gfClamp))
      vel[idx][1] = vel[idx - Y][1];
    if (flags.is3D() && flags.isFluid(i, j, k - 1) &&
        ghostFluidWasClamped(idx - Z, +Z, phi, gfClamp))
      vel[idx][2] = vel[idx - Z][2];

    if (flags.isFluid(i + 1, j, k) && ghostFluidWasClamped(idx + X, -X, phi, gfClamp))
      vel[idx][0] = vel[idx + X][0];
    if (flags.isFluid(i, j + 1, k) && ghostFluidWasClamped(idx + Y, -Y, phi, gfClamp))
      vel[idx][1] = vel[idx + Y][1];
    if (flags.is3D() && flags.isFluid(i, j, k + 1) &&
        ghostFluidWasClamped(idx + Z, -Z, phi, gfClamp))
      vel[idx][2] = vel[idx + Z][2];
  }
  inline MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return pressure;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> &getArg3()
  {
    return phi;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return gfClamp;
  }
  typedef Real type4;
  void runMessage()
  {
    debMsg("Executing kernel knReplaceClampedGhostFluidVels ", 3);
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
            op(i, j, k, vel, flags, pressure, phi, gfClamp);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, vel, flags, pressure, phi, gfClamp);
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
  const FlagGrid &flags;
  const Grid<Real> &pressure;
  const Grid<Real> &phi;
  Real gfClamp;
};

//! Kernel: Compute min value of Real grid

struct CountEmptyCells : public KernelBase {
  CountEmptyCells(const FlagGrid &flags) : KernelBase(&flags, 0), flags(flags), numEmpty(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const FlagGrid &flags, int &numEmpty)
  {
    if (flags.isEmpty(idx))
      numEmpty++;
  }
  inline operator int()
  {
    return numEmpty;
  }
  inline int &getRet()
  {
    return numEmpty;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  void runMessage()
  {
    debMsg("Executing kernel CountEmptyCells ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, numEmpty);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CountEmptyCells(CountEmptyCells &o, tbb::split) : KernelBase(o), flags(o.flags), numEmpty(0)
  {
  }
  void join(const CountEmptyCells &o)
  {
    numEmpty += o.numEmpty;
  }
  const FlagGrid &flags;
  int numEmpty;
};

// *****************************************************************************
// Misc helpers

//! Change 'A' and 'rhs' such that pressure at 'fixPidx' is fixed to 'value'
void fixPressure(int fixPidx,
                 Real value,
                 Grid<Real> &rhs,
                 Grid<Real> &A0,
                 Grid<Real> &Ai,
                 Grid<Real> &Aj,
                 Grid<Real> &Ak)
{
  // Bring to rhs at neighbors
  rhs[fixPidx + Ai.getStrideX()] -= Ai[fixPidx] * value;
  rhs[fixPidx + Aj.getStrideY()] -= Aj[fixPidx] * value;
  rhs[fixPidx - Ai.getStrideX()] -= Ai[fixPidx - Ai.getStrideX()] * value;
  rhs[fixPidx - Aj.getStrideY()] -= Aj[fixPidx - Aj.getStrideY()] * value;
  if (rhs.is3D()) {
    rhs[fixPidx + Ak.getStrideZ()] -= Ak[fixPidx] * value;
    rhs[fixPidx - Ak.getStrideZ()] -= Ak[fixPidx - Ak.getStrideZ()] * value;
  }

  // Trivialize equation at 'fixPidx' to: pressure[fixPidx] = value
  rhs[fixPidx] = value;
  A0[fixPidx] = Real(1);
  Ai[fixPidx] = Aj[fixPidx] = Ak[fixPidx] = Real(0);
  Ai[fixPidx - Ai.getStrideX()] = Real(0);
  Aj[fixPidx - Aj.getStrideY()] = Real(0);
  if (rhs.is3D()) {
    Ak[fixPidx - Ak.getStrideZ()] = Real(0);
  }
}

// for "static" MG mode, keep one MG data structure per fluid solver
// leave cleanup to OS/user if nonzero at program termination (PcMGStatic mode)
// alternatively, manually release in scene file with releaseMG
static std::map<FluidSolver *, GridMg *> gMapMG;

void releaseMG(FluidSolver *solver = nullptr)
{
  // release all?
  if (!solver) {
    for (std::map<FluidSolver *, GridMg *>::iterator it = gMapMG.begin(); it != gMapMG.end();
         it++) {
      if (it->first != nullptr)
        releaseMG(it->first);
    }
    return;
  }

  GridMg *mg = gMapMG[solver];
  if (mg) {
    delete mg;
    gMapMG[solver] = nullptr;
  }
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "releaseMG", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      FluidSolver *solver = _args.getPtrOpt<FluidSolver>("solver", 0, nullptr, &_lock);
      _retval = getPyNone();
      releaseMG(solver);
      _args.check();
    }
    pbFinalizePlugin(parent, "releaseMG", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("releaseMG", e.what());
    return 0;
  }
}
static const Pb::Register _RP_releaseMG("", "releaseMG", _W_0);
extern "C" {
void PbRegister_releaseMG()
{
  KEEP_UNUSED(_RP_releaseMG);
}
}

// *****************************************************************************
// Main pressure solve

// Note , all three pressure solve helper functions take
// identical parameters, apart from the RHS grid (and different const values)

//! Compute rhs for pressure solve

void computePressureRhs(Grid<Real> &rhs,
                        const MACGrid &vel,
                        const Grid<Real> &pressure,
                        const FlagGrid &flags,
                        Real cgAccuracy = 1e-3,
                        const Grid<Real> *phi = nullptr,
                        const Grid<Real> *perCellCorr = nullptr,
                        const MACGrid *fractions = nullptr,
                        const MACGrid *obvel = nullptr,
                        Real gfClamp = 1e-04,
                        Real cgMaxIterFac = 1.5,
                        bool precondition = true,
                        int preconditioner = PcMIC,
                        bool enforceCompatibility = false,
                        bool useL2Norm = false,
                        bool zeroPressureFixing = false,
                        const Grid<Real> *curv = nullptr,
                        const Real surfTens = 0.)
{
  // compute divergence and init right hand side
  MakeRhs kernMakeRhs(
      flags, rhs, vel, perCellCorr, fractions, obvel, phi, curv, surfTens, gfClamp);

  if (enforceCompatibility)
    rhs += (Real)(-kernMakeRhs.sum / (Real)kernMakeRhs.cnt);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "computePressureRhs", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &rhs = *_args.getPtr<Grid<Real>>("rhs", 0, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Grid<Real> &pressure = *_args.getPtr<Grid<Real>>("pressure", 2, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 3, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 4, 1e-3, &_lock);
      const Grid<Real> *phi = _args.getPtrOpt<Grid<Real>>("phi", 5, nullptr, &_lock);
      const Grid<Real> *perCellCorr = _args.getPtrOpt<Grid<Real>>(
          "perCellCorr", 6, nullptr, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 7, nullptr, &_lock);
      const MACGrid *obvel = _args.getPtrOpt<MACGrid>("obvel", 8, nullptr, &_lock);
      Real gfClamp = _args.getOpt<Real>("gfClamp", 9, 1e-04, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 10, 1.5, &_lock);
      bool precondition = _args.getOpt<bool>("precondition", 11, true, &_lock);
      int preconditioner = _args.getOpt<int>("preconditioner", 12, PcMIC, &_lock);
      bool enforceCompatibility = _args.getOpt<bool>("enforceCompatibility", 13, false, &_lock);
      bool useL2Norm = _args.getOpt<bool>("useL2Norm", 14, false, &_lock);
      bool zeroPressureFixing = _args.getOpt<bool>("zeroPressureFixing", 15, false, &_lock);
      const Grid<Real> *curv = _args.getPtrOpt<Grid<Real>>("curv", 16, nullptr, &_lock);
      const Real surfTens = _args.getOpt<Real>("surfTens", 17, 0., &_lock);
      _retval = getPyNone();
      computePressureRhs(rhs,
                         vel,
                         pressure,
                         flags,
                         cgAccuracy,
                         phi,
                         perCellCorr,
                         fractions,
                         obvel,
                         gfClamp,
                         cgMaxIterFac,
                         precondition,
                         preconditioner,
                         enforceCompatibility,
                         useL2Norm,
                         zeroPressureFixing,
                         curv,
                         surfTens);
      _args.check();
    }
    pbFinalizePlugin(parent, "computePressureRhs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("computePressureRhs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_computePressureRhs("", "computePressureRhs", _W_1);
extern "C" {
void PbRegister_computePressureRhs()
{
  KEEP_UNUSED(_RP_computePressureRhs);
}
}

//! Build and solve pressure system of equations
//! perCellCorr: a divergence correction for each cell, optional
//! fractions: for 2nd order obstacle boundaries, optional
//! gfClamp: clamping threshold for ghost fluid method
//! cgMaxIterFac: heuristic to determine maximal number of CG iteations, increase for more accurate
//! solutions preconditioner: MIC, or MG (see Preconditioner enum) useL2Norm: use max norm by
//! default, can be turned to L2 here zeroPressureFixing: remove null space by fixing a single
//! pressure value, needed for MG curv: curvature for surface tension effects surfTens: surface
//! tension coefficient retRhs: return RHS divergence, e.g., for debugging; optional

void solvePressureSystem(Grid<Real> &rhs,
                         MACGrid &vel,
                         Grid<Real> &pressure,
                         const FlagGrid &flags,
                         Real cgAccuracy = 1e-3,
                         const Grid<Real> *phi = nullptr,
                         const Grid<Real> *perCellCorr = nullptr,
                         const MACGrid *fractions = nullptr,
                         Real gfClamp = 1e-04,
                         Real cgMaxIterFac = 1.5,
                         bool precondition = true,
                         int preconditioner = PcMIC,
                         const bool enforceCompatibility = false,
                         const bool useL2Norm = false,
                         const bool zeroPressureFixing = false,
                         const Grid<Real> *curv = nullptr,
                         const Real surfTens = 0.)
{
  if (precondition == false)
    preconditioner = PcNone;  // for backwards compatibility

  // reserve temp grids
  FluidSolver *parent = flags.getParent();
  Grid<Real> residual(parent);
  Grid<Real> search(parent);
  Grid<Real> A0(parent);
  Grid<Real> Ai(parent);
  Grid<Real> Aj(parent);
  Grid<Real> Ak(parent);
  Grid<Real> tmp(parent);

  // setup matrix and boundaries
  MakeLaplaceMatrix(flags, A0, Ai, Aj, Ak, fractions);

  if (phi) {
    ApplyGhostFluidDiagonal(A0, flags, *phi, gfClamp);
  }

  // check whether we need to fix some pressure value...
  // (manually enable, or automatically for high accuracy, can cause asymmetries otherwise)
  if (zeroPressureFixing || cgAccuracy < 1e-07) {
    if (FLOATINGPOINT_PRECISION == 1)
      debMsg(
          "Warning - high CG accuracy with single-precision floating point accuracy might not "
          "converge...",
          2);

    int numEmpty = CountEmptyCells(flags);
    IndexInt fixPidx = -1;
    if (numEmpty == 0) {
      // Determine appropriate fluid cell for pressure fixing
      // 1) First check some preferred positions for approx. symmetric zeroPressureFixing
      Vec3i topCenter(
          flags.getSizeX() / 2, flags.getSizeY() - 1, flags.is3D() ? flags.getSizeZ() / 2 : 0);
      Vec3i preferredPos[] = {topCenter, topCenter - Vec3i(0, 1, 0), topCenter - Vec3i(0, 2, 0)};

      for (Vec3i pos : preferredPos) {
        if (flags.isFluid(pos)) {
          fixPidx = flags.index(pos);
          break;
        }
      }

      // 2) Then search whole domain
      if (fixPidx == -1) {
        FOR_IJK_BND(flags, 1)
        {
          if (flags.isFluid(i, j, k)) {
            fixPidx = flags.index(i, j, k);
            // break FOR_IJK_BND loop
            i = flags.getSizeX() - 1;
            j = flags.getSizeY() - 1;
            k = __kmax;
          }
        }
      }
      // debMsg("No empty cells! Fixing pressure of cell "<<fixPidx<<" to zero",1);
    }
    if (fixPidx >= 0) {
      fixPressure(fixPidx, Real(0), rhs, A0, Ai, Aj, Ak);
      static bool msgOnce = false;
      if (!msgOnce) {
        debMsg("Pinning pressure of cell " << fixPidx << " to zero", 2);
        msgOnce = true;
      }
    }
  }

  // CG setup
  // note: the last factor increases the max iterations for 2d, which right now can't use a
  // preconditioner
  GridCgInterface *gcg;
  vector<Grid<Real> *> matA{&A0, &Ai, &Aj};

  if (vel.is3D()) {
    matA.push_back(&Ak);
    gcg = new GridCg<ApplyMatrix>(pressure, rhs, residual, search, flags, tmp, matA);
  }
  else {
    gcg = new GridCg<ApplyMatrix2D>(pressure, rhs, residual, search, flags, tmp, matA);
  }

  gcg->setAccuracy(cgAccuracy);
  gcg->setUseL2Norm(useL2Norm);

  int maxIter = (int)(cgMaxIterFac * flags.getSize().max()) * (flags.is3D() ? 1 : 4);

  Grid<Real> *pca0 = nullptr, *pca1 = nullptr, *pca2 = nullptr, *pca3 = nullptr;
  GridMg *pmg = nullptr;

  // optional preconditioning
  if (preconditioner == PcMIC) {
    pca0 = new Grid<Real>(parent);
    pca1 = new Grid<Real>(parent);
    pca2 = new Grid<Real>(parent);
    pca3 = new Grid<Real>(parent);
    gcg->setICPreconditioner(GridCgInterface::PC_mICP, pca0, pca1, pca2, pca3);
  }
  else if (preconditioner == PcMGDynamic || preconditioner == PcMGStatic) {
    maxIter = 100;

    pmg = gMapMG[parent];
    // Release MG from previous step if present (e.g. if previous solve was with MGStatic)
    if (pmg && preconditioner == PcMGDynamic) {
      releaseMG(parent);
      pmg = nullptr;
    }
    if (!pmg) {
      pmg = new GridMg(pressure.getSize());
      gMapMG[parent] = pmg;
    }

    gcg->setMGPreconditioner(GridCgInterface::PC_MGP, pmg);
  }

  auto cleanup = [&](){
    // Cleanup
    if (gcg)
      delete gcg;
    if (pca0)
      delete pca0;
    if (pca1)
      delete pca1;
    if (pca2)
      delete pca2;
    if (pca3)
      delete pca3;

    // PcMGDynamic: always delete multigrid solver after use
    // PcMGStatic: keep multigrid solver for next solve
    if (pmg && preconditioner == PcMGDynamic)
      releaseMG(parent);
  };

  // CG solve
  try {
  for (int iter = 0; iter < maxIter; iter++) {
    if (!gcg->iterate())
      iter = maxIter;
    if (iter < maxIter)
      debMsg("FluidSolver::solvePressure iteration " << iter
                                                     << ", residual: " << gcg->getResNorm(),
             9);
  }
  debMsg("FluidSolver::solvePressure done. Iterations:" << gcg->getIterations()
                                                        << ", residual:" << gcg->getResNorm(),
         2);
  }
  catch (const Manta::Error &e) {
    cleanup();
    throw e;
  }

  cleanup();
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "solvePressureSystem", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &rhs = *_args.getPtr<Grid<Real>>("rhs", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      Grid<Real> &pressure = *_args.getPtr<Grid<Real>>("pressure", 2, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 3, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 4, 1e-3, &_lock);
      const Grid<Real> *phi = _args.getPtrOpt<Grid<Real>>("phi", 5, nullptr, &_lock);
      const Grid<Real> *perCellCorr = _args.getPtrOpt<Grid<Real>>(
          "perCellCorr", 6, nullptr, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 7, nullptr, &_lock);
      Real gfClamp = _args.getOpt<Real>("gfClamp", 8, 1e-04, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 9, 1.5, &_lock);
      bool precondition = _args.getOpt<bool>("precondition", 10, true, &_lock);
      int preconditioner = _args.getOpt<int>("preconditioner", 11, PcMIC, &_lock);
      const bool enforceCompatibility = _args.getOpt<bool>(
          "enforceCompatibility", 12, false, &_lock);
      const bool useL2Norm = _args.getOpt<bool>("useL2Norm", 13, false, &_lock);
      const bool zeroPressureFixing = _args.getOpt<bool>("zeroPressureFixing", 14, false, &_lock);
      const Grid<Real> *curv = _args.getPtrOpt<Grid<Real>>("curv", 15, nullptr, &_lock);
      const Real surfTens = _args.getOpt<Real>("surfTens", 16, 0., &_lock);
      _retval = getPyNone();
      solvePressureSystem(rhs,
                          vel,
                          pressure,
                          flags,
                          cgAccuracy,
                          phi,
                          perCellCorr,
                          fractions,
                          gfClamp,
                          cgMaxIterFac,
                          precondition,
                          preconditioner,
                          enforceCompatibility,
                          useL2Norm,
                          zeroPressureFixing,
                          curv,
                          surfTens);
      _args.check();
    }
    pbFinalizePlugin(parent, "solvePressureSystem", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("solvePressureSystem", e.what());
    return 0;
  }
}
static const Pb::Register _RP_solvePressureSystem("", "solvePressureSystem", _W_2);
extern "C" {
void PbRegister_solvePressureSystem()
{
  KEEP_UNUSED(_RP_solvePressureSystem);
}
}

//! Apply pressure gradient to make velocity field divergence free

void correctVelocity(MACGrid &vel,
                     Grid<Real> &pressure,
                     const FlagGrid &flags,
                     Real cgAccuracy = 1e-3,
                     const Grid<Real> *phi = nullptr,
                     const Grid<Real> *perCellCorr = nullptr,
                     const MACGrid *fractions = nullptr,
                     Real gfClamp = 1e-04,
                     Real cgMaxIterFac = 1.5,
                     bool precondition = true,
                     int preconditioner = PcMIC,
                     bool enforceCompatibility = false,
                     bool useL2Norm = false,
                     bool zeroPressureFixing = false,
                     const Grid<Real> *curv = nullptr,
                     const Real surfTens = 0.)
{
  knCorrectVelocity(flags, vel, pressure);
  if (phi) {
    knCorrectVelocityGhostFluid(vel, flags, pressure, *phi, gfClamp, curv, surfTens);
    // improve behavior of clamping for large time steps:
    knReplaceClampedGhostFluidVels(vel, flags, pressure, *phi, gfClamp);
  }
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "correctVelocity", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Real> &pressure = *_args.getPtr<Grid<Real>>("pressure", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 3, 1e-3, &_lock);
      const Grid<Real> *phi = _args.getPtrOpt<Grid<Real>>("phi", 4, nullptr, &_lock);
      const Grid<Real> *perCellCorr = _args.getPtrOpt<Grid<Real>>(
          "perCellCorr", 5, nullptr, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 6, nullptr, &_lock);
      Real gfClamp = _args.getOpt<Real>("gfClamp", 7, 1e-04, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 8, 1.5, &_lock);
      bool precondition = _args.getOpt<bool>("precondition", 9, true, &_lock);
      int preconditioner = _args.getOpt<int>("preconditioner", 10, PcMIC, &_lock);
      bool enforceCompatibility = _args.getOpt<bool>("enforceCompatibility", 11, false, &_lock);
      bool useL2Norm = _args.getOpt<bool>("useL2Norm", 12, false, &_lock);
      bool zeroPressureFixing = _args.getOpt<bool>("zeroPressureFixing", 13, false, &_lock);
      const Grid<Real> *curv = _args.getPtrOpt<Grid<Real>>("curv", 14, nullptr, &_lock);
      const Real surfTens = _args.getOpt<Real>("surfTens", 15, 0., &_lock);
      _retval = getPyNone();
      correctVelocity(vel,
                      pressure,
                      flags,
                      cgAccuracy,
                      phi,
                      perCellCorr,
                      fractions,
                      gfClamp,
                      cgMaxIterFac,
                      precondition,
                      preconditioner,
                      enforceCompatibility,
                      useL2Norm,
                      zeroPressureFixing,
                      curv,
                      surfTens);
      _args.check();
    }
    pbFinalizePlugin(parent, "correctVelocity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("correctVelocity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_correctVelocity("", "correctVelocity", _W_3);
extern "C" {
void PbRegister_correctVelocity()
{
  KEEP_UNUSED(_RP_correctVelocity);
}
}

//! Perform pressure projection of the velocity grid, calls
//! all three pressure helper functions in a row.

void solvePressure(MACGrid &vel,
                   Grid<Real> &pressure,
                   const FlagGrid &flags,
                   Real cgAccuracy = 1e-3,
                   const Grid<Real> *phi = nullptr,
                   const Grid<Real> *perCellCorr = nullptr,
                   const MACGrid *fractions = nullptr,
                   const MACGrid *obvel = nullptr,
                   Real gfClamp = 1e-04,
                   Real cgMaxIterFac = 1.5,
                   bool precondition = true,
                   int preconditioner = PcMIC,
                   bool enforceCompatibility = false,
                   bool useL2Norm = false,
                   bool zeroPressureFixing = false,
                   const Grid<Real> *curv = nullptr,
                   const Real surfTens = 0.,
                   Grid<Real> *retRhs = nullptr)
{
  Grid<Real> rhs(vel.getParent());

  computePressureRhs(rhs,
                     vel,
                     pressure,
                     flags,
                     cgAccuracy,
                     phi,
                     perCellCorr,
                     fractions,
                     obvel,
                     gfClamp,
                     cgMaxIterFac,
                     precondition,
                     preconditioner,
                     enforceCompatibility,
                     useL2Norm,
                     zeroPressureFixing,
                     curv,
                     surfTens);

  solvePressureSystem(rhs,
                      vel,
                      pressure,
                      flags,
                      cgAccuracy,
                      phi,
                      perCellCorr,
                      fractions,
                      gfClamp,
                      cgMaxIterFac,
                      precondition,
                      preconditioner,
                      enforceCompatibility,
                      useL2Norm,
                      zeroPressureFixing,
                      curv,
                      surfTens);

  correctVelocity(vel,
                  pressure,
                  flags,
                  cgAccuracy,
                  phi,
                  perCellCorr,
                  fractions,
                  gfClamp,
                  cgMaxIterFac,
                  precondition,
                  preconditioner,
                  enforceCompatibility,
                  useL2Norm,
                  zeroPressureFixing,
                  curv,
                  surfTens);

  // optionally , return RHS
  if (retRhs) {
    retRhs->copyFrom(rhs);
  }
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "solvePressure", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Real> &pressure = *_args.getPtr<Grid<Real>>("pressure", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 3, 1e-3, &_lock);
      const Grid<Real> *phi = _args.getPtrOpt<Grid<Real>>("phi", 4, nullptr, &_lock);
      const Grid<Real> *perCellCorr = _args.getPtrOpt<Grid<Real>>(
          "perCellCorr", 5, nullptr, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 6, nullptr, &_lock);
      const MACGrid *obvel = _args.getPtrOpt<MACGrid>("obvel", 7, nullptr, &_lock);
      Real gfClamp = _args.getOpt<Real>("gfClamp", 8, 1e-04, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 9, 1.5, &_lock);
      bool precondition = _args.getOpt<bool>("precondition", 10, true, &_lock);
      int preconditioner = _args.getOpt<int>("preconditioner", 11, PcMIC, &_lock);
      bool enforceCompatibility = _args.getOpt<bool>("enforceCompatibility", 12, false, &_lock);
      bool useL2Norm = _args.getOpt<bool>("useL2Norm", 13, false, &_lock);
      bool zeroPressureFixing = _args.getOpt<bool>("zeroPressureFixing", 14, false, &_lock);
      const Grid<Real> *curv = _args.getPtrOpt<Grid<Real>>("curv", 15, nullptr, &_lock);
      const Real surfTens = _args.getOpt<Real>("surfTens", 16, 0., &_lock);
      Grid<Real> *retRhs = _args.getPtrOpt<Grid<Real>>("retRhs", 17, nullptr, &_lock);
      _retval = getPyNone();
      solvePressure(vel,
                    pressure,
                    flags,
                    cgAccuracy,
                    phi,
                    perCellCorr,
                    fractions,
                    obvel,
                    gfClamp,
                    cgMaxIterFac,
                    precondition,
                    preconditioner,
                    enforceCompatibility,
                    useL2Norm,
                    zeroPressureFixing,
                    curv,
                    surfTens,
                    retRhs);
      _args.check();
    }
    pbFinalizePlugin(parent, "solvePressure", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("solvePressure", e.what());
    return 0;
  }
}
static const Pb::Register _RP_solvePressure("", "solvePressure", _W_4);
extern "C" {
void PbRegister_solvePressure()
{
  KEEP_UNUSED(_RP_solvePressure);
}
}

}  // namespace Manta
