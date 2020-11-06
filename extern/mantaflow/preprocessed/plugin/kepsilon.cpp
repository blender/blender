

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
 * Turbulence modeling plugins
 *
 ******************************************************************************/

#include "grid.h"
#include "commonkernels.h"
#include "vortexsheet.h"
#include "conjugategrad.h"

using namespace std;

namespace Manta {

// k-epsilon model constants
const Real keCmu = 0.09;
const Real keC1 = 1.44;
const Real keC2 = 1.92;
const Real keS1 = 1.0;
const Real keS2 = 1.3;

// k-epsilon limiters
const Real keU0 = 1.0;
const Real keImin = 2e-3;
const Real keImax = 1.0;
const Real keNuMin = 1e-3;
const Real keNuMax = 5.0;

//! clamp k and epsilon to limits

struct KnTurbulenceClamp : public KernelBase {
  KnTurbulenceClamp(
      Grid<Real> &kgrid, Grid<Real> &egrid, Real minK, Real maxK, Real minNu, Real maxNu)
      : KernelBase(&kgrid, 0),
        kgrid(kgrid),
        egrid(egrid),
        minK(minK),
        maxK(maxK),
        minNu(minNu),
        maxNu(maxNu)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 Grid<Real> &kgrid,
                 Grid<Real> &egrid,
                 Real minK,
                 Real maxK,
                 Real minNu,
                 Real maxNu) const
  {
    Real eps = egrid[idx];
    Real ke = clamp(kgrid[idx], minK, maxK);
    Real nu = keCmu * square(ke) / eps;
    if (nu > maxNu)
      eps = keCmu * square(ke) / maxNu;
    if (nu < minNu)
      eps = keCmu * square(ke) / minNu;

    kgrid[idx] = ke;
    egrid[idx] = eps;
  }
  inline Grid<Real> &getArg0()
  {
    return kgrid;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return egrid;
  }
  typedef Grid<Real> type1;
  inline Real &getArg2()
  {
    return minK;
  }
  typedef Real type2;
  inline Real &getArg3()
  {
    return maxK;
  }
  typedef Real type3;
  inline Real &getArg4()
  {
    return minNu;
  }
  typedef Real type4;
  inline Real &getArg5()
  {
    return maxNu;
  }
  typedef Real type5;
  void runMessage()
  {
    debMsg("Executing kernel KnTurbulenceClamp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, kgrid, egrid, minK, maxK, minNu, maxNu);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &kgrid;
  Grid<Real> &egrid;
  Real minK;
  Real maxK;
  Real minNu;
  Real maxNu;
};

//! Compute k-epsilon production term P = 2*nu_T*sum_ij(Sij^2) and the turbulent viscosity
//! nu_T=C_mu*k^2/eps

struct KnComputeProduction : public KernelBase {
  KnComputeProduction(const MACGrid &vel,
                      const Grid<Vec3> &velCenter,
                      const Grid<Real> &ke,
                      const Grid<Real> &eps,
                      Grid<Real> &prod,
                      Grid<Real> &nuT,
                      Grid<Real> *strain,
                      Real pscale = 1.0f)
      : KernelBase(&vel, 1),
        vel(vel),
        velCenter(velCenter),
        ke(ke),
        eps(eps),
        prod(prod),
        nuT(nuT),
        strain(strain),
        pscale(pscale)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const MACGrid &vel,
                 const Grid<Vec3> &velCenter,
                 const Grid<Real> &ke,
                 const Grid<Real> &eps,
                 Grid<Real> &prod,
                 Grid<Real> &nuT,
                 Grid<Real> *strain,
                 Real pscale = 1.0f) const
  {
    Real curEps = eps(i, j, k);
    if (curEps > 0) {
      // turbulent viscosity: nu_T = C_mu * k^2/eps
      Real curNu = keCmu * square(ke(i, j, k)) / curEps;

      // compute Sij = 1/2 * (dU_i/dx_j + dU_j/dx_i)
      Vec3 diag = Vec3(vel(i + 1, j, k).x, vel(i, j + 1, k).y, vel(i, j, k + 1).z) - vel(i, j, k);
      Vec3 ux = 0.5 * (velCenter(i + 1, j, k) - velCenter(i - 1, j, k));
      Vec3 uy = 0.5 * (velCenter(i, j + 1, k) - velCenter(i, j - 1, k));
      Vec3 uz = 0.5 * (velCenter(i, j, k + 1) - velCenter(i, j, k - 1));
      Real S12 = 0.5 * (ux.y + uy.x);
      Real S13 = 0.5 * (ux.z + uz.x);
      Real S23 = 0.5 * (uy.z + uz.y);
      Real S2 = square(diag.x) + square(diag.y) + square(diag.z) + 2.0 * square(S12) +
                2.0 * square(S13) + 2.0 * square(S23);

      // P = 2*nu_T*sum_ij(Sij^2)
      prod(i, j, k) = 2.0 * curNu * S2 * pscale;
      nuT(i, j, k) = curNu;
      if (strain)
        (*strain)(i, j, k) = sqrt(S2);
    }
    else {
      prod(i, j, k) = 0;
      nuT(i, j, k) = 0;
      if (strain)
        (*strain)(i, j, k) = 0;
    }
  }
  inline const MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline const Grid<Vec3> &getArg1()
  {
    return velCenter;
  }
  typedef Grid<Vec3> type1;
  inline const Grid<Real> &getArg2()
  {
    return ke;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> &getArg3()
  {
    return eps;
  }
  typedef Grid<Real> type3;
  inline Grid<Real> &getArg4()
  {
    return prod;
  }
  typedef Grid<Real> type4;
  inline Grid<Real> &getArg5()
  {
    return nuT;
  }
  typedef Grid<Real> type5;
  inline Grid<Real> *getArg6()
  {
    return strain;
  }
  typedef Grid<Real> type6;
  inline Real &getArg7()
  {
    return pscale;
  }
  typedef Real type7;
  void runMessage()
  {
    debMsg("Executing kernel KnComputeProduction ", 3);
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
            op(i, j, k, vel, velCenter, ke, eps, prod, nuT, strain, pscale);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, vel, velCenter, ke, eps, prod, nuT, strain, pscale);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const MACGrid &vel;
  const Grid<Vec3> &velCenter;
  const Grid<Real> &ke;
  const Grid<Real> &eps;
  Grid<Real> &prod;
  Grid<Real> &nuT;
  Grid<Real> *strain;
  Real pscale;
};

//! Compute k-epsilon production term P = 2*nu_T*sum_ij(Sij^2) and the turbulent viscosity
//! nu_T=C_mu*k^2/eps

void KEpsilonComputeProduction(const MACGrid &vel,
                               Grid<Real> &k,
                               Grid<Real> &eps,
                               Grid<Real> &prod,
                               Grid<Real> &nuT,
                               Grid<Real> *strain = 0,
                               Real pscale = 1.0f)
{
  // get centered velocity grid
  Grid<Vec3> vcenter(k.getParent());
  GetCentered(vcenter, vel);
  FillInBoundary(vcenter, 1);

  // compute limits
  const Real minK = 1.5 * square(keU0) * square(keImin);
  const Real maxK = 1.5 * square(keU0) * square(keImax);
  KnTurbulenceClamp(k, eps, minK, maxK, keNuMin, keNuMax);

  KnComputeProduction(vel, vcenter, k, eps, prod, nuT, strain, pscale);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "KEpsilonComputeProduction", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Real> &k = *_args.getPtr<Grid<Real>>("k", 1, &_lock);
      Grid<Real> &eps = *_args.getPtr<Grid<Real>>("eps", 2, &_lock);
      Grid<Real> &prod = *_args.getPtr<Grid<Real>>("prod", 3, &_lock);
      Grid<Real> &nuT = *_args.getPtr<Grid<Real>>("nuT", 4, &_lock);
      Grid<Real> *strain = _args.getPtrOpt<Grid<Real>>("strain", 5, 0, &_lock);
      Real pscale = _args.getOpt<Real>("pscale", 6, 1.0f, &_lock);
      _retval = getPyNone();
      KEpsilonComputeProduction(vel, k, eps, prod, nuT, strain, pscale);
      _args.check();
    }
    pbFinalizePlugin(parent, "KEpsilonComputeProduction", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("KEpsilonComputeProduction", e.what());
    return 0;
  }
}
static const Pb::Register _RP_KEpsilonComputeProduction("", "KEpsilonComputeProduction", _W_0);
extern "C" {
void PbRegister_KEpsilonComputeProduction()
{
  KEEP_UNUSED(_RP_KEpsilonComputeProduction);
}
}

//! Integrate source terms of k-epsilon equation

struct KnAddTurbulenceSource : public KernelBase {
  KnAddTurbulenceSource(Grid<Real> &kgrid, Grid<Real> &egrid, const Grid<Real> &pgrid, Real dt)
      : KernelBase(&kgrid, 0), kgrid(kgrid), egrid(egrid), pgrid(pgrid), dt(dt)
  {
    runMessage();
    run();
  }
  inline void op(
      IndexInt idx, Grid<Real> &kgrid, Grid<Real> &egrid, const Grid<Real> &pgrid, Real dt) const
  {
    Real eps = egrid[idx], prod = pgrid[idx], ke = kgrid[idx];
    if (ke <= 0)
      ke = 1e-3;  // pre-clamp to avoid nan

    Real newK = ke + dt * (prod - eps);
    Real newEps = eps + dt * (prod * keC1 - eps * keC2) * (eps / ke);
    if (newEps <= 0)
      newEps = 1e-4;  // pre-clamp to avoid nan

    kgrid[idx] = newK;
    egrid[idx] = newEps;
  }
  inline Grid<Real> &getArg0()
  {
    return kgrid;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return egrid;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return pgrid;
  }
  typedef Grid<Real> type2;
  inline Real &getArg3()
  {
    return dt;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel KnAddTurbulenceSource ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, kgrid, egrid, pgrid, dt);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &kgrid;
  Grid<Real> &egrid;
  const Grid<Real> &pgrid;
  Real dt;
};

//! Integrate source terms of k-epsilon equation
void KEpsilonSources(Grid<Real> &k, Grid<Real> &eps, Grid<Real> &prod)
{
  Real dt = k.getParent()->getDt();

  KnAddTurbulenceSource(k, eps, prod, dt);

  // compute limits
  const Real minK = 1.5 * square(keU0) * square(keImin);
  const Real maxK = 1.5 * square(keU0) * square(keImax);
  KnTurbulenceClamp(k, eps, minK, maxK, keNuMin, keNuMax);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "KEpsilonSources", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &k = *_args.getPtr<Grid<Real>>("k", 0, &_lock);
      Grid<Real> &eps = *_args.getPtr<Grid<Real>>("eps", 1, &_lock);
      Grid<Real> &prod = *_args.getPtr<Grid<Real>>("prod", 2, &_lock);
      _retval = getPyNone();
      KEpsilonSources(k, eps, prod);
      _args.check();
    }
    pbFinalizePlugin(parent, "KEpsilonSources", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("KEpsilonSources", e.what());
    return 0;
  }
}
static const Pb::Register _RP_KEpsilonSources("", "KEpsilonSources", _W_1);
extern "C" {
void PbRegister_KEpsilonSources()
{
  KEEP_UNUSED(_RP_KEpsilonSources);
}
}

