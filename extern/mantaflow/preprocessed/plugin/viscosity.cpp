

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2020 Sebastian Barschkis, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Accurate Viscous Free Surfaces for Buckling, Coiling, and Rotating Liquids
 * Batty et al., SCA 2008
 *
 ******************************************************************************/

#include "conjugategrad.h"
#include "general.h"
#include "grid.h"
#include "vectorbase.h"

#include <chrono>

#if OPENMP == 1 || TBB == 1
#  define ENABLE_PARALLEL 0
#endif

#if ENABLE_PARALLEL == 1
#  include <thread>
#  include <algorithm>

static const int manta_num_threads = std::thread::hardware_concurrency();

#  define parallel_block \
    { \
      std::vector<std::thread> threads; \
      {

#  define do_parallel threads.push_back( std::thread([&]() {
#  define do_end \
    } ) );

#  define block_end \
    } \
    for (auto &thread : threads) { \
      thread.join(); \
    } \
    }

#endif

#define FOR_INT_IJK(num) \
  for (int k_off = 0; k_off < num; ++k_off) \
    for (int j_off = 0; j_off < num; ++j_off) \
      for (int i_off = 0; i_off < num; ++i_off)

using namespace std;

namespace Manta {

//! Assumes phi0<0 and phi1>=0, phi2>=0, and phi3>=0 or vice versa.
//! In particular, phi0 must not equal any of phi1, phi2 or phi3.
static Real sortedTetFraction(Real phi0, Real phi1, Real phi2, Real phi3)
{
  return phi0 * phi0 * phi0 / ((phi0 - phi1) * (phi0 - phi2) * (phi0 - phi3));
}

//! Assumes phi0<0, phi1<0, and phi2>=0, and phi3>=0 or vice versa.
//! In particular, phi0 and phi1 must not equal any of phi2 and phi3.
static Real sortedPrismFraction(Real phi0, Real phi1, Real phi2, Real phi3)
{
  Real a = phi0 / (phi0 - phi2);
  Real b = phi0 / (phi0 - phi3);
  Real c = phi1 / (phi1 - phi3);
  Real d = phi1 / (phi1 - phi2);
  return a * b * (1 - d) + b * (1 - c) * d + c * d;
}

Real volumeFraction(Real phi0, Real phi1, Real phi2, Real phi3)
{
  sort(phi0, phi1, phi2, phi3);
  if (phi3 <= 0)
    return 1;
  else if (phi2 <= 0)
    return 1 - sortedTetFraction(phi3, phi2, phi1, phi0);
  else if (phi1 <= 0)
    return sortedPrismFraction(phi0, phi1, phi2, phi3);
  else if (phi0 <= 0)
    return sortedTetFraction(phi0, phi1, phi2, phi3);
  else
    return 0;
}

//! The average of the two possible decompositions of the cube into five tetrahedra.
Real volumeFraction(Real phi000,
                    Real phi100,
                    Real phi010,
                    Real phi110,
                    Real phi001,
                    Real phi101,
                    Real phi011,
                    Real phi111)
{
  return (volumeFraction(phi000, phi001, phi101, phi011) +
          volumeFraction(phi000, phi101, phi100, phi110) +
          volumeFraction(phi000, phi010, phi011, phi110) +
          volumeFraction(phi101, phi011, phi111, phi110) +
          2 * volumeFraction(phi000, phi011, phi101, phi110) +
          volumeFraction(phi100, phi101, phi001, phi111) +
          volumeFraction(phi100, phi001, phi000, phi010) +
          volumeFraction(phi100, phi110, phi111, phi010) +
          volumeFraction(phi001, phi111, phi011, phi010) +
          2 * volumeFraction(phi100, phi111, phi001, phi010)) /
         12;
}

//! Kernel loop over grid with 2x base resolution!

struct KnEstimateVolumeFraction : public KernelBase {
  KnEstimateVolumeFraction(Grid<Real> &volumes,
                           const Grid<Real> &phi,
                           const Vec3 &startCentre,
                           const Real dx)
      : KernelBase(&volumes, 0), volumes(volumes), phi(phi), startCentre(startCentre), dx(dx)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &volumes,
                 const Grid<Real> &phi,
                 const Vec3 &startCentre,
                 const Real dx) const
  {
    const Vec3 centre = startCentre + Vec3(i, j, k) * 0.5;
    const Real offset = 0.5 * dx;
    const int order = 1;  // is sufficient

    Real phi000 = phi.getInterpolatedHi(centre + Vec3(-offset, -offset, -offset), order);
    Real phi001 = phi.getInterpolatedHi(centre + Vec3(-offset, -offset, +offset), order);
    Real phi010 = phi.getInterpolatedHi(centre + Vec3(-offset, +offset, -offset), order);
    Real phi011 = phi.getInterpolatedHi(centre + Vec3(-offset, +offset, +offset), order);
    Real phi100 = phi.getInterpolatedHi(centre + Vec3(+offset, -offset, -offset), order);
    Real phi101 = phi.getInterpolatedHi(centre + Vec3(+offset, -offset, +offset), order);
    Real phi110 = phi.getInterpolatedHi(centre + Vec3(+offset, +offset, -offset), order);
    Real phi111 = phi.getInterpolatedHi(centre + Vec3(+offset, +offset, +offset), order);

    volumes(i, j, k) = volumeFraction(
        phi000, phi100, phi010, phi110, phi001, phi101, phi011, phi111);
  }
  inline Grid<Real> &getArg0()
  {
    return volumes;
  }
  typedef Grid<Real> type0;
  inline const Grid<Real> &getArg1()
  {
    return phi;
  }
  typedef Grid<Real> type1;
  inline const Vec3 &getArg2()
  {
    return startCentre;
  }
  typedef Vec3 type2;
  inline const Real &getArg3()
  {
    return dx;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel KnEstimateVolumeFraction ", 3);
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
            op(i, j, k, volumes, phi, startCentre, dx);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, volumes, phi, startCentre, dx);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &volumes;
  const Grid<Real> &phi;
  const Vec3 &startCentre;
  const Real dx;
};

