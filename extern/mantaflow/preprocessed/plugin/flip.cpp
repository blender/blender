

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
 * FLIP (fluid implicit particles)
 * for use with particle data fields
 *
 ******************************************************************************/

#include "particle.h"
#include "general.h"
#include "grid.h"
#include "commonkernels.h"
#include "randomstream.h"
#include "levelset.h"
#include "shapes.h"
#include "matrixbase.h"

using namespace std;
namespace Manta {

// init

//! note - this is a simplified version , sampleLevelsetWithParticles has more functionality

void sampleFlagsWithParticles(const FlagGrid &flags,
                              BasicParticleSystem &parts,
                              const int discretization,
                              const Real randomness)
{
  const bool is3D = flags.is3D();
  const Real jlen = randomness / discretization;
  const Vec3 disp(1.0 / discretization, 1.0 / discretization, 1.0 / discretization);
  RandomStream mRand(9832);

  FOR_IJK_BND(flags, 0)
  {
    if (flags.isObstacle(i, j, k))
      continue;
    if (flags.isFluid(i, j, k)) {
      const Vec3 pos(i, j, k);
      for (int dk = 0; dk < (is3D ? discretization : 1); dk++)
        for (int dj = 0; dj < discretization; dj++)
          for (int di = 0; di < discretization; di++) {
            Vec3 subpos = pos + disp * Vec3(0.5 + di, 0.5 + dj, 0.5 + dk);
            subpos += jlen * (Vec3(1, 1, 1) - 2.0 * mRand.getVec3());
            if (!is3D)
              subpos[2] = 0.5;
            parts.addBuffered(subpos);
          }
    }
  }
  parts.insertBufferedParticles();
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "sampleFlagsWithParticles", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 1, &_lock);
      const int discretization = _args.get<int>("discretization", 2, &_lock);
      const Real randomness = _args.get<Real>("randomness", 3, &_lock);
      _retval = getPyNone();
      sampleFlagsWithParticles(flags, parts, discretization, randomness);
      _args.check();
    }
    pbFinalizePlugin(parent, "sampleFlagsWithParticles", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("sampleFlagsWithParticles", e.what());
    return 0;
  }
}
static const Pb::Register _RP_sampleFlagsWithParticles("", "sampleFlagsWithParticles", _W_0);
extern "C" {
void PbRegister_sampleFlagsWithParticles()
{
  KEEP_UNUSED(_RP_sampleFlagsWithParticles);
}
}

//! sample a level set with particles, use reset to clear the particle buffer,
//! and skipEmpty for a continuous inflow (in the latter case, only empty cells will
//! be re-filled once they empty when calling sampleLevelsetWithParticles during
//! the main loop).

void sampleLevelsetWithParticles(const LevelsetGrid &phi,
                                 const FlagGrid &flags,
                                 BasicParticleSystem &parts,
                                 const int discretization,
                                 const Real randomness,
                                 const bool reset = false,
                                 const bool refillEmpty = false,
                                 const int particleFlag = -1)
{
  const bool is3D = phi.is3D();
  const Real jlen = randomness / discretization;
  const Vec3 disp(1.0 / discretization, 1.0 / discretization, 1.0 / discretization);
  RandomStream mRand(9832);

  if (reset) {
    parts.clear();
    parts.doCompress();
  }

  FOR_IJK_BND(phi, 0)
  {
    if (flags.isObstacle(i, j, k))
      continue;
    if (refillEmpty && flags.isFluid(i, j, k))
      continue;
    if (phi(i, j, k) < 1.733) {
      const Vec3 pos(i, j, k);
      for (int dk = 0; dk < (is3D ? discretization : 1); dk++)
        for (int dj = 0; dj < discretization; dj++)
          for (int di = 0; di < discretization; di++) {
            Vec3 subpos = pos + disp * Vec3(0.5 + di, 0.5 + dj, 0.5 + dk);
            subpos += jlen * (Vec3(1, 1, 1) - 2.0 * mRand.getVec3());
            if (!is3D)
              subpos[2] = 0.5;
            if (phi.getInterpolated(subpos) > 0.)
              continue;
            if (particleFlag < 0) {
              parts.addBuffered(subpos);
            }
            else {
              parts.addBuffered(subpos, particleFlag);
            }
          }
    }
  }

  parts.insertBufferedParticles();
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "sampleLevelsetWithParticles", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const LevelsetGrid &phi = *_args.getPtr<LevelsetGrid>("phi", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      const int discretization = _args.get<int>("discretization", 3, &_lock);
      const Real randomness = _args.get<Real>("randomness", 4, &_lock);
      const bool reset = _args.getOpt<bool>("reset", 5, false, &_lock);
      const bool refillEmpty = _args.getOpt<bool>("refillEmpty", 6, false, &_lock);
      const int particleFlag = _args.getOpt<int>("particleFlag", 7, -1, &_lock);
      _retval = getPyNone();
      sampleLevelsetWithParticles(
          phi, flags, parts, discretization, randomness, reset, refillEmpty, particleFlag);
      _args.check();
    }
    pbFinalizePlugin(parent, "sampleLevelsetWithParticles", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("sampleLevelsetWithParticles", e.what());
    return 0;
  }
}
static const Pb::Register _RP_sampleLevelsetWithParticles("", "sampleLevelsetWithParticles", _W_1);
extern "C" {
void PbRegister_sampleLevelsetWithParticles()
{
  KEEP_UNUSED(_RP_sampleLevelsetWithParticles);
}
}

//! sample a shape with particles, use reset to clear the particle buffer,
//! and skipEmpty for a continuous inflow (in the latter case, only empty cells will
//! be re-filled once they empty when calling sampleShapeWithParticles during
//! the main loop).

