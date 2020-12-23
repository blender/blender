

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
 * Wave equation
 *
 ******************************************************************************/

#include "levelset.h"
#include "commonkernels.h"
#include "particle.h"
#include "conjugategrad.h"
#include <cmath>

using namespace std;

namespace Manta {

/******************************************************************************
 *
 * explicit integration
 *
 ******************************************************************************/

struct knCalcSecDeriv2d : public KernelBase {
  knCalcSecDeriv2d(const Grid<Real> &v, Grid<Real> &ret) : KernelBase(&v, 1), v(v), ret(ret)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<Real> &v, Grid<Real> &ret) const
  {
    ret(i, j, k) = (-4. * v(i, j, k) + v(i - 1, j, k) + v(i + 1, j, k) + v(i, j - 1, k) +
                    v(i, j + 1, k));
  }
  inline const Grid<Real> &getArg0()
  {
    return v;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return ret;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel knCalcSecDeriv2d ", 3);
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
            op(i, j, k, v, ret);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, v, ret);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const Grid<Real> &v;
  Grid<Real> &ret;
};
;

//! calculate a second derivative for the wave equation
void calcSecDeriv2d(const Grid<Real> &v, Grid<Real> &curv)
{
  knCalcSecDeriv2d(v, curv);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "calcSecDeriv2d", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const Grid<Real> &v = *_args.getPtr<Grid<Real>>("v", 0, &_lock);
      Grid<Real> &curv = *_args.getPtr<Grid<Real>>("curv", 1, &_lock);
      _retval = getPyNone();
      calcSecDeriv2d(v, curv);
      _args.check();
    }
    pbFinalizePlugin(parent, "calcSecDeriv2d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("calcSecDeriv2d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_calcSecDeriv2d("", "calcSecDeriv2d", _W_0);
extern "C" {
void PbRegister_calcSecDeriv2d()
{
  KEEP_UNUSED(_RP_calcSecDeriv2d);
}
}

// mass conservation

struct knTotalSum : public KernelBase {
  knTotalSum(Grid<Real> &h) : KernelBase(&h, 1), h(h), sum(0)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &h, double &sum)
  {
    sum += h(i, j, k);
  }
  inline operator double()
  {
    return sum;
  }
  inline double &getRet()
  {
    return sum;
  }
  inline Grid<Real> &getArg0()
  {
    return h;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel knTotalSum ", 3);
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
            op(i, j, k, h, sum);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, h, sum);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  knTotalSum(knTotalSum &o, tbb::split) : KernelBase(o), h(o.h), sum(0)
  {
  }
  void join(const knTotalSum &o)
  {
    sum += o.sum;
  }
  Grid<Real> &h;
  double sum;
};

//! calculate the sum of all values in a grid (for wave equation solves)
Real totalSum(Grid<Real> &height)
{
  knTotalSum ts(height);
  return ts.sum;
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "totalSum", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &height = *_args.getPtr<Grid<Real>>("height", 0, &_lock);
      _retval = toPy(totalSum(height));
      _args.check();
    }
    pbFinalizePlugin(parent, "totalSum", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("totalSum", e.what());
    return 0;
  }
}
static const Pb::Register _RP_totalSum("", "totalSum", _W_1);
extern "C" {
void PbRegister_totalSum()
{
  KEEP_UNUSED(_RP_totalSum);
}
}

//! normalize all values in a grid (for wave equation solves)
void normalizeSumTo(Grid<Real> &height, Real target)
{
  knTotalSum ts(height);
  Real factor = target / ts.sum;
  height.multConst(factor);
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "normalizeSumTo", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &height = *_args.getPtr<Grid<Real>>("height", 0, &_lock);
      Real target = _args.get<Real>("target", 1, &_lock);
      _retval = getPyNone();
      normalizeSumTo(height, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "normalizeSumTo", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("normalizeSumTo", e.what());
    return 0;
  }
}
static const Pb::Register _RP_normalizeSumTo("", "normalizeSumTo", _W_2);
extern "C" {
void PbRegister_normalizeSumTo()
{
  KEEP_UNUSED(_RP_normalizeSumTo);
}
}

/******************************************************************************
 *
 * implicit time integration
 *
 ******************************************************************************/

//! Kernel: Construct the right-hand side of the poisson equation