struct KnUpdateVolumeGrid : public KernelBase {
  KnUpdateVolumeGrid(Grid<Real> &cVolLiquid,
                     Grid<Real> &uVolLiquid,
                     Grid<Real> &vVolLiquid,
                     Grid<Real> &wVolLiquid,
                     Grid<Real> &exVolLiquid,
                     Grid<Real> &eyVolLiquid,
                     Grid<Real> &ezVolLiquid,
                     const Grid<Real> &src)
      : KernelBase(&cVolLiquid, 0),
        cVolLiquid(cVolLiquid),
        uVolLiquid(uVolLiquid),
        vVolLiquid(vVolLiquid),
        wVolLiquid(wVolLiquid),
        exVolLiquid(exVolLiquid),
        eyVolLiquid(eyVolLiquid),
        ezVolLiquid(ezVolLiquid),
        src(src)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &cVolLiquid,
                 Grid<Real> &uVolLiquid,
                 Grid<Real> &vVolLiquid,
                 Grid<Real> &wVolLiquid,
                 Grid<Real> &exVolLiquid,
                 Grid<Real> &eyVolLiquid,
                 Grid<Real> &ezVolLiquid,
                 const Grid<Real> &src) const
  {
    // Work out c
    cVolLiquid(i, j, k) = 0;
    FOR_INT_IJK(2)
    {
      cVolLiquid(i, j, k) += src(2 * i + i_off, 2 * j + j_off, 2 * k + k_off);
    }
    cVolLiquid(i, j, k) /= 8;

    // Work out u
    if (i >= 1) {
      uVolLiquid(i, j, k) = 0;
      int base_i = 2 * i - 1;
      int base_j = 2 * j;
      int base_k = 2 * k;
      FOR_INT_IJK(2)
      {
        uVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      uVolLiquid(i, j, k) /= 8;
    }

    // v
    if (j >= 1) {
      vVolLiquid(i, j, k) = 0;
      int base_i = 2 * i;
      int base_j = 2 * j - 1;
      int base_k = 2 * k;
      FOR_INT_IJK(2)
      {
        vVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      vVolLiquid(i, j, k) /= 8;
    }

    // w
    if (k >= 1) {
      wVolLiquid(i, j, k) = 0;
      int base_i = 2 * i;
      int base_j = 2 * j;
      int base_k = 2 * k - 1;
      FOR_INT_IJK(2)
      {
        wVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      wVolLiquid(i, j, k) /= 8;
    }

    // e-x
    if (j >= 1 && k >= 1) {
      exVolLiquid(i, j, k) = 0;
      int base_i = 2 * i;
      int base_j = 2 * j - 1;
      int base_k = 2 * k - 1;
      FOR_INT_IJK(2)
      {
        exVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      exVolLiquid(i, j, k) /= 8;
    }

    // e-y
    if (i >= 1 && k >= 1) {
      eyVolLiquid(i, j, k) = 0;
      int base_i = 2 * i - 1;
      int base_j = 2 * j;
      int base_k = 2 * k - 1;
      FOR_INT_IJK(2)
      {
        eyVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      eyVolLiquid(i, j, k) /= 8;
    }

    // e-z
    if (i >= 1 && j >= 1) {
      ezVolLiquid(i, j, k) = 0;
      int base_i = 2 * i - 1;
      int base_j = 2 * j - 1;
      int base_k = 2 * k;
      FOR_INT_IJK(2)
      {
        ezVolLiquid(i, j, k) += src(base_i + i_off, base_j + j_off, base_k + k_off);
      }
      ezVolLiquid(i, j, k) /= 8;
    }
  }
  inline Grid<Real> &getArg0()
  {
    return cVolLiquid;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return uVolLiquid;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> &getArg2()
  {
    return vVolLiquid;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> &getArg3()
  {
    return wVolLiquid;
  }
  typedef Grid<Real> type3;
  inline Grid<Real> &getArg4()
  {
    return exVolLiquid;
  }
  typedef Grid<Real> type4;
  inline Grid<Real> &getArg5()
  {
    return eyVolLiquid;
  }
  typedef Grid<Real> type5;
  inline Grid<Real> &getArg6()
  {
    return ezVolLiquid;
  }
  typedef Grid<Real> type6;
  inline const Grid<Real> &getArg7()
  {
    return src;
  }
  typedef Grid<Real> type7;
  void runMessage()
  {
    debMsg("Executing kernel KnUpdateVolumeGrid ", 3);
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
            op(i,
               j,
               k,
               cVolLiquid,
               uVolLiquid,
               vVolLiquid,
               wVolLiquid,
               exVolLiquid,
               eyVolLiquid,
               ezVolLiquid,
               src);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i,
             j,
             k,
             cVolLiquid,
             uVolLiquid,
             vVolLiquid,
             wVolLiquid,
             exVolLiquid,
             eyVolLiquid,
             ezVolLiquid,
             src);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &cVolLiquid;
  Grid<Real> &uVolLiquid;
  Grid<Real> &vVolLiquid;
  Grid<Real> &wVolLiquid;
  Grid<Real> &exVolLiquid;
  Grid<Real> &eyVolLiquid;
  Grid<Real> &ezVolLiquid;
  const Grid<Real> &src;
};

void computeWeights(const Grid<Real> &phi,
                    Grid<Real> &doubleSized,
                    Grid<Real> &cVolLiquid,
                    Grid<Real> &uVolLiquid,
                    Grid<Real> &vVolLiquid,
                    Grid<Real> &wVolLiquid,
                    Grid<Real> &exVolLiquid,
                    Grid<Real> &eyVolLiquid,
                    Grid<Real> &ezVolLiquid,
                    Real dx)
{
  KnEstimateVolumeFraction(doubleSized, phi, Vec3(0.25 * dx, 0.25 * dx, 0.25 * dx), 0.5 * dx);
  KnUpdateVolumeGrid(cVolLiquid,
                     uVolLiquid,
                     vVolLiquid,
                     wVolLiquid,
                     exVolLiquid,
                     eyVolLiquid,
                     ezVolLiquid,
                     doubleSized);
}

struct KnUpdateFaceStates : public KernelBase {
  KnUpdateFaceStates(const FlagGrid &flags,
                     Grid<int> &uState,
                     Grid<int> &vState,
                     Grid<int> &wState)
      : KernelBase(&flags, 0), flags(flags), uState(uState), vState(vState), wState(wState)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<int> &uState,
                 Grid<int> &vState,
                 Grid<int> &wState) const
  {
    bool curObs = flags.isObstacle(i, j, k);
    uState(i, j, k) = (i > 0 && !flags.isObstacle(i - 1, j, k) && !curObs) ?
                          FlagGrid::TypeFluid :
                          FlagGrid::TypeObstacle;
    vState(i, j, k) = (j > 0 && !flags.isObstacle(i, j - 1, k) && !curObs) ?
                          FlagGrid::TypeFluid :
                          FlagGrid::TypeObstacle;
    wState(i, j, k) = (k > 0 && !flags.isObstacle(i, j, k - 1) && !curObs) ?
                          FlagGrid::TypeFluid :
                          FlagGrid::TypeObstacle;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<int> &getArg1()
  {
    return uState;
  }
  typedef Grid<int> type1;
  inline Grid<int> &getArg2()
  {
    return vState;
  }
  typedef Grid<int> type2;
  inline Grid<int> &getArg3()
  {
    return wState;
  }
  typedef Grid<int> type3;
  void runMessage()
  {
    debMsg("Executing kernel KnUpdateFaceStates ", 3);
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
            op(i, j, k, flags, uState, vState, wState);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, uState, vState, wState);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<int> &uState;
  Grid<int> &vState;
  Grid<int> &wState;
};

struct KnApplyVelocities : public KernelBase {
  KnApplyVelocities(MACGrid &dst,
                    const Grid<int> &uState,
                    const Grid<int> &vState,
                    const Grid<int> &wState,
                    Grid<Real> &srcU,
                    Grid<Real> &srcV,
                    Grid<Real> &srcW)
      : KernelBase(&dst, 0),
        dst(dst),
        uState(uState),
        vState(vState),
        wState(wState),
        srcU(srcU),
        srcV(srcV),
        srcW(srcW)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &dst,
                 const Grid<int> &uState,
                 const Grid<int> &vState,
                 const Grid<int> &wState,
                 Grid<Real> &srcU,
                 Grid<Real> &srcV,
                 Grid<Real> &srcW) const
  {
    dst(i, j, k).x = (uState(i, j, k) == FlagGrid::TypeFluid) ? srcU(i, j, k) : 0;
    dst(i, j, k).y = (vState(i, j, k) == FlagGrid::TypeFluid) ? srcV(i, j, k) : 0;
    if (dst.is3D())
      dst(i, j, k).z = (wState(i, j, k) == FlagGrid::TypeFluid) ? srcW(i, j, k) : 0;
  }
  inline MACGrid &getArg0()
  {
    return dst;
  }
  typedef MACGrid type0;
  inline const Grid<int> &getArg1()
  {
    return uState;
  }
  typedef Grid<int> type1;
  inline const Grid<int> &getArg2()
  {
    return vState;
  }
  typedef Grid<int> type2;
  inline const Grid<int> &getArg3()
  {
    return wState;
  }
  typedef Grid<int> type3;
  inline Grid<Real> &getArg4()
  {
    return srcU;
  }
  typedef Grid<Real> type4;
  inline Grid<Real> &getArg5()
  {
    return srcV;
  }
  typedef Grid<Real> type5;
  inline Grid<Real> &getArg6()
  {
    return srcW;
  }
  typedef Grid<Real> type6;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyVelocities ", 3);
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
            op(i, j, k, dst, uState, vState, wState, srcU, srcV, srcW);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, dst, uState, vState, wState, srcU, srcV, srcW);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &dst;
  const Grid<int> &uState;
  const Grid<int> &vState;
  const Grid<int> &wState;
  Grid<Real> &srcU;
  Grid<Real> &srcV;
  Grid<Real> &srcW;
};

void solveViscosity(const FlagGrid &flags,
                    MACGrid &vel,
                    Grid<Real> &cVolLiquid,
                    Grid<Real> &uVolLiquid,
                    Grid<Real> &vVolLiquid,
                    Grid<Real> &wVolLiquid,
                    Grid<Real> &exVolLiquid,
                    Grid<Real> &eyVolLiquid,
                    Grid<Real> &ezVolLiquid,
                    Grid<Real> &viscosity,
                    const Real dt,
                    const Real dx,
                    const Real cgAccuracy,
                    const Real cgMaxIterFac)
{
  const Real factor = dt * square(1.0 / dx);
  const int maxIter = (int)(cgMaxIterFac * flags.getSize().max()) * (flags.is3D() ? 1 : 4);
  GridCg<ApplyMatrixViscosityU> *uGcg;
  GridCg<ApplyMatrixViscosityV> *vGcg;
  GridCg<ApplyMatrixViscosityW> *wGcg;

  // Tmp grids for CG solve in U, V, W dimensions
  FluidSolver *parent = flags.getParent();
  Grid<Real> uResidual(parent);
  Grid<Real> vResidual(parent);
  Grid<Real> wResidual(parent);
  Grid<Real> uSearch(parent);
  Grid<Real> vSearch(parent);
  Grid<Real> wSearch(parent);
  Grid<Real> uTmp(parent);
  Grid<Real> vTmp(parent);
  Grid<Real> wTmp(parent);
  Grid<Real> uRhs(parent);
  Grid<Real> vRhs(parent);
  Grid<Real> wRhs(parent);

  // A matrix U grids
  Grid<Real> uA0(parent);        // diagonal elements in A matrix
  Grid<Real> uAplusi(parent);    // neighbor at i+1
  Grid<Real> uAplusj(parent);    // neighbor at j+1
  Grid<Real> uAplusk(parent);    // neighbor at k+1
  Grid<Real> uAminusi(parent);   // neighbor at i-1
  Grid<Real> uAminusj(parent);   // neighbor at j-1
  Grid<Real> uAminusk(parent);   // neighbor at k-1
  Grid<Real> uAhelper1(parent);  // additional helper grids for off diagonal elements
  Grid<Real> uAhelper2(parent);
  Grid<Real> uAhelper3(parent);
  Grid<Real> uAhelper4(parent);
  Grid<Real> uAhelper5(parent);
  Grid<Real> uAhelper6(parent);
  Grid<Real> uAhelper7(parent);
  Grid<Real> uAhelper8(parent);

  // A matrix V grids
  Grid<Real> vA0(parent);
  Grid<Real> vAplusi(parent);
  Grid<Real> vAplusj(parent);
  Grid<Real> vAplusk(parent);
  Grid<Real> vAminusi(parent);
  Grid<Real> vAminusj(parent);
  Grid<Real> vAminusk(parent);
  Grid<Real> vAhelper1(parent);
  Grid<Real> vAhelper2(parent);
  Grid<Real> vAhelper3(parent);
  Grid<Real> vAhelper4(parent);
  Grid<Real> vAhelper5(parent);
  Grid<Real> vAhelper6(parent);
  Grid<Real> vAhelper7(parent);
  Grid<Real> vAhelper8(parent);

  // A matrix W grids
  Grid<Real> wA0(parent);
  Grid<Real> wAplusi(parent);
  Grid<Real> wAplusj(parent);
  Grid<Real> wAplusk(parent);
  Grid<Real> wAminusi(parent);
  Grid<Real> wAminusj(parent);
  Grid<Real> wAminusk(parent);
  Grid<Real> wAhelper1(parent);
  Grid<Real> wAhelper2(parent);
  Grid<Real> wAhelper3(parent);
  Grid<Real> wAhelper4(parent);
  Grid<Real> wAhelper5(parent);
  Grid<Real> wAhelper6(parent);
  Grid<Real> wAhelper7(parent);
  Grid<Real> wAhelper8(parent);

  // Solution grids for CG solvers
  Grid<Real> uSolution(parent);
  Grid<Real> vSolution(parent);
  Grid<Real> wSolution(parent);

  // Save state of voxel face (fluid or obstacle)
  Grid<int> uState(parent);
  Grid<int> vState(parent);
  Grid<int> wState(parent);

  // Save state of voxel face (fluid or obstacle)
  KnUpdateFaceStates(flags, uState, vState, wState);

  // Shorter names for flags, we will use them often
  int isFluid = FlagGrid::TypeFluid;
  int isObstacle = FlagGrid::TypeObstacle;

  // Main viscosity loop: construct A matrices and rhs's in all dimensions
  FOR_IJK_BND(flags, 1)
  {

    // U-terms: 2u_xx+ v_xy +uyy + u_zz + w_xz
    if (uState(i, j, k) == isFluid) {

      uRhs(i, j, k) = uVolLiquid(i, j, k) * vel(i, j, k).x;
      uA0(i, j, k) = uVolLiquid(i, j, k);

      Real viscRight = viscosity(i, j, k);
      Real viscLeft = viscosity(i - 1, j, k);
      Real volRight = cVolLiquid(i, j, k);
      Real volLeft = cVolLiquid(i - 1, j, k);

      Real viscTop = 0.25 * (viscosity(i - 1, j + 1, k) + viscosity(i - 1, j, k) +
                             viscosity(i, j + 1, k) + viscosity(i, j, k));
      Real viscBottom = 0.25 * (viscosity(i - 1, j, k) + viscosity(i - 1, j - 1, k) +
                                viscosity(i, j, k) + viscosity(i, j - 1, k));
      Real volTop = ezVolLiquid(i, j + 1, k);
      Real volBottom = ezVolLiquid(i, j, k);

      Real viscFront = 0.25 * (viscosity(i - 1, j, k + 1) + viscosity(i - 1, j, k) +
                               viscosity(i, j, k + 1) + viscosity(i, j, k));
      Real viscBack = 0.25 * (viscosity(i - 1, j, k) + viscosity(i - 1, j, k - 1) +
                              viscosity(i, j, k) + viscosity(i, j, k - 1));
      Real volFront = eyVolLiquid(i, j, k + 1);
      Real volBack = eyVolLiquid(i, j, k);

      Real factorRight = 2 * factor * viscRight * volRight;
      Real factorLeft = 2 * factor * viscLeft * volLeft;
      Real factorTop = factor * viscTop * volTop;
      Real factorBottom = factor * viscBottom * volBottom;
      Real factorFront = factor * viscFront * volFront;
      Real factorBack = factor * viscBack * volBack;

      // u_x_right
      uA0(i, j, k) += factorRight;
      if (uState(i + 1, j, k) == isFluid) {
        uAplusi(i, j, k) += -factorRight;
      }
      else if (uState(i + 1, j, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i + 1, j, k).x * factorRight;
      }

      // u_x_left
      uA0(i, j, k) += factorLeft;
      if (uState(i - 1, j, k) == isFluid) {
        uAminusi(i, j, k) += -factorLeft;
      }
      else if (uState(i - 1, j, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i - 1, j, k).x * factorLeft;
      }

      // u_y_top
      uA0(i, j, k) += factorTop;
      if (uState(i, j + 1, k) == isFluid) {
        uAplusj(i, j, k) += -factorTop;
      }
      else if (uState(i, j + 1, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j + 1, k).x * factorTop;
      }

      // u_y_bottom
      uA0(i, j, k) += factorBottom;
      if (uState(i, j - 1, k) == isFluid) {
        uAminusj(i, j, k) += -factorBottom;
      }
      else if (uState(i, j - 1, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j - 1, k).x * factorBottom;
      }

      // u_z_front
      uA0(i, j, k) += factorFront;
      if (uState(i, j, k + 1) == isFluid) {
        uAplusk(i, j, k) += -factorFront;
      }
      else if (uState(i, j, k + 1) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j, k + 1).x * factorFront;
      }

      // u_z_back
      uA0(i, j, k) += factorBack;
      if (uState(i, j, k - 1) == isFluid) {
        uAminusk(i, j, k) += -factorBack;
      }
      else if (uState(i, j, k - 1) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j, k - 1).x * factorBack;
      }

      // v_x_top
      if (vState(i, j + 1, k) == isFluid) {
        uAhelper1(i, j, k) += -factorTop;
      }
      else if (vState(i, j + 1, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j + 1, k).y * factorTop;
      }

      if (vState(i - 1, j + 1, k) == isFluid) {
        uAhelper2(i, j, k) += factorTop;
      }
      else if (vState(i - 1, j + 1, k) == isObstacle) {
        uRhs(i, j, k) -= vel(i - 1, j + 1, k).y * factorTop;
      }

      // v_x_bottom
      if (vState(i, j, k) == isFluid) {
        uAhelper3(i, j, k) += factorBottom;
      }
      else if (vState(i, j, k) == isObstacle) {
        uRhs(i, j, k) -= vel(i, j, k).y * factorBottom;
      }

      if (vState(i - 1, j, k) == isFluid) {
        uAhelper4(i, j, k) += -factorBottom;
      }
      else if (vState(i - 1, j, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i - 1, j, k).y * factorBottom;
      }

      // w_x_front
      if (wState(i, j, k + 1) == isFluid) {
        uAhelper5(i, j, k) += -factorFront;
      }
      else if (wState(i, j, k + 1) == isObstacle) {
        uRhs(i, j, k) -= -vel(i, j, k + 1).z * factorFront;
      }

      if (wState(i - 1, j, k + 1) == isFluid) {
        uAhelper6(i, j, k) += factorFront;
      }
      else if (wState(i - 1, j, k + 1) == isObstacle) {
        uRhs(i, j, k) -= vel(i - 1, j, k + 1).z * factorFront;
      }

      // w_x_back
      if (wState(i, j, k) == isFluid) {
        uAhelper7(i, j, k) += factorBack;
      }
      else if (wState(i, j, k) == isObstacle) {
        uRhs(i, j, k) -= vel(i, j, k).z * factorBack;
      }

      if (wState(i - 1, j, k) == isFluid) {
        uAhelper8(i, j, k) += -factorBack;
      }
      else if (wState(i - 1, j, k) == isObstacle) {
        uRhs(i, j, k) -= -vel(i - 1, j, k).z * factorBack;
      }
    }

    // V-terms: vxx + 2vyy + vzz + u_yx + w_yz
    if (vState(i, j, k) == isFluid) {

      vRhs(i, j, k) = vVolLiquid(i, j, k) * vel(i, j, k).y;
      vA0(i, j, k) = vVolLiquid(i, j, k);

      Real viscRight = 0.25 * (viscosity(i, j - 1, k) + viscosity(i + 1, j - 1, k) +
                               viscosity(i, j, k) + viscosity(i + 1, j, k));
      Real viscLeft = 0.25 * (viscosity(i, j - 1, k) + viscosity(i - 1, j - 1, k) +
                              viscosity(i, j, k) + viscosity(i - 1, j, k));
      Real volRight = ezVolLiquid(i + 1, j, k);
      Real volLeft = ezVolLiquid(i, j, k);

      Real viscTop = viscosity(i, j, k);
      Real viscBottom = viscosity(i, j - 1, k);
      Real volTop = cVolLiquid(i, j, k);
      Real volBottom = cVolLiquid(i, j - 1, k);

      Real viscFront = 0.25 * (viscosity(i, j - 1, k) + viscosity(i, j - 1, k + 1) +
                               viscosity(i, j, k) + viscosity(i, j, k + 1));
      Real viscBack = 0.25 * (viscosity(i, j - 1, k) + viscosity(i, j - 1, k - 1) +
                              viscosity(i, j, k) + viscosity(i, j, k - 1));
      Real volFront = exVolLiquid(i, j, k + 1);
      Real volBack = exVolLiquid(i, j, k);

      Real factorRight = factor * viscRight * volRight;
      Real factorLeft = factor * viscLeft * volLeft;
      Real factorTop = 2 * factor * viscTop * volTop;
      Real factorBottom = 2 * factor * viscBottom * volBottom;
      Real factorFront = factor * viscFront * volFront;
      Real factorBack = factor * viscBack * volBack;

      // v_x_right
      vA0(i, j, k) += factorRight;
      if (vState(i + 1, j, k) == isFluid) {
        vAplusi(i, j, k) += -factorRight;
      }
      else if (vState(i + 1, j, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i + 1, j, k).y * factorRight;
      }

      // v_x_left
      vA0(i, j, k) += factorLeft;
      if (vState(i - 1, j, k) == isFluid) {
        vAminusi(i, j, k) += -factorLeft;
      }
      else if (vState(i - 1, j, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i - 1, j, k).y * factorLeft;
      }

      // vy_top
      vA0(i, j, k) += factorTop;
      if (vState(i, j + 1, k) == isFluid) {
        vAplusj(i, j, k) += -factorTop;
      }
      else if (vState(i, j + 1, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j + 1, k).y * factorTop;
      }

      // vy_bottom
      vA0(i, j, k) += factorBottom;
      if (vState(i, j - 1, k) == isFluid) {
        vAminusj(i, j, k) += -factorBottom;
      }
      else if (vState(i, j - 1, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j - 1, k).y * factorBottom;
      }

      // v_z_front
      vA0(i, j, k) += factorFront;
      if (vState(i, j, k + 1) == isFluid) {
        vAplusk(i, j, k) += -factorFront;
      }
      else if (vState(i, j, k + 1) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j, k + 1).y * factorFront;
      }

      // v_z_back
      vA0(i, j, k) += factorBack;
      if (vState(i, j, k - 1) == isFluid) {
        vAminusk(i, j, k) += -factorBack;
      }
      else if (vState(i, j, k - 1) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j, k - 1).y * factorBack;
      }

      // u_y_right
      if (uState(i + 1, j, k) == isFluid) {
        vAhelper1(i, j, k) += -factorRight;
      }
      else if (uState(i + 1, j, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i + 1, j, k).x * factorRight;
      }

      if (uState(i + 1, j - 1, k) == isFluid) {
        vAhelper2(i, j, k) += factorRight;
      }
      else if (uState(i + 1, j - 1, k) == isObstacle) {
        vRhs(i, j, k) -= vel(i + 1, j - 1, k).x * factorRight;
      }

      // u_y_left
      if (uState(i, j, k) == isFluid) {
        vAhelper3(i, j, k) += factorLeft;
      }
      else if (uState(i, j, k) == isObstacle) {
        vRhs(i, j, k) -= vel(i, j, k).x * factorLeft;
      }

      if (uState(i, j - 1, k) == isFluid) {
        vAhelper4(i, j, k) += -factorLeft;
      }
      else if (uState(i, j - 1, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j - 1, k).x * factorLeft;
      }

      // w_y_front
      if (wState(i, j, k + 1) == isFluid) {
        vAhelper5(i, j, k) += -factorFront;
      }
      else if (wState(i, j, k + 1) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j, k + 1).z * factorFront;
      }

      if (wState(i, j - 1, k + 1) == isFluid) {
        vAhelper6(i, j, k) += factorFront;
      }
      else if (wState(i, j - 1, k + 1) == isObstacle) {
        vRhs(i, j, k) -= vel(i, j - 1, k + 1).z * factorFront;
      }

      // w_y_back
      if (wState(i, j, k) == isFluid) {
        vAhelper7(i, j, k) += factorBack;
      }
      else if (wState(i, j, k) == isObstacle) {
        vRhs(i, j, k) -= vel(i, j, k).z * factorBack;
      }

      if (wState(i, j - 1, k) == isFluid) {
        vAhelper8(i, j, k) += -factorBack;
      }
      else if (wState(i, j - 1, k) == isObstacle) {
        vRhs(i, j, k) -= -vel(i, j - 1, k).z * factorBack;
      }
    }

    // W-terms: wxx+ wyy+ 2wzz + u_zx + v_zy
    if (wState(i, j, k) == isFluid) {

      wRhs(i, j, k) = wVolLiquid(i, j, k) * vel(i, j, k).z;
      wA0(i, j, k) = wVolLiquid(i, j, k);

      Real viscRight = 0.25 * (viscosity(i, j, k) + viscosity(i, j, k - 1) +
                               viscosity(i + 1, j, k) + viscosity(i + 1, j, k - 1));
      Real viscLeft = 0.25 * (viscosity(i, j, k) + viscosity(i, j, k - 1) +
                              viscosity(i - 1, j, k) + viscosity(i - 1, j, k - 1));
      Real volRight = eyVolLiquid(i + 1, j, k);
      Real volLeft = eyVolLiquid(i, j, k);

      Real viscTop = 0.25 * (viscosity(i, j, k) + viscosity(i, j, k - 1) + viscosity(i, j + 1, k) +
                             viscosity(i, j + 1, k - 1));
      Real viscBottom = 0.25 * (viscosity(i, j, k) + viscosity(i, j, k - 1) +
                                viscosity(i, j - 1, k) + viscosity(i, j - 1, k - 1));
      Real volTop = exVolLiquid(i, j + 1, k);
      Real volBottom = exVolLiquid(i, j, k);

      Real viscFront = viscosity(i, j, k);
      Real viscBack = viscosity(i, j, k - 1);
      Real volFront = cVolLiquid(i, j, k);
      Real volBack = cVolLiquid(i, j, k - 1);

      Real factorRight = factor * viscRight * volRight;
      Real factorLeft = factor * viscLeft * volLeft;
      Real factorTop = factor * viscTop * volTop;
      Real factorBottom = factor * viscBottom * volBottom;
      Real factorFront = 2 * factor * viscFront * volFront;
      Real factorBack = 2 * factor * viscBack * volBack;

      // w_x_right
      wA0(i, j, k) += factorRight;
      if (wState(i + 1, j, k) == isFluid) {
        wAplusi(i, j, k) += -factorRight;
      }
      else if (wState(i + 1, j, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i + 1, j, k).z * factorRight;
      }

      // w_x_left
      wA0(i, j, k) += factorLeft;
      if (wState(i - 1, j, k) == isFluid) {
        wAminusi(i, j, k) += -factorLeft;
      }
      else if (wState(i - 1, j, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i - 1, j, k).z * factorLeft;
      }

      // w_y_top
      wA0(i, j, k) += factorTop;
      if (wState(i, j + 1, k) == isFluid) {
        wAplusj(i, j, k) += -factorTop;
      }
      else if (wState(i, j + 1, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j + 1, k).z * factorTop;
      }

      // w_y_bottom
      wA0(i, j, k) += factorBottom;
      if (wState(i, j - 1, k) == isFluid) {
        wAminusj(i, j, k) += -factorBottom;
      }
      else if (wState(i, j - 1, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j - 1, k).z * factorBottom;
      }

      // w_z_front
      wA0(i, j, k) += factorFront;
      if (wState(i, j, k + 1) == isFluid) {
        wAplusk(i, j, k) += -factorFront;
      }
      else if (wState(i, j, k + 1) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j, k + 1).z * factorFront;
      }

      // w_z_back
      wA0(i, j, k) += factorBack;
      if (wState(i, j, k - 1) == isFluid) {
        wAminusk(i, j, k) += -factorBack;
      }
      else if (wState(i, j, k - 1) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j, k - 1).z * factorBack;
      }

      // u_z_right
      if (uState(i + 1, j, k) == isFluid) {
        wAhelper1(i, j, k) += -factorRight;
      }
      else if (uState(i + 1, j, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i + 1, j, k).x * factorRight;
      }

      if (uState(i + 1, j, k - 1) == isFluid) {
        wAhelper2(i, j, k) += factorRight;
      }
      else if (uState(i + 1, j, k - 1) == isObstacle) {
        wRhs(i, j, k) -= vel(i + 1, j, k - 1).x * factorRight;
      }

      // u_z_left
      if (uState(i, j, k) == isFluid) {
        wAhelper3(i, j, k) += factorLeft;
      }
      else if (uState(i, j, k) == isObstacle) {
        wRhs(i, j, k) -= vel(i, j, k).x * factorLeft;
      }

      if (uState(i, j, k - 1) == isFluid) {
        wAhelper4(i, j, k) += -factorLeft;
      }
      else if (uState(i, j, k - 1) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j, k - 1).x * factorLeft;
      }

      // v_z_top
      if (vState(i, j + 1, k) == isFluid) {
        wAhelper5(i, j, k) += -factorTop;
      }
      else if (vState(i, j + 1, k) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j + 1, k).y * factorTop;
      }

      if (vState(i, j + 1, k - 1) == isFluid) {
        wAhelper6(i, j, k) += factorTop;
      }
      else if (vState(i, j + 1, k - 1) == isObstacle) {
        wRhs(i, j, k) -= vel(i, j + 1, k - 1).y * factorTop;
      }

      // v_z_bottom
      if (vState(i, j, k) == isFluid) {
        wAhelper7(i, j, k) += factorBottom;
      }
      else if (vState(i, j, k) == isObstacle) {
        wRhs(i, j, k) -= vel(i, j, k).y * factorBottom;
      }

      if (vState(i, j, k - 1) == isFluid) {
        wAhelper8(i, j, k) += -factorBottom;
      }
      else if (vState(i, j, k - 1) == isObstacle) {
        wRhs(i, j, k) -= -vel(i, j, k - 1).y * factorBottom;
      }
    }
  }

  // CG solver for U
  if (flags.is3D()) {
    vector<Grid<Real> *> uMatA{&uA0,
                               &uAplusi,
                               &uAplusj,
                               &uAplusk,
                               &uAminusi,
                               &uAminusj,
                               &uAminusk,
                               &uAhelper1,
                               &uAhelper2,
                               &uAhelper3,
                               &uAhelper4,
                               &uAhelper5,
                               &uAhelper6,
                               &uAhelper7,
                               &uAhelper8};
    vector<Grid<Real> *> uVecRhs{&vRhs, &wRhs};
    uGcg = new GridCg<ApplyMatrixViscosityU>(
        uSolution, uRhs, uResidual, uSearch, flags, uTmp, uMatA, uVecRhs);
  }
  else {
    errMsg("Viscosity: 2D Matrix application not yet supported in viscosity solver");
  }

  // CG solver for V
  if (flags.is3D()) {
    vector<Grid<Real> *> vMatA{&vA0,
                               &vAplusi,
                               &vAplusj,
                               &vAplusk,
                               &vAminusi,
                               &vAminusj,
                               &vAminusk,
                               &vAhelper1,
                               &vAhelper2,
                               &vAhelper3,
                               &vAhelper4,
                               &vAhelper5,
                               &vAhelper6,
                               &vAhelper7,
                               &vAhelper8};
    vector<Grid<Real> *> vVecRhs{&uRhs, &wRhs};
    vGcg = new GridCg<ApplyMatrixViscosityV>(
        vSolution, vRhs, vResidual, vSearch, flags, vTmp, vMatA, vVecRhs);
  }
  else {
    errMsg("Viscosity: 2D Matrix application not yet supported in viscosity solver");
  }

  // CG solver for W
  if (flags.is3D()) {
    vector<Grid<Real> *> wMatA{&wA0,
                               &wAplusi,
                               &wAplusj,
                               &wAplusk,
                               &wAminusi,
                               &wAminusj,
                               &wAminusk,
                               &wAhelper1,
                               &wAhelper2,
                               &wAhelper3,
                               &wAhelper4,
                               &wAhelper5,
                               &wAhelper6,
                               &wAhelper7,
                               &wAhelper8};
    vector<Grid<Real> *> wVecRhs{&uRhs, &vRhs};
    wGcg = new GridCg<ApplyMatrixViscosityW>(
        wSolution, wRhs, wResidual, wSearch, flags, wTmp, wMatA, wVecRhs);
  }
  else {
    errMsg("Viscosity: 2D Matrix application not yet supported in viscosity solver");
  }

  // Same accuracy for all dimensions
  uGcg->setAccuracy(cgAccuracy);
  vGcg->setAccuracy(cgAccuracy);
  wGcg->setAccuracy(cgAccuracy);

  // CG solve. Preconditioning not supported yet. Instead, U, V, W  can optionally be solved in
  // parallel.
  for (int uIter = 0, vIter = 0, wIter = 0; uIter < maxIter || vIter < maxIter || wIter < maxIter;
       uIter++, vIter++, wIter++) {
#if ENABLE_PARALLEL == 1
    parallel_block do_parallel
#endif
        if (uIter < maxIter && !uGcg->iterate()) uIter = maxIter;
#if ENABLE_PARALLEL == 1
    do_end do_parallel
#endif
        if (vIter < maxIter && !vGcg->iterate()) vIter = maxIter;
#if ENABLE_PARALLEL == 1
    do_end do_parallel
#endif
        if (wIter < maxIter && !wGcg->iterate()) wIter = maxIter;
#if ENABLE_PARALLEL == 1
    do_end block_end
#endif

        // Make sure that next CG iteration has updated rhs grids
        uRhs.copyFrom(uSearch);
    vRhs.copyFrom(vSearch);
    wRhs.copyFrom(wSearch);
  }
  debMsg(
      "Viscosity: solveViscosity() done. "
      "Iterations (u,v,w): ("
          << uGcg->getIterations() << "," << vGcg->getIterations() << "," << wGcg->getIterations()
          << "), "
             "Residual (u,v,w): ("
          << uGcg->getResNorm() << "," << vGcg->getResNorm() << "," << wGcg->getResNorm() << ")",
      2);

  delete uGcg;
  delete vGcg;
  delete wGcg;

  // Apply solutions to global velocity grid
  KnApplyVelocities(vel, uState, vState, wState, uSolution, vSolution, wSolution);
}