//! Initialize the domain or boundary conditions
void KEpsilonBcs(
    const FlagGrid &flags, Grid<Real> &k, Grid<Real> &eps, Real intensity, Real nu, bool fillArea)
{
  // compute limits
  const Real vk = 1.5 * square(keU0) * square(intensity);
  const Real ve = keCmu * square(vk) / nu;

  FOR_IDX(k)
  {
    if (fillArea || flags.isObstacle(idx)) {
      k[idx] = vk;
      eps[idx] = ve;
    }
  }
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "KEpsilonBcs", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &k = *_args.getPtr<Grid<Real>>("k", 1, &_lock);
      Grid<Real> &eps = *_args.getPtr<Grid<Real>>("eps", 2, &_lock);
      Real intensity = _args.get<Real>("intensity", 3, &_lock);
      Real nu = _args.get<Real>("nu", 4, &_lock);
      bool fillArea = _args.get<bool>("fillArea", 5, &_lock);
      _retval = getPyNone();
      KEpsilonBcs(flags, k, eps, intensity, nu, fillArea);
      _args.check();
    }
    pbFinalizePlugin(parent, "KEpsilonBcs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("KEpsilonBcs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_KEpsilonBcs("", "KEpsilonBcs", _W_2);
extern "C" {
void PbRegister_KEpsilonBcs()
{
  KEEP_UNUSED(_RP_KEpsilonBcs);
}
}

//! Gradient diffusion smoothing. Not unconditionally stable -- should probably do substepping etc.
void ApplyGradDiff(
    const Grid<Real> &grid, Grid<Real> &res, const Grid<Real> &nu, Real dt, Real sigma)
{
  // should do this (but requires better boundary handling)
  /*MACGrid grad(grid.getParent());
  GradientOpMAC(grad, grid);
  grad *= nu;
  DivergenceOpMAC(res, grad);
  res *= dt/sigma;  */

  LaplaceOp(res, grid);
  res *= nu;
  res *= dt / sigma;
}

//! Compute k-epsilon turbulent viscosity
void KEpsilonGradientDiffusion(
    Grid<Real> &k, Grid<Real> &eps, Grid<Real> &nuT, Real sigmaU = 4.0, MACGrid *vel = 0)
{
  Real dt = k.getParent()->getDt();
  Grid<Real> res(k.getParent());

  // gradient diffusion of k
  ApplyGradDiff(k, res, nuT, dt, keS1);
  k += res;

  // gradient diffusion of epsilon
  ApplyGradDiff(eps, res, nuT, dt, keS2);
  eps += res;

  // gradient diffusion of velocity
  if (vel) {
    Grid<Real> vc(k.getParent());
    for (int c = 0; c < 3; c++) {
      GetComponent(*vel, vc, c);
      ApplyGradDiff(vc, res, nuT, dt, sigmaU);
      vc += res;
      SetComponent(*vel, vc, c);
    }
  }
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "KEpsilonGradientDiffusion", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &k = *_args.getPtr<Grid<Real>>("k", 0, &_lock);
      Grid<Real> &eps = *_args.getPtr<Grid<Real>>("eps", 1, &_lock);
      Grid<Real> &nuT = *_args.getPtr<Grid<Real>>("nuT", 2, &_lock);
      Real sigmaU = _args.getOpt<Real>("sigmaU", 3, 4.0, &_lock);
      MACGrid *vel = _args.getPtrOpt<MACGrid>("vel", 4, 0, &_lock);
      _retval = getPyNone();
      KEpsilonGradientDiffusion(k, eps, nuT, sigmaU, vel);
      _args.check();
    }
    pbFinalizePlugin(parent, "KEpsilonGradientDiffusion", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("KEpsilonGradientDiffusion", e.what());
    return 0;
  }
}
static const Pb::Register _RP_KEpsilonGradientDiffusion("", "KEpsilonGradientDiffusion", _W_3);
extern "C" {
void PbRegister_KEpsilonGradientDiffusion()
{
  KEEP_UNUSED(_RP_KEpsilonGradientDiffusion);
}
}

}  // namespace Manta