void sampleShapeWithParticles(const Shape &shape,
                              const FlagGrid &flags,
                              BasicParticleSystem &parts,
                              const int discretization,
                              const Real randomness,
                              const bool reset = false,
                              const bool refillEmpty = false,
                              const LevelsetGrid *exclude = nullptr)
{
  const bool is3D = flags.is3D();
  const Real jlen = randomness / discretization;
  const Vec3 disp(1.0 / discretization, 1.0 / discretization, 1.0 / discretization);
  RandomStream mRand(9832);

  if (reset) {
    parts.clear();
    parts.doCompress();
  }

  FOR_IJK_BND(flags, 0)
  {
    if (flags.isObstacle(i, j, k))
      continue;
    if (refillEmpty && flags.isFluid(i, j, k))
      continue;
    const Vec3 pos(i, j, k);
    for (int dk = 0; dk < (is3D ? discretization : 1); dk++)
      for (int dj = 0; dj < discretization; dj++)
        for (int di = 0; di < discretization; di++) {
          Vec3 subpos = pos + disp * Vec3(0.5 + di, 0.5 + dj, 0.5 + dk);
          subpos += jlen * (Vec3(1, 1, 1) - 2.0 * mRand.getVec3());
          if (!is3D)
            subpos[2] = 0.5;
          if (exclude && exclude->getInterpolated(subpos) <= 0.)
            continue;
          if (!shape.isInside(subpos))
            continue;
          parts.addBuffered(subpos);
        }
  }

  parts.insertBufferedParticles();
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "sampleShapeWithParticles", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const Shape &shape = *_args.getPtr<Shape>("shape", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      const int discretization = _args.get<int>("discretization", 3, &_lock);
      const Real randomness = _args.get<Real>("randomness", 4, &_lock);
      const bool reset = _args.getOpt<bool>("reset", 5, false, &_lock);
      const bool refillEmpty = _args.getOpt<bool>("refillEmpty", 6, false, &_lock);
      const LevelsetGrid *exclude = _args.getPtrOpt<LevelsetGrid>("exclude", 7, nullptr, &_lock);
      _retval = getPyNone();
      sampleShapeWithParticles(
          shape, flags, parts, discretization, randomness, reset, refillEmpty, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "sampleShapeWithParticles", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("sampleShapeWithParticles", e.what());
    return 0;
  }
}
static const Pb::Register _RP_sampleShapeWithParticles("", "sampleShapeWithParticles", _W_2);
extern "C" {
void PbRegister_sampleShapeWithParticles()
{
  KEEP_UNUSED(_RP_sampleShapeWithParticles);
}
}

//! mark fluid cells and helpers
struct knClearFluidFlags : public KernelBase {
  knClearFluidFlags(FlagGrid &flags, int dummy = 0)
      : KernelBase(&flags, 0), flags(flags), dummy(dummy)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, FlagGrid &flags, int dummy = 0) const
  {
    if (flags.isFluid(i, j, k)) {
      flags(i, j, k) = (flags(i, j, k) | FlagGrid::TypeEmpty) & ~FlagGrid::TypeFluid;
    }
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline int &getArg1()
  {
    return dummy;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel knClearFluidFlags ", 3);
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
            op(i, j, k, flags, dummy);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, dummy);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  FlagGrid &flags;
  int dummy;
};

struct knSetNbObstacle : public KernelBase {
  knSetNbObstacle(FlagGrid &nflags, const FlagGrid &flags, const Grid<Real> &phiObs)
      : KernelBase(&nflags, 1), nflags(nflags), flags(flags), phiObs(phiObs)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, FlagGrid &nflags, const FlagGrid &flags, const Grid<Real> &phiObs) const
  {
    if (phiObs(i, j, k) > 0.)
      return;
    if (flags.isEmpty(i, j, k)) {
      bool set = false;
      if ((flags.isFluid(i - 1, j, k)) && (phiObs(i + 1, j, k) <= 0.))
        set = true;
      if ((flags.isFluid(i + 1, j, k)) && (phiObs(i - 1, j, k) <= 0.))
        set = true;
      if ((flags.isFluid(i, j - 1, k)) && (phiObs(i, j + 1, k) <= 0.))
        set = true;
      if ((flags.isFluid(i, j + 1, k)) && (phiObs(i, j - 1, k) <= 0.))
        set = true;
      if (flags.is3D()) {
        if ((flags.isFluid(i, j, k - 1)) && (phiObs(i, j, k + 1) <= 0.))
          set = true;
        if ((flags.isFluid(i, j, k + 1)) && (phiObs(i, j, k - 1) <= 0.))
          set = true;
      }
      if (set)
        nflags(i, j, k) = (flags(i, j, k) | FlagGrid::TypeFluid) & ~FlagGrid::TypeEmpty;
    }
  }
  inline FlagGrid &getArg0()
  {
    return nflags;
  }
  typedef FlagGrid type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return phiObs;
  }
  typedef Grid<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetNbObstacle ", 3);
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
            op(i, j, k, nflags, flags, phiObs);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, nflags, flags, phiObs);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  FlagGrid &nflags;
  const FlagGrid &flags;
  const Grid<Real> &phiObs;
};
void markFluidCells(const BasicParticleSystem &parts,
                    FlagGrid &flags,
                    const Grid<Real> *phiObs = nullptr,
                    const ParticleDataImpl<int> *ptype = nullptr,
                    const int exclude = 0)
{
  // remove all fluid cells
  knClearFluidFlags(flags, 0);

  // mark all particles in flaggrid as fluid
  for (IndexInt idx = 0; idx < parts.size(); idx++) {
    if (!parts.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      continue;
    Vec3i p = toVec3i(parts.getPos(idx));
    if (flags.isInBounds(p) && flags.isEmpty(p))
      flags(p) = (flags(p) | FlagGrid::TypeFluid) & ~FlagGrid::TypeEmpty;
  }

  // special for second order obstacle BCs, check empty cells in boundary region
  if (phiObs) {
    FlagGrid tmp(flags);
    knSetNbObstacle(tmp, flags, *phiObs);
    flags.swap(tmp);
  }
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "markFluidCells", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const Grid<Real> *phiObs = _args.getPtrOpt<Grid<Real>>("phiObs", 2, nullptr, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 3, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 4, 0, &_lock);
      _retval = getPyNone();
      markFluidCells(parts, flags, phiObs, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "markFluidCells", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("markFluidCells", e.what());
    return 0;
  }
}
static const Pb::Register _RP_markFluidCells("", "markFluidCells", _W_3);
extern "C" {
void PbRegister_markFluidCells()
{
  KEEP_UNUSED(_RP_markFluidCells);
}
}

// for testing purposes only...
void testInitGridWithPos(Grid<Real> &grid)
{
  FOR_IJK(grid)
  {
    grid(i, j, k) = norm(Vec3(i, j, k));
  }
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "testInitGridWithPos", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &grid = *_args.getPtr<Grid<Real>>("grid", 0, &_lock);
      _retval = getPyNone();
      testInitGridWithPos(grid);
      _args.check();
    }
    pbFinalizePlugin(parent, "testInitGridWithPos", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("testInitGridWithPos", e.what());
    return 0;
  }
}
static const Pb::Register _RP_testInitGridWithPos("", "testInitGridWithPos", _W_4);
extern "C" {
void PbRegister_testInitGridWithPos()
{
  KEEP_UNUSED(_RP_testInitGridWithPos);
}
}

//! helper to calculate particle radius factor to cover the diagonal of a cell in 2d/3d
inline Real calculateRadiusFactor(const Grid<Real> &grid, Real factor)
{
  return (grid.is3D() ? sqrt(3.) : sqrt(2.)) *
         (factor + .01);  // note, a 1% safety factor is added here
}

//! re-sample particles based on an input levelset
// optionally skip seeding new particles in "exclude" SDF

void adjustNumber(BasicParticleSystem &parts,
                  const MACGrid &vel,
                  const FlagGrid &flags,
                  int minParticles,
                  int maxParticles,
                  const LevelsetGrid &phi,
                  Real radiusFactor = 1.,
                  Real narrowBand = -1.,
                  const Grid<Real> *exclude = nullptr)
{
  // which levelset to use as threshold
  const Real SURFACE_LS = -1.0 * calculateRadiusFactor(phi, radiusFactor);
  Grid<int> tmp(vel.getParent());
  std::ostringstream out;

  // count particles in cells, and delete excess particles
  for (IndexInt idx = 0; idx < (int)parts.size(); idx++) {
    if (parts.isActive(idx)) {
      Vec3i p = toVec3i(parts.getPos(idx));
      if (!tmp.isInBounds(p)) {
        parts.kill(idx);  // out of domain, remove
        continue;
      }

      Real phiv = phi.getInterpolated(parts.getPos(idx));
      if (phiv > 0) {
        parts.kill(idx);
        continue;
      }
      if (narrowBand > 0. && phiv < -narrowBand) {
        parts.kill(idx);
        continue;
      }

      bool atSurface = false;
      if (phiv > SURFACE_LS)
        atSurface = true;
      int num = tmp(p);

      // dont delete particles in non fluid cells here, the particles are "always right"
      if (num > maxParticles && (!atSurface)) {
        parts.kill(idx);
      }
      else {
        tmp(p) = num + 1;
      }
    }
  }

  // seed new particles
  RandomStream mRand(9832);
  FOR_IJK(tmp)
  {
    int cnt = tmp(i, j, k);

    // skip cells near surface
    if (phi(i, j, k) > SURFACE_LS)
      continue;
    if (narrowBand > 0. && phi(i, j, k) < -narrowBand) {
      continue;
    }
    if (exclude && ((*exclude)(i, j, k) < 0.)) {
      continue;
    }

    if (flags.isFluid(i, j, k) && cnt < minParticles) {
      for (int m = cnt; m < minParticles; m++) {
        Vec3 pos = Vec3(i, j, k) + mRand.getVec3();
        // Vec3 pos (i + 0.5, j + 0.5, k + 0.5); // cell center
        parts.addBuffered(pos);
      }
    }
  }

  parts.doCompress();
  parts.insertBufferedParticles();
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "adjustNumber", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      int minParticles = _args.get<int>("minParticles", 3, &_lock);
      int maxParticles = _args.get<int>("maxParticles", 4, &_lock);
      const LevelsetGrid &phi = *_args.getPtr<LevelsetGrid>("phi", 5, &_lock);
      Real radiusFactor = _args.getOpt<Real>("radiusFactor", 6, 1., &_lock);
      Real narrowBand = _args.getOpt<Real>("narrowBand", 7, -1., &_lock);
      const Grid<Real> *exclude = _args.getPtrOpt<Grid<Real>>("exclude", 8, nullptr, &_lock);
      _retval = getPyNone();
      adjustNumber(
          parts, vel, flags, minParticles, maxParticles, phi, radiusFactor, narrowBand, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "adjustNumber", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("adjustNumber", e.what());
    return 0;
  }
}
static const Pb::Register _RP_adjustNumber("", "adjustNumber", _W_5);
extern "C" {
void PbRegister_adjustNumber()
{
  KEEP_UNUSED(_RP_adjustNumber);
}
}