//! To use the viscosity plugin, scenes must call this function before solving pressure.
//! Note that the 'volumes' grid uses 2x the base resolution

void applyViscosity(const FlagGrid &flags,
                    const Grid<Real> &phi,
                    MACGrid &vel,
                    Grid<Real> &volumes,
                    Grid<Real> &viscosity,
                    const Real cgAccuracy = 1e-9,
                    const Real cgMaxIterFac = 1.5)
{
  const Real dx = flags.getParent()->getDx();
  const Real dt = flags.getParent()->getDt();

  // Reserve temp grids for volume weight calculation
  FluidSolver *parent = flags.getParent();
  Grid<Real> cVolLiquid(parent);
  Grid<Real> uVolLiquid(parent);
  Grid<Real> vVolLiquid(parent);
  Grid<Real> wVolLiquid(parent);
  Grid<Real> exVolLiquid(parent);
  Grid<Real> eyVolLiquid(parent);
  Grid<Real> ezVolLiquid(parent);

  // Ensure final weight grid gets cleared at every step
  volumes.clear();

  // Save viscous fluid volume in double-sized volumes grid
  computeWeights(phi,
                 volumes,
                 cVolLiquid,
                 uVolLiquid,
                 vVolLiquid,
                 wVolLiquid,
                 exVolLiquid,
                 eyVolLiquid,
                 ezVolLiquid,
                 dx);

  // Set up A matrix and rhs. Solve with CG. Update velocity grid.
  solveViscosity(flags,
                 vel,
                 cVolLiquid,
                 uVolLiquid,
                 vVolLiquid,
                 wVolLiquid,
                 exVolLiquid,
                 eyVolLiquid,
                 ezVolLiquid,
                 viscosity,
                 dt,
                 dx,
                 cgAccuracy,
                 cgMaxIterFac);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "applyViscosity", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const Grid<Real> &phi = *_args.getPtr<Grid<Real>>("phi", 1, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 2, &_lock);
      Grid<Real> &volumes = *_args.getPtr<Grid<Real>>("volumes", 3, &_lock);
      Grid<Real> &viscosity = *_args.getPtr<Grid<Real>>("viscosity", 4, &_lock);
      const Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 5, 1e-9, &_lock);
      const Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 6, 1.5, &_lock);
      _retval = getPyNone();
      applyViscosity(flags, phi, vel, volumes, viscosity, cgAccuracy, cgMaxIterFac);
      _args.check();
    }
    pbFinalizePlugin(parent, "applyViscosity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("applyViscosity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_applyViscosity("", "applyViscosity", _W_0);
extern "C" {
void PbRegister_applyViscosity()
{
  KEEP_UNUSED(_RP_applyViscosity);
}
}

}  // namespace Manta

#if ENABLE_PARALLEL == 1

#  undef parallel_block
#  undef do_parallel
#  undef do_end
#  undef block_end
#  undef parallel_for
#  undef parallel_end

#endif