struct MakeRhsWE : public KernelBase {
  MakeRhsWE(const FlagGrid &flags,
            Grid<Real> &rhs,
            const Grid<Real> &ut,
            const Grid<Real> &utm1,
            Real s,
            bool crankNic = false)
      : KernelBase(&flags, 1), flags(flags), rhs(rhs), ut(ut), utm1(utm1), s(s), crankNic(crankNic)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &rhs,
                 const Grid<Real> &ut,
                 const Grid<Real> &utm1,
                 Real s,
                 bool crankNic = false) const
  {
    rhs(i, j, k) = (2. * ut(i, j, k) - utm1(i, j, k));
    if (crankNic) {
      rhs(i, j, k) += s * (-4. * ut(i, j, k) + 1. * ut(i - 1, j, k) + 1. * ut(i + 1, j, k) +
                           1. * ut(i, j - 1, k) + 1. * ut(i, j + 1, k));
    }
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
  inline const Grid<Real> &getArg2()
  {
    return ut;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> &getArg3()
  {
    return utm1;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return s;
  }
  typedef Real type4;
  inline bool &getArg5()
  {
    return crankNic;
  }
  typedef bool type5;
  void runMessage()
  {
    debMsg("Executing kernel MakeRhsWE ", 3);
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
            op(i, j, k, flags, rhs, ut, utm1, s, crankNic);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, rhs, ut, utm1, s, crankNic);
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
  Grid<Real> &rhs;
  const Grid<Real> &ut;
  const Grid<Real> &utm1;
  Real s;
  bool crankNic;
};

//! do a CG solve for the wave equation (note, out grid only there for debugging... could be
//! removed)

void cgSolveWE(const FlagGrid &flags,
               Grid<Real> &ut,
               Grid<Real> &utm1,
               Grid<Real> &out,
               bool crankNic = false,
               Real cSqr = 0.25,
               Real cgMaxIterFac = 1.5,
               Real cgAccuracy = 1e-5)
{
  // reserve temp grids
  FluidSolver *parent = flags.getParent();
  Grid<Real> rhs(parent);
  Grid<Real> residual(parent);
  Grid<Real> search(parent);
  Grid<Real> A0(parent);
  Grid<Real> Ai(parent);
  Grid<Real> Aj(parent);
  Grid<Real> Ak(parent);
  Grid<Real> tmp(parent);
  // solution...
  out.clear();

  // setup matrix and boundaries
  MakeLaplaceMatrix(flags, A0, Ai, Aj, Ak);
  Real dt = parent->getDt();
  Real s = dt * dt * cSqr * 0.5;
  FOR_IJK(flags)
  {
    Ai(i, j, k) *= s;
    Aj(i, j, k) *= s;
    Ak(i, j, k) *= s;
    A0(i, j, k) *= s;
    A0(i, j, k) += 1.;
  }

  // compute divergence and init right hand side
  rhs.clear();
  // h=dt
  // rhs:   = 2 ut - ut-1
  // A:    (h2 c2/ dx)=s   ,  (1+4s)uij + s ui-1j + ...
  // Cr.Nic.
  // rhs:  cr nic = 2 ut - ut-1 + h^2c^2/2 b
  // A:    (h2 c2/2 dx)=s   ,  (1+4s)uij + s ui-1j + ...
  MakeRhsWE kernMakeRhs(flags, rhs, ut, utm1, s, crankNic);

  const int maxIter = (int)(cgMaxIterFac * flags.getSize().max()) * (flags.is3D() ? 1 : 4);
  GridCgInterface *gcg;
  vector<Grid<Real> *> matA{&A0, &Ai, &Aj};

  if (flags.is3D()) {
    matA.push_back(&Ak);
    gcg = new GridCg<ApplyMatrix>(out, rhs, residual, search, flags, tmp, matA);
  }
  else {
    gcg = new GridCg<ApplyMatrix2D>(out, rhs, residual, search, flags, tmp, matA);
  }

  gcg->setAccuracy(cgAccuracy);

  // no preconditioning for now...
  for (int iter = 0; iter < maxIter; iter++) {
    if (!gcg->iterate())
      iter = maxIter;
  }
  debMsg("cgSolveWaveEq iterations:" << gcg->getIterations() << ", res:" << gcg->getSigma(), 1);

  utm1.swap(ut);
  ut.copyFrom(out);

  delete gcg;
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "cgSolveWE", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &ut = *_args.getPtr<Grid<Real>>("ut", 1, &_lock);
      Grid<Real> &utm1 = *_args.getPtr<Grid<Real>>("utm1", 2, &_lock);
      Grid<Real> &out = *_args.getPtr<Grid<Real>>("out", 3, &_lock);
      bool crankNic = _args.getOpt<bool>("crankNic", 4, false, &_lock);
      Real cSqr = _args.getOpt<Real>("cSqr", 5, 0.25, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 6, 1.5, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 7, 1e-5, &_lock);
      _retval = getPyNone();
      cgSolveWE(flags, ut, utm1, out, crankNic, cSqr, cgMaxIterFac, cgAccuracy);
      _args.check();
    }
    pbFinalizePlugin(parent, "cgSolveWE", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("cgSolveWE", e.what());
    return 0;
  }
}
static const Pb::Register _RP_cgSolveWE("", "cgSolveWE", _W_3);
extern "C" {
void PbRegister_cgSolveWE()
{
  KEEP_UNUSED(_RP_cgSolveWE);
}
}

}  // namespace Manta