// simple and slow helper conversion to show contents of int grids like a real grid in the ui
// (use eg to quickly display contents of the particle-index grid)

void debugIntToReal(const Grid<int> &source, Grid<Real> &dest, Real factor = 1.)
{
  FOR_IJK(source)
  {
    dest(i, j, k) = (Real)source(i, j, k) * factor;
  }
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "debugIntToReal", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const Grid<int> &source = *_args.getPtr<Grid<int>>("source", 0, &_lock);
      Grid<Real> &dest = *_args.getPtr<Grid<Real>>("dest", 1, &_lock);
      Real factor = _args.getOpt<Real>("factor", 2, 1., &_lock);
      _retval = getPyNone();
      debugIntToReal(source, dest, factor);
      _args.check();
    }
    pbFinalizePlugin(parent, "debugIntToReal", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("debugIntToReal", e.what());
    return 0;
  }
}
static const Pb::Register _RP_debugIntToReal("", "debugIntToReal", _W_6);
extern "C" {
void PbRegister_debugIntToReal()
{
  KEEP_UNUSED(_RP_debugIntToReal);
}
}

// build a grid that contains indices for a particle system
// the particles in a cell i,j,k are particles[index(i,j,k)] to particles[index(i+1,j,k)-1]
// (ie,  particles[index(i+1,j,k)] already belongs to cell i+1,j,k)

void gridParticleIndex(const BasicParticleSystem &parts,
                       ParticleIndexSystem &indexSys,
                       const FlagGrid &flags,
                       Grid<int> &index,
                       Grid<int> *counter = nullptr)
{
  bool delCounter = false;
  if (!counter) {
    counter = new Grid<int>(flags.getParent());
    delCounter = true;
  }
  else {
    counter->clear();
  }

  // count particles in cells, and delete excess particles
  index.clear();
  int inactive = 0;
  for (IndexInt idx = 0; idx < (IndexInt)parts.size(); idx++) {
    if (parts.isActive(idx)) {
      // check index for validity...
      Vec3i p = toVec3i(parts.getPos(idx));
      if (!index.isInBounds(p)) {
        inactive++;
        continue;
      }

      index(p)++;
    }
    else {
      inactive++;
    }
  }

  // note - this one might be smaller...
  indexSys.resize(parts.size() - inactive);

  // convert per cell number to continuous index
  IndexInt idx = 0;
  FOR_IJK(index)
  {
    int num = index(i, j, k);
    index(i, j, k) = idx;
    idx += num;
  }

  // add particles to indexed array, we still need a per cell particle counter
  for (IndexInt idx = 0; idx < (IndexInt)parts.size(); idx++) {
    if (!parts.isActive(idx))
      continue;
    Vec3i p = toVec3i(parts.getPos(idx));
    if (!index.isInBounds(p)) {
      continue;
    }

    // initialize position and index into original array
    // indexSys[ index(p)+(*counter)(p) ].pos        = parts[idx].pos;
    indexSys[index(p) + (*counter)(p)].sourceIndex = idx;
    (*counter)(p)++;
  }

  if (delCounter)
    delete counter;
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "gridParticleIndex", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleIndexSystem &indexSys = *_args.getPtr<ParticleIndexSystem>("indexSys", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      Grid<int> &index = *_args.getPtr<Grid<int>>("index", 3, &_lock);
      Grid<int> *counter = _args.getPtrOpt<Grid<int>>("counter", 4, nullptr, &_lock);
      _retval = getPyNone();
      gridParticleIndex(parts, indexSys, flags, index, counter);
      _args.check();
    }
    pbFinalizePlugin(parent, "gridParticleIndex", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("gridParticleIndex", e.what());
    return 0;
  }
}
static const Pb::Register _RP_gridParticleIndex("", "gridParticleIndex", _W_7);
extern "C" {
void PbRegister_gridParticleIndex()
{
  KEEP_UNUSED(_RP_gridParticleIndex);
}
}

struct ComputeUnionLevelsetPindex : public KernelBase {
  ComputeUnionLevelsetPindex(const Grid<int> &index,
                             const BasicParticleSystem &parts,
                             const ParticleIndexSystem &indexSys,
                             LevelsetGrid &phi,
                             const Real radius,
                             const ParticleDataImpl<int> *ptype,
                             const int exclude)
      : KernelBase(&index, 0),
        index(index),
        parts(parts),
        indexSys(indexSys),
        phi(phi),
        radius(radius),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const Grid<int> &index,
                 const BasicParticleSystem &parts,
                 const ParticleIndexSystem &indexSys,
                 LevelsetGrid &phi,
                 const Real radius,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    const Vec3 gridPos = Vec3(i, j, k) + Vec3(0.5);  // shifted by half cell
    Real phiv = radius * 1.0;                        // outside

    int r = int(radius) + 1;
    int rZ = phi.is3D() ? r : 0;
    for (int zj = k - rZ; zj <= k + rZ; zj++)
      for (int yj = j - r; yj <= j + r; yj++)
        for (int xj = i - r; xj <= i + r; xj++) {
          if (!phi.isInBounds(Vec3i(xj, yj, zj)))
            continue;

          // note, for the particle indices in indexSys the access is periodic (ie, dont skip for
          // eg inBounds(sx,10,10)
          IndexInt isysIdxS = index.index(xj, yj, zj);
          IndexInt pStart = index(isysIdxS), pEnd = 0;
          if (phi.isInBounds(isysIdxS + 1))
            pEnd = index(isysIdxS + 1);
          else
            pEnd = indexSys.size();

          // now loop over particles in cell
          for (IndexInt p = pStart; p < pEnd; ++p) {
            const int psrc = indexSys[p].sourceIndex;
            if (ptype && ((*ptype)[psrc] & exclude))
              continue;
            const Vec3 pos = parts[psrc].pos;
            phiv = std::min(phiv, fabs(norm(gridPos - pos)) - radius);
          }
        }
    phi(i, j, k) = phiv;
  }
  inline const Grid<int> &getArg0()
  {
    return index;
  }
  typedef Grid<int> type0;
  inline const BasicParticleSystem &getArg1()
  {
    return parts;
  }
  typedef BasicParticleSystem type1;
  inline const ParticleIndexSystem &getArg2()
  {
    return indexSys;
  }
  typedef ParticleIndexSystem type2;
  inline LevelsetGrid &getArg3()
  {
    return phi;
  }
  typedef LevelsetGrid type3;
  inline const Real &getArg4()
  {
    return radius;
  }
  typedef Real type4;
  inline const ParticleDataImpl<int> *getArg5()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type5;
  inline const int &getArg6()
  {
    return exclude;
  }
  typedef int type6;
  void runMessage()
  {
    debMsg("Executing kernel ComputeUnionLevelsetPindex ", 3);
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
            op(i, j, k, index, parts, indexSys, phi, radius, ptype, exclude);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, index, parts, indexSys, phi, radius, ptype, exclude);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const Grid<int> &index;
  const BasicParticleSystem &parts;
  const ParticleIndexSystem &indexSys;
  LevelsetGrid &phi;
  const Real radius;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

void unionParticleLevelset(const BasicParticleSystem &parts,
                           const ParticleIndexSystem &indexSys,
                           const FlagGrid &flags,
                           const Grid<int> &index,
                           LevelsetGrid &phi,
                           const Real radiusFactor = 1.,
                           const ParticleDataImpl<int> *ptype = nullptr,
                           const int exclude = 0)
{
  // use half a cell diagonal as base radius
  const Real radius = 0.5 * calculateRadiusFactor(phi, radiusFactor);
  // no reset of phi necessary here
  ComputeUnionLevelsetPindex(index, parts, indexSys, phi, radius, ptype, exclude);

  phi.setBound(0.5, 0);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "unionParticleLevelset", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const ParticleIndexSystem &indexSys = *_args.getPtr<ParticleIndexSystem>(
          "indexSys", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      const Grid<int> &index = *_args.getPtr<Grid<int>>("index", 3, &_lock);
      LevelsetGrid &phi = *_args.getPtr<LevelsetGrid>("phi", 4, &_lock);
      const Real radiusFactor = _args.getOpt<Real>("radiusFactor", 5, 1., &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 6, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 7, 0, &_lock);
      _retval = getPyNone();
      unionParticleLevelset(parts, indexSys, flags, index, phi, radiusFactor, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "unionParticleLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("unionParticleLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_unionParticleLevelset("", "unionParticleLevelset", _W_8);
extern "C" {
void PbRegister_unionParticleLevelset()
{
  KEEP_UNUSED(_RP_unionParticleLevelset);
}
}

//! kernel for computing averaged particle level set weights

struct ComputeAveragedLevelsetWeight : public KernelBase {
  ComputeAveragedLevelsetWeight(const BasicParticleSystem &parts,
                                const Grid<int> &index,
                                const ParticleIndexSystem &indexSys,
                                LevelsetGrid &phi,
                                const Real radius,
                                const ParticleDataImpl<int> *ptype,
                                const int exclude,
                                Grid<Vec3> *save_pAcc = nullptr,
                                Grid<Real> *save_rAcc = nullptr)
      : KernelBase(&index, 0),
        parts(parts),
        index(index),
        indexSys(indexSys),
        phi(phi),
        radius(radius),
        ptype(ptype),
        exclude(exclude),
        save_pAcc(save_pAcc),
        save_rAcc(save_rAcc)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const BasicParticleSystem &parts,
                 const Grid<int> &index,
                 const ParticleIndexSystem &indexSys,
                 LevelsetGrid &phi,
                 const Real radius,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude,
                 Grid<Vec3> *save_pAcc = nullptr,
                 Grid<Real> *save_rAcc = nullptr) const
  {
    const Vec3 gridPos = Vec3(i, j, k) + Vec3(0.5);  // shifted by half cell
    Real phiv = radius * 1.0;                        // outside

    // loop over neighborhood, similar to ComputeUnionLevelsetPindex
    const Real sradiusInv = 1. / (4. * radius * radius);
    int r = int(1. * radius) + 1;
    int rZ = phi.is3D() ? r : 0;
    // accumulators
    Real wacc = 0.;
    Vec3 pacc = Vec3(0.);
    Real racc = 0.;

    for (int zj = k - rZ; zj <= k + rZ; zj++)
      for (int yj = j - r; yj <= j + r; yj++)
        for (int xj = i - r; xj <= i + r; xj++) {
          if (!phi.isInBounds(Vec3i(xj, yj, zj)))
            continue;

          IndexInt isysIdxS = index.index(xj, yj, zj);
          IndexInt pStart = index(isysIdxS), pEnd = 0;
          if (phi.isInBounds(isysIdxS + 1))
            pEnd = index(isysIdxS + 1);
          else
            pEnd = indexSys.size();
          for (IndexInt p = pStart; p < pEnd; ++p) {
            IndexInt psrc = indexSys[p].sourceIndex;
            if (ptype && ((*ptype)[psrc] & exclude))
              continue;

            Vec3 pos = parts[psrc].pos;
            Real s = normSquare(gridPos - pos) * sradiusInv;
            // Real  w = std::max(0., cubed(1.-s) );
            Real w = std::max(0., (1. - s));  // a bit smoother
            wacc += w;
            racc += radius * w;
            pacc += pos * w;
          }
        }

    if (wacc > VECTOR_EPSILON) {
      racc /= wacc;
      pacc /= wacc;
      phiv = fabs(norm(gridPos - pacc)) - racc;

      if (save_pAcc)
        (*save_pAcc)(i, j, k) = pacc;
      if (save_rAcc)
        (*save_rAcc)(i, j, k) = racc;
    }
    phi(i, j, k) = phiv;
  }
  inline const BasicParticleSystem &getArg0()
  {
    return parts;
  }
  typedef BasicParticleSystem type0;
  inline const Grid<int> &getArg1()
  {
    return index;
  }
  typedef Grid<int> type1;
  inline const ParticleIndexSystem &getArg2()
  {
    return indexSys;
  }
  typedef ParticleIndexSystem type2;
  inline LevelsetGrid &getArg3()
  {
    return phi;
  }
  typedef LevelsetGrid type3;
  inline const Real &getArg4()
  {
    return radius;
  }
  typedef Real type4;
  inline const ParticleDataImpl<int> *getArg5()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type5;
  inline const int &getArg6()
  {
    return exclude;
  }
  typedef int type6;
  inline Grid<Vec3> *getArg7()
  {
    return save_pAcc;
  }
  typedef Grid<Vec3> type7;
  inline Grid<Real> *getArg8()
  {
    return save_rAcc;
  }
  typedef Grid<Real> type8;
  void runMessage()
  {
    debMsg("Executing kernel ComputeAveragedLevelsetWeight ", 3);
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
            op(i, j, k, parts, index, indexSys, phi, radius, ptype, exclude, save_pAcc, save_rAcc);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, parts, index, indexSys, phi, radius, ptype, exclude, save_pAcc, save_rAcc);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const BasicParticleSystem &parts;
  const Grid<int> &index;
  const ParticleIndexSystem &indexSys;
  LevelsetGrid &phi;
  const Real radius;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
  Grid<Vec3> *save_pAcc;
  Grid<Real> *save_rAcc;
};

template<class T> T smoothingValue(const Grid<T> val, int i, int j, int k, T center)
{
  return val(i, j, k);
}

// smoothing, and

template<class T> struct knSmoothGrid : public KernelBase {
  knSmoothGrid(const Grid<T> &me, Grid<T> &tmp, Real factor)
      : KernelBase(&me, 1), me(me), tmp(tmp), factor(factor)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<T> &me, Grid<T> &tmp, Real factor) const
  {
    T val = me(i, j, k) + me(i + 1, j, k) + me(i - 1, j, k) + me(i, j + 1, k) + me(i, j - 1, k);
    if (me.is3D()) {
      val += me(i, j, k + 1) + me(i, j, k - 1);
    }
    tmp(i, j, k) = val * factor;
  }
  inline const Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline Grid<T> &getArg1()
  {
    return tmp;
  }
  typedef Grid<T> type1;
  inline Real &getArg2()
  {
    return factor;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel knSmoothGrid ", 3);
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
            op(i, j, k, me, tmp, factor);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, me, tmp, factor);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const Grid<T> &me;
  Grid<T> &tmp;
  Real factor;
};

template<class T> struct knSmoothGridNeg : public KernelBase {
  knSmoothGridNeg(const Grid<T> &me, Grid<T> &tmp, Real factor)
      : KernelBase(&me, 1), me(me), tmp(tmp), factor(factor)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, const Grid<T> &me, Grid<T> &tmp, Real factor) const
  {
    T val = me(i, j, k) + me(i + 1, j, k) + me(i - 1, j, k) + me(i, j + 1, k) + me(i, j - 1, k);
    if (me.is3D()) {
      val += me(i, j, k + 1) + me(i, j, k - 1);
    }
    val *= factor;
    if (val < tmp(i, j, k))
      tmp(i, j, k) = val;
    else
      tmp(i, j, k) = me(i, j, k);
  }
  inline const Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline Grid<T> &getArg1()
  {
    return tmp;
  }
  typedef Grid<T> type1;
  inline Real &getArg2()
  {
    return factor;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel knSmoothGridNeg ", 3);
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
            op(i, j, k, me, tmp, factor);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, me, tmp, factor);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const Grid<T> &me;
  Grid<T> &tmp;
  Real factor;
};

//! Zhu & Bridson particle level set creation

void averagedParticleLevelset(const BasicParticleSystem &parts,
                              const ParticleIndexSystem &indexSys,
                              const FlagGrid &flags,
                              const Grid<int> &index,
                              LevelsetGrid &phi,
                              const Real radiusFactor = 1.,
                              const int smoothen = 1,
                              const int smoothenNeg = 1,
                              const ParticleDataImpl<int> *ptype = nullptr,
                              const int exclude = 0)
{
  // use half a cell diagonal as base radius
  const Real radius = 0.5 * calculateRadiusFactor(phi, radiusFactor);
  ComputeAveragedLevelsetWeight(parts, index, indexSys, phi, radius, ptype, exclude);

  // post-process level-set
  for (int i = 0; i < std::max(smoothen, smoothenNeg); ++i) {
    LevelsetGrid tmp(flags.getParent());
    if (i < smoothen) {
      knSmoothGrid<Real>(phi, tmp, 1. / (phi.is3D() ? 7. : 5.));
      phi.swap(tmp);
    }
    if (i < smoothenNeg) {
      knSmoothGridNeg<Real>(phi, tmp, 1. / (phi.is3D() ? 7. : 5.));
      phi.swap(tmp);
    }
  }
  phi.setBound(0.5, 0);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "averagedParticleLevelset", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const ParticleIndexSystem &indexSys = *_args.getPtr<ParticleIndexSystem>(
          "indexSys", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      const Grid<int> &index = *_args.getPtr<Grid<int>>("index", 3, &_lock);
      LevelsetGrid &phi = *_args.getPtr<LevelsetGrid>("phi", 4, &_lock);
      const Real radiusFactor = _args.getOpt<Real>("radiusFactor", 5, 1., &_lock);
      const int smoothen = _args.getOpt<int>("smoothen", 6, 1, &_lock);
      const int smoothenNeg = _args.getOpt<int>("smoothenNeg", 7, 1, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 8, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 9, 0, &_lock);
      _retval = getPyNone();
      averagedParticleLevelset(
          parts, indexSys, flags, index, phi, radiusFactor, smoothen, smoothenNeg, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "averagedParticleLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("averagedParticleLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_averagedParticleLevelset("", "averagedParticleLevelset", _W_9);
extern "C" {
void PbRegister_averagedParticleLevelset()
{
  KEEP_UNUSED(_RP_averagedParticleLevelset);
}
}

//! kernel for improvedParticleLevelset

struct correctLevelset : public KernelBase {
  correctLevelset(LevelsetGrid &phi,
                  const Grid<Vec3> &pAcc,
                  const Grid<Real> &rAcc,
                  const Real radius,
                  const Real t_low,
                  const Real t_high)
      : KernelBase(&phi, 1),
        phi(phi),
        pAcc(pAcc),
        rAcc(rAcc),
        radius(radius),
        t_low(t_low),
        t_high(t_high)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 LevelsetGrid &phi,
                 const Grid<Vec3> &pAcc,
                 const Grid<Real> &rAcc,
                 const Real radius,
                 const Real t_low,
                 const Real t_high) const
  {
    if (rAcc(i, j, k) <= VECTOR_EPSILON)
      return;  // outside nothing happens

    // create jacobian of pAcc via central differences
    Matrix3x3f jacobian = Matrix3x3f(0.5 * (pAcc(i + 1, j, k).x - pAcc(i - 1, j, k).x),
                                     0.5 * (pAcc(i, j + 1, k).x - pAcc(i, j - 1, k).x),
                                     0.5 * (pAcc(i, j, k + 1).x - pAcc(i, j, k - 1).x),
                                     0.5 * (pAcc(i + 1, j, k).y - pAcc(i - 1, j, k).y),
                                     0.5 * (pAcc(i, j + 1, k).y - pAcc(i, j - 1, k).y),
                                     0.5 * (pAcc(i, j, k + 1).y - pAcc(i, j, k - 1).y),
                                     0.5 * (pAcc(i + 1, j, k).z - pAcc(i - 1, j, k).z),
                                     0.5 * (pAcc(i, j + 1, k).z - pAcc(i, j - 1, k).z),
                                     0.5 * (pAcc(i, j, k + 1).z - pAcc(i, j, k - 1).z));

    // compute largest eigenvalue of jacobian
    Vec3 EV = jacobian.eigenvalues();
    Real maxEV = std::max(std::max(EV.x, EV.y), EV.z);

    // calculate correction factor
    Real correction = 1;
    if (maxEV >= t_low) {
      Real t = (t_high - maxEV) / (t_high - t_low);
      correction = t * t * t - 3 * t * t + 3 * t;
    }
    correction = clamp(correction,
                       Real(0),
                       Real(1));  // enforce correction factor to [0,1] (not explicitly in paper)

    const Vec3 gridPos = Vec3(i, j, k) + Vec3(0.5);  // shifted by half cell
    const Real correctedPhi = fabs(norm(gridPos - pAcc(i, j, k))) - rAcc(i, j, k) * correction;
    phi(i, j, k) = (correctedPhi > radius) ?
                       radius :
                       correctedPhi;  // adjust too high outside values when too few particles are
                                      // nearby to make smoothing possible (not in paper)
  }
  inline LevelsetGrid &getArg0()
  {
    return phi;
  }
  typedef LevelsetGrid type0;
  inline const Grid<Vec3> &getArg1()
  {
    return pAcc;
  }
  typedef Grid<Vec3> type1;
  inline const Grid<Real> &getArg2()
  {
    return rAcc;
  }
  typedef Grid<Real> type2;
  inline const Real &getArg3()
  {
    return radius;
  }
  typedef Real type3;
  inline const Real &getArg4()
  {
    return t_low;
  }
  typedef Real type4;
  inline const Real &getArg5()
  {
    return t_high;
  }
  typedef Real type5;
  void runMessage()
  {
    debMsg("Executing kernel correctLevelset ", 3);
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
            op(i, j, k, phi, pAcc, rAcc, radius, t_low, t_high);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, phi, pAcc, rAcc, radius, t_low, t_high);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  LevelsetGrid &phi;
  const Grid<Vec3> &pAcc;
  const Grid<Real> &rAcc;
  const Real radius;
  const Real t_low;
  const Real t_high;
};

//! Approach from "A unified particle model for fluid-solid interactions" by Solenthaler et al. in
//! 2007

void improvedParticleLevelset(const BasicParticleSystem &parts,
                              const ParticleIndexSystem &indexSys,
                              const FlagGrid &flags,
                              const Grid<int> &index,
                              LevelsetGrid &phi,
                              const Real radiusFactor = 1.,
                              const int smoothen = 1,
                              const int smoothenNeg = 1,
                              const Real t_low = 0.4,
                              const Real t_high = 3.5,
                              const ParticleDataImpl<int> *ptype = nullptr,
                              const int exclude = 0)
{
  // create temporary grids to store values from levelset weight computation
  Grid<Vec3> save_pAcc(flags.getParent());
  Grid<Real> save_rAcc(flags.getParent());

  const Real radius = 0.5 * calculateRadiusFactor(
                                phi, radiusFactor);  // use half a cell diagonal as base radius
  ComputeAveragedLevelsetWeight(
      parts, index, indexSys, phi, radius, ptype, exclude, &save_pAcc, &save_rAcc);
  correctLevelset(phi, save_pAcc, save_rAcc, radius, t_low, t_high);

  // post-process level-set
  for (int i = 0; i < std::max(smoothen, smoothenNeg); ++i) {
    LevelsetGrid tmp(flags.getParent());
    if (i < smoothen) {
      knSmoothGrid<Real>(phi, tmp, 1. / (phi.is3D() ? 7. : 5.));
      phi.swap(tmp);
    }
    if (i < smoothenNeg) {
      knSmoothGridNeg<Real>(phi, tmp, 1. / (phi.is3D() ? 7. : 5.));
      phi.swap(tmp);
    }
  }
  phi.setBound(0.5, 0);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "improvedParticleLevelset", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const ParticleIndexSystem &indexSys = *_args.getPtr<ParticleIndexSystem>(
          "indexSys", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      const Grid<int> &index = *_args.getPtr<Grid<int>>("index", 3, &_lock);
      LevelsetGrid &phi = *_args.getPtr<LevelsetGrid>("phi", 4, &_lock);
      const Real radiusFactor = _args.getOpt<Real>("radiusFactor", 5, 1., &_lock);
      const int smoothen = _args.getOpt<int>("smoothen", 6, 1, &_lock);
      const int smoothenNeg = _args.getOpt<int>("smoothenNeg", 7, 1, &_lock);
      const Real t_low = _args.getOpt<Real>("t_low", 8, 0.4, &_lock);
      const Real t_high = _args.getOpt<Real>("t_high", 9, 3.5, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 10, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 11, 0, &_lock);
      _retval = getPyNone();
      improvedParticleLevelset(parts,
                               indexSys,
                               flags,
                               index,
                               phi,
                               radiusFactor,
                               smoothen,
                               smoothenNeg,
                               t_low,
                               t_high,
                               ptype,
                               exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "improvedParticleLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("improvedParticleLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_improvedParticleLevelset("", "improvedParticleLevelset", _W_10);
extern "C" {
void PbRegister_improvedParticleLevelset()
{
  KEEP_UNUSED(_RP_improvedParticleLevelset);
}
}

struct knPushOutofObs : public KernelBase {
  knPushOutofObs(BasicParticleSystem &parts,
                 const FlagGrid &flags,
                 const Grid<Real> &phiObs,
                 const Real shift,
                 const Real thresh,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude)
      : KernelBase(parts.size()),
        parts(parts),
        flags(flags),
        phiObs(phiObs),
        shift(shift),
        thresh(thresh),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystem &parts,
                 const FlagGrid &flags,
                 const Grid<Real> &phiObs,
                 const Real shift,
                 const Real thresh,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (!parts.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    Vec3i p = toVec3i(parts.getPos(idx));

    if (!flags.isInBounds(p))
      return;
    Real v = phiObs.getInterpolated(parts.getPos(idx));
    if (v < thresh) {
      Vec3 grad = getGradient(phiObs, p.x, p.y, p.z);
      if (normalize(grad) < VECTOR_EPSILON)
        return;
      parts.setPos(idx, parts.getPos(idx) + grad * (thresh - v + shift));
    }
  }
  inline BasicParticleSystem &getArg0()
  {
    return parts;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return phiObs;
  }
  typedef Grid<Real> type2;
  inline const Real &getArg3()
  {
    return shift;
  }
  typedef Real type3;
  inline const Real &getArg4()
  {
    return thresh;
  }
  typedef Real type4;
  inline const ParticleDataImpl<int> *getArg5()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type5;
  inline const int &getArg6()
  {
    return exclude;
  }
  typedef int type6;
  void runMessage()
  {
    debMsg("Executing kernel knPushOutofObs ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, parts, flags, phiObs, shift, thresh, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystem &parts;
  const FlagGrid &flags;
  const Grid<Real> &phiObs;
  const Real shift;
  const Real thresh;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};
//! push particles out of obstacle levelset

void pushOutofObs(BasicParticleSystem &parts,
                  const FlagGrid &flags,
                  const Grid<Real> &phiObs,
                  const Real shift = 0,
                  const Real thresh = 0,
                  const ParticleDataImpl<int> *ptype = nullptr,
                  const int exclude = 0)
{
  knPushOutofObs(parts, flags, phiObs, shift, thresh, ptype, exclude);
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "pushOutofObs", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const Grid<Real> &phiObs = *_args.getPtr<Grid<Real>>("phiObs", 2, &_lock);
      const Real shift = _args.getOpt<Real>("shift", 3, 0, &_lock);
      const Real thresh = _args.getOpt<Real>("thresh", 4, 0, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 5, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 6, 0, &_lock);
      _retval = getPyNone();
      pushOutofObs(parts, flags, phiObs, shift, thresh, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "pushOutofObs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("pushOutofObs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_pushOutofObs("", "pushOutofObs", _W_11);
extern "C" {
void PbRegister_pushOutofObs()
{
  KEEP_UNUSED(_RP_pushOutofObs);
}
}

//******************************************************************************
// grid interpolation functions

template<class T> struct knSafeDivReal : public KernelBase {
  knSafeDivReal(Grid<T> &me, const Grid<Real> &other, Real cutoff = VECTOR_EPSILON)
      : KernelBase(&me, 0), me(me), other(other), cutoff(cutoff)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 Grid<T> &me,
                 const Grid<Real> &other,
                 Real cutoff = VECTOR_EPSILON) const
  {
    if (other[idx] < cutoff) {
      me[idx] = 0.;
    }
    else {
      T div(other[idx]);
      me[idx] = safeDivide(me[idx], div);
    }
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline const Grid<Real> &getArg1()
  {
    return other;
  }
  typedef Grid<Real> type1;
  inline Real &getArg2()
  {
    return cutoff;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel knSafeDivReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other, cutoff);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  const Grid<Real> &other;
  Real cutoff;
};

// Set velocities on the grid from the particle system

struct knMapLinearVec3ToMACGrid : public KernelBase {
  knMapLinearVec3ToMACGrid(const BasicParticleSystem &p,
                           const FlagGrid &flags,
                           const MACGrid &vel,
                           Grid<Vec3> &tmp,
                           const ParticleDataImpl<Vec3> &pvel,
                           const ParticleDataImpl<int> *ptype,
                           const int exclude)
      : KernelBase(p.size()),
        p(p),
        flags(flags),
        vel(vel),
        tmp(tmp),
        pvel(pvel),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 const FlagGrid &flags,
                 const MACGrid &vel,
                 Grid<Vec3> &tmp,
                 const ParticleDataImpl<Vec3> &pvel,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude)
  {
    unusedParameter(flags);
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    vel.setInterpolated(p[idx].pos, pvel[idx], &tmp[0]);
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline Grid<Vec3> &getArg3()
  {
    return tmp;
  }
  typedef Grid<Vec3> type3;
  inline const ParticleDataImpl<Vec3> &getArg4()
  {
    return pvel;
  }
  typedef ParticleDataImpl<Vec3> type4;
  inline const ParticleDataImpl<int> *getArg5()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type5;
  inline const int &getArg6()
  {
    return exclude;
  }
  typedef int type6;
  void runMessage()
  {
    debMsg("Executing kernel knMapLinearVec3ToMACGrid ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void run()
  {
    const IndexInt _sz = size;
    for (IndexInt i = 0; i < _sz; i++)
      op(i, p, flags, vel, tmp, pvel, ptype, exclude);
  }
  const BasicParticleSystem &p;
  const FlagGrid &flags;
  const MACGrid &vel;
  Grid<Vec3> &tmp;
  const ParticleDataImpl<Vec3> &pvel;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

// optionally , this function can use an existing vec3 grid to store the weights
// this is useful in combination with the simple extrapolation function

void mapPartsToMAC(const FlagGrid &flags,
                   MACGrid &vel,
                   MACGrid &velOld,
                   const BasicParticleSystem &parts,
                   const ParticleDataImpl<Vec3> &partVel,
                   Grid<Vec3> *weight = nullptr,
                   const ParticleDataImpl<int> *ptype = nullptr,
                   const int exclude = 0)
{
  // interpol -> grid. tmpgrid for particle contribution weights
  bool freeTmp = false;
  if (!weight) {
    weight = new Grid<Vec3>(flags.getParent());
    freeTmp = true;
  }
  else {
    weight->clear();  // make sure we start with a zero grid!
  }
  vel.clear();
  knMapLinearVec3ToMACGrid(parts, flags, vel, *weight, partVel, ptype, exclude);

  // stomp small values in weight to zero to prevent roundoff errors
  weight->stomp(Vec3(VECTOR_EPSILON));
  vel.safeDivide(*weight);

  // store original state
  velOld.copyFrom(vel);
  if (freeTmp)
    delete weight;
}
static PyObject *_W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapPartsToMAC", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      MACGrid &velOld = *_args.getPtr<MACGrid>("velOld", 2, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 3, &_lock);
      const ParticleDataImpl<Vec3> &partVel = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "partVel", 4, &_lock);
      Grid<Vec3> *weight = _args.getPtrOpt<Grid<Vec3>>("weight", 5, nullptr, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 6, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 7, 0, &_lock);
      _retval = getPyNone();
      mapPartsToMAC(flags, vel, velOld, parts, partVel, weight, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapPartsToMAC", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapPartsToMAC", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapPartsToMAC("", "mapPartsToMAC", _W_12);
extern "C" {
void PbRegister_mapPartsToMAC()
{
  KEEP_UNUSED(_RP_mapPartsToMAC);
}
}

template<class T> struct knMapLinear : public KernelBase {
  knMapLinear(const BasicParticleSystem &p,
              const FlagGrid &flags,
              const Grid<T> &target,
              Grid<Real> &gtmp,
              const ParticleDataImpl<T> &psource)
      : KernelBase(p.size()), p(p), flags(flags), target(target), gtmp(gtmp), psource(psource)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 const FlagGrid &flags,
                 const Grid<T> &target,
                 Grid<Real> &gtmp,
                 const ParticleDataImpl<T> &psource)
  {
    unusedParameter(flags);
    if (!p.isActive(idx))
      return;
    target.setInterpolated(p[idx].pos, psource[idx], gtmp);
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Grid<T> &getArg2()
  {
    return target;
  }
  typedef Grid<T> type2;
  inline Grid<Real> &getArg3()
  {
    return gtmp;
  }
  typedef Grid<Real> type3;
  inline const ParticleDataImpl<T> &getArg4()
  {
    return psource;
  }
  typedef ParticleDataImpl<T> type4;
  void runMessage()
  {
    debMsg("Executing kernel knMapLinear ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void run()
  {
    const IndexInt _sz = size;
    for (IndexInt i = 0; i < _sz; i++)
      op(i, p, flags, target, gtmp, psource);
  }
  const BasicParticleSystem &p;
  const FlagGrid &flags;
  const Grid<T> &target;
  Grid<Real> &gtmp;
  const ParticleDataImpl<T> &psource;
};

template<class T>
void mapLinearRealHelper(const FlagGrid &flags,
                         Grid<T> &target,
                         const BasicParticleSystem &parts,
                         const ParticleDataImpl<T> &source)
{
  Grid<Real> tmp(flags.getParent());
  target.clear();
  knMapLinear<T>(parts, flags, target, tmp, source);
  knSafeDivReal<T>(target, tmp);
}

void mapPartsToGrid(const FlagGrid &flags,
                    Grid<Real> &target,
                    const BasicParticleSystem &parts,
                    const ParticleDataImpl<Real> &source)
{
  mapLinearRealHelper<Real>(flags, target, parts, source);
}
static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapPartsToGrid", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      const ParticleDataImpl<Real> &source = *_args.getPtr<ParticleDataImpl<Real>>(
          "source", 3, &_lock);
      _retval = getPyNone();
      mapPartsToGrid(flags, target, parts, source);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapPartsToGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapPartsToGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapPartsToGrid("", "mapPartsToGrid", _W_13);
extern "C" {
void PbRegister_mapPartsToGrid()
{
  KEEP_UNUSED(_RP_mapPartsToGrid);
}
}

void mapPartsToGridVec3(const FlagGrid &flags,
                        Grid<Vec3> &target,
                        const BasicParticleSystem &parts,
                        const ParticleDataImpl<Vec3> &source)
{
  mapLinearRealHelper<Vec3>(flags, target, parts, source);
}
static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapPartsToGridVec3", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      const ParticleDataImpl<Vec3> &source = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "source", 3, &_lock);
      _retval = getPyNone();
      mapPartsToGridVec3(flags, target, parts, source);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapPartsToGridVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapPartsToGridVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapPartsToGridVec3("", "mapPartsToGridVec3", _W_14);
extern "C" {
void PbRegister_mapPartsToGridVec3()
{
  KEEP_UNUSED(_RP_mapPartsToGridVec3);
}
}

// integers need "max" mode, not yet implemented
// PYTHON() void mapPartsToGridInt ( FlagGrid& flags, Grid<int >& target , BasicParticleSystem&
// parts , ParticleDataImpl<int >& source ) { 	mapLinearRealHelper<int >(flags,target,parts,source);
//}

template<class T> struct knMapFromGrid : public KernelBase {
  knMapFromGrid(const BasicParticleSystem &p, const Grid<T> &gsrc, ParticleDataImpl<T> &target)
      : KernelBase(p.size()), p(p), gsrc(gsrc), target(target)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 const Grid<T> &gsrc,
                 ParticleDataImpl<T> &target) const
  {
    if (!p.isActive(idx))
      return;
    target[idx] = gsrc.getInterpolated(p[idx].pos);
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const Grid<T> &getArg1()
  {
    return gsrc;
  }
  typedef Grid<T> type1;
  inline ParticleDataImpl<T> &getArg2()
  {
    return target;
  }
  typedef ParticleDataImpl<T> type2;
  void runMessage()
  {
    debMsg("Executing kernel knMapFromGrid ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, gsrc, target);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &p;
  const Grid<T> &gsrc;
  ParticleDataImpl<T> &target;
};
void mapGridToParts(const Grid<Real> &source,
                    const BasicParticleSystem &parts,
                    ParticleDataImpl<Real> &target)
{
  knMapFromGrid<Real>(parts, source, target);
}
static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapGridToParts", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const Grid<Real> &source = *_args.getPtr<Grid<Real>>("source", 0, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 1, &_lock);
      ParticleDataImpl<Real> &target = *_args.getPtr<ParticleDataImpl<Real>>("target", 2, &_lock);
      _retval = getPyNone();
      mapGridToParts(source, parts, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapGridToParts", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapGridToParts", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapGridToParts("", "mapGridToParts", _W_15);
extern "C" {
void PbRegister_mapGridToParts()
{
  KEEP_UNUSED(_RP_mapGridToParts);
}
}

void mapGridToPartsVec3(const Grid<Vec3> &source,
                        const BasicParticleSystem &parts,
                        ParticleDataImpl<Vec3> &target)
{
  knMapFromGrid<Vec3>(parts, source, target);
}
static PyObject *_W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapGridToPartsVec3", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const Grid<Vec3> &source = *_args.getPtr<Grid<Vec3>>("source", 0, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 1, &_lock);
      ParticleDataImpl<Vec3> &target = *_args.getPtr<ParticleDataImpl<Vec3>>("target", 2, &_lock);
      _retval = getPyNone();
      mapGridToPartsVec3(source, parts, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapGridToPartsVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapGridToPartsVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapGridToPartsVec3("", "mapGridToPartsVec3", _W_16);
extern "C" {
void PbRegister_mapGridToPartsVec3()
{
  KEEP_UNUSED(_RP_mapGridToPartsVec3);
}
}

// Get velocities from grid

struct knMapLinearMACGridToVec3_PIC : public KernelBase {
  knMapLinearMACGridToVec3_PIC(const BasicParticleSystem &p,
                               const FlagGrid &flags,
                               const MACGrid &vel,
                               ParticleDataImpl<Vec3> &pvel,
                               const ParticleDataImpl<int> *ptype,
                               const int exclude)
      : KernelBase(p.size()),
        p(p),
        flags(flags),
        vel(vel),
        pvel(pvel),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 const FlagGrid &flags,
                 const MACGrid &vel,
                 ParticleDataImpl<Vec3> &pvel,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    // pure PIC
    pvel[idx] = vel.getInterpolated(p[idx].pos);
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline ParticleDataImpl<Vec3> &getArg3()
  {
    return pvel;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline const ParticleDataImpl<int> *getArg4()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type4;
  inline const int &getArg5()
  {
    return exclude;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel knMapLinearMACGridToVec3_PIC ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, flags, vel, pvel, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &p;
  const FlagGrid &flags;
  const MACGrid &vel;
  ParticleDataImpl<Vec3> &pvel;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

void mapMACToParts(const FlagGrid &flags,
                   const MACGrid &vel,
                   const BasicParticleSystem &parts,
                   ParticleDataImpl<Vec3> &partVel,
                   const ParticleDataImpl<int> *ptype = nullptr,
                   const int exclude = 0)
{
  knMapLinearMACGridToVec3_PIC(parts, flags, vel, partVel, ptype, exclude);
}
static PyObject *_W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mapMACToParts", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      ParticleDataImpl<Vec3> &partVel = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "partVel", 3, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 4, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 5, 0, &_lock);
      _retval = getPyNone();
      mapMACToParts(flags, vel, parts, partVel, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "mapMACToParts", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mapMACToParts", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mapMACToParts("", "mapMACToParts", _W_17);
extern "C" {
void PbRegister_mapMACToParts()
{
  KEEP_UNUSED(_RP_mapMACToParts);
}
}

// with flip delta interpolation

struct knMapLinearMACGridToVec3_FLIP : public KernelBase {
  knMapLinearMACGridToVec3_FLIP(const BasicParticleSystem &p,
                                const FlagGrid &flags,
                                const MACGrid &vel,
                                const MACGrid &oldVel,
                                ParticleDataImpl<Vec3> &pvel,
                                const Real flipRatio,
                                const ParticleDataImpl<int> *ptype,
                                const int exclude)
      : KernelBase(p.size()),
        p(p),
        flags(flags),
        vel(vel),
        oldVel(oldVel),
        pvel(pvel),
        flipRatio(flipRatio),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 const FlagGrid &flags,
                 const MACGrid &vel,
                 const MACGrid &oldVel,
                 ParticleDataImpl<Vec3> &pvel,
                 const Real flipRatio,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    Vec3 v = vel.getInterpolated(p[idx].pos);
    Vec3 delta = v - oldVel.getInterpolated(p[idx].pos);
    pvel[idx] = flipRatio * (pvel[idx] + delta) + (1.0 - flipRatio) * v;
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline const MACGrid &getArg3()
  {
    return oldVel;
  }
  typedef MACGrid type3;
  inline ParticleDataImpl<Vec3> &getArg4()
  {
    return pvel;
  }
  typedef ParticleDataImpl<Vec3> type4;
  inline const Real &getArg5()
  {
    return flipRatio;
  }
  typedef Real type5;
  inline const ParticleDataImpl<int> *getArg6()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type6;
  inline const int &getArg7()
  {
    return exclude;
  }
  typedef int type7;
  void runMessage()
  {
    debMsg("Executing kernel knMapLinearMACGridToVec3_FLIP ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, flags, vel, oldVel, pvel, flipRatio, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &p;
  const FlagGrid &flags;
  const MACGrid &vel;
  const MACGrid &oldVel;
  ParticleDataImpl<Vec3> &pvel;
  const Real flipRatio;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

void flipVelocityUpdate(const FlagGrid &flags,
                        const MACGrid &vel,
                        const MACGrid &velOld,
                        const BasicParticleSystem &parts,
                        ParticleDataImpl<Vec3> &partVel,
                        const Real flipRatio,
                        const ParticleDataImpl<int> *ptype = nullptr,
                        const int exclude = 0)
{
  knMapLinearMACGridToVec3_FLIP(parts, flags, vel, velOld, partVel, flipRatio, ptype, exclude);
}
static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipVelocityUpdate", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const MACGrid &velOld = *_args.getPtr<MACGrid>("velOld", 2, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 3, &_lock);
      ParticleDataImpl<Vec3> &partVel = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "partVel", 4, &_lock);
      const Real flipRatio = _args.get<Real>("flipRatio", 5, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 6, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 7, 0, &_lock);
      _retval = getPyNone();
      flipVelocityUpdate(flags, vel, velOld, parts, partVel, flipRatio, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipVelocityUpdate", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipVelocityUpdate", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipVelocityUpdate("", "flipVelocityUpdate", _W_18);
extern "C" {
void PbRegister_flipVelocityUpdate()
{
  KEEP_UNUSED(_RP_flipVelocityUpdate);
}
}

//******************************************************************************
// narrow band

struct knCombineVels : public KernelBase {
  knCombineVels(MACGrid &vel,
                const Grid<Vec3> &w,
                MACGrid &combineVel,
                const LevelsetGrid *phi,
                Real narrowBand,
                Real thresh)
      : KernelBase(&vel, 0),
        vel(vel),
        w(w),
        combineVel(combineVel),
        phi(phi),
        narrowBand(narrowBand),
        thresh(thresh)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &vel,
                 const Grid<Vec3> &w,
                 MACGrid &combineVel,
                 const LevelsetGrid *phi,
                 Real narrowBand,
                 Real thresh) const
  {
    int idx = vel.index(i, j, k);

    for (int c = 0; c < 3; ++c) {
      // Correct narrow-band FLIP
      if (phi) {
        Vec3 pos(i, j, k);
        pos[(c + 1) % 3] += Real(0.5);
        pos[(c + 2) % 3] += Real(0.5);
        Real p = phi->getInterpolated(pos);
        if (p < -narrowBand) {
          vel[idx][c] = 0;
          continue;
        }
      }

      if (w[idx][c] > thresh) {
        combineVel[idx][c] = vel[idx][c];
        vel[idx][c] = -1;
      }
      else {
        vel[idx][c] = 0;
      }
    }
  }
  inline MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline const Grid<Vec3> &getArg1()
  {
    return w;
  }
  typedef Grid<Vec3> type1;
  inline MACGrid &getArg2()
  {
    return combineVel;
  }
  typedef MACGrid type2;
  inline const LevelsetGrid *getArg3()
  {
    return phi;
  }
  typedef LevelsetGrid type3;
  inline Real &getArg4()
  {
    return narrowBand;
  }
  typedef Real type4;
  inline Real &getArg5()
  {
    return thresh;
  }
  typedef Real type5;
  void runMessage()
  {
    debMsg("Executing kernel knCombineVels ", 3);
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
            op(i, j, k, vel, w, combineVel, phi, narrowBand, thresh);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, vel, w, combineVel, phi, narrowBand, thresh);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &vel;
  const Grid<Vec3> &w;
  MACGrid &combineVel;
  const LevelsetGrid *phi;
  Real narrowBand;
  Real thresh;
};

//! narrow band velocity combination

void combineGridVel(MACGrid &vel,
                    const Grid<Vec3> &weight,
                    MACGrid &combineVel,
                    const LevelsetGrid *phi = nullptr,
                    Real narrowBand = 0.0,
                    Real thresh = 0.0)
{
  knCombineVels(vel, weight, combineVel, phi, narrowBand, thresh);
}
static PyObject *_W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "combineGridVel", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      const Grid<Vec3> &weight = *_args.getPtr<Grid<Vec3>>("weight", 1, &_lock);
      MACGrid &combineVel = *_args.getPtr<MACGrid>("combineVel", 2, &_lock);
      const LevelsetGrid *phi = _args.getPtrOpt<LevelsetGrid>("phi", 3, nullptr, &_lock);
      Real narrowBand = _args.getOpt<Real>("narrowBand", 4, 0.0, &_lock);
      Real thresh = _args.getOpt<Real>("thresh", 5, 0.0, &_lock);
      _retval = getPyNone();
      combineGridVel(vel, weight, combineVel, phi, narrowBand, thresh);
      _args.check();
    }
    pbFinalizePlugin(parent, "combineGridVel", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("combineGridVel", e.what());
    return 0;
  }
}
static const Pb::Register _RP_combineGridVel("", "combineGridVel", _W_19);
extern "C" {
void PbRegister_combineGridVel()
{
  KEEP_UNUSED(_RP_combineGridVel);
}
}

//! surface tension helper
void getLaplacian(Grid<Real> &laplacian, const Grid<Real> &grid)
{
  LaplaceOp(laplacian, grid);
}
static PyObject *_W_20(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getLaplacian", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &laplacian = *_args.getPtr<Grid<Real>>("laplacian", 0, &_lock);
      const Grid<Real> &grid = *_args.getPtr<Grid<Real>>("grid", 1, &_lock);
      _retval = getPyNone();
      getLaplacian(laplacian, grid);
      _args.check();
    }
    pbFinalizePlugin(parent, "getLaplacian", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getLaplacian", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getLaplacian("", "getLaplacian", _W_20);
extern "C" {
void PbRegister_getLaplacian()
{
  KEEP_UNUSED(_RP_getLaplacian);
}
}

void getCurvature(Grid<Real> &curv, const Grid<Real> &grid, const Real h = 1.0)
{
  CurvatureOp(curv, grid, h);
}
static PyObject *_W_21(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getCurvature", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &curv = *_args.getPtr<Grid<Real>>("curv", 0, &_lock);
      const Grid<Real> &grid = *_args.getPtr<Grid<Real>>("grid", 1, &_lock);
      const Real h = _args.getOpt<Real>("h", 2, 1.0, &_lock);
      _retval = getPyNone();
      getCurvature(curv, grid, h);
      _args.check();
    }
    pbFinalizePlugin(parent, "getCurvature", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getCurvature", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getCurvature("", "getCurvature", _W_21);
extern "C" {
void PbRegister_getCurvature()
{
  KEEP_UNUSED(_RP_getCurvature);
}
}

}  // namespace Manta
