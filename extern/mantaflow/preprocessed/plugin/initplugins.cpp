

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
 * Tools to setup fields and inflows
 *
 ******************************************************************************/

#include "vectorbase.h"
#include "shapes.h"
#include "commonkernels.h"
#include "particle.h"
#include "noisefield.h"
#include "simpleimage.h"
#include "mesh.h"

using namespace std;

namespace Manta {

//! Apply noise to grid

struct KnApplyNoiseInfl : public KernelBase {
  KnApplyNoiseInfl(const FlagGrid &flags,
                   Grid<Real> &density,
                   const WaveletNoiseField &noise,
                   const Grid<Real> &sdf,
                   Real scale,
                   Real sigma)
      : KernelBase(&flags, 0),
        flags(flags),
        density(density),
        noise(noise),
        sdf(sdf),
        scale(scale),
        sigma(sigma)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &density,
                 const WaveletNoiseField &noise,
                 const Grid<Real> &sdf,
                 Real scale,
                 Real sigma) const
  {
    if (!flags.isFluid(i, j, k) || sdf(i, j, k) > sigma)
      return;
    Real factor = clamp(1.0 - 0.5 / sigma * (sdf(i, j, k) + sigma), 0.0, 1.0);

    Real target = noise.evaluate(Vec3(i, j, k)) * scale * factor;
    if (density(i, j, k) < target)
      density(i, j, k) = target;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return density;
  }
  typedef Grid<Real> type1;
  inline const WaveletNoiseField &getArg2()
  {
    return noise;
  }
  typedef WaveletNoiseField type2;
  inline const Grid<Real> &getArg3()
  {
    return sdf;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return scale;
  }
  typedef Real type4;
  inline Real &getArg5()
  {
    return sigma;
  }
  typedef Real type5;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyNoiseInfl ", 3);
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
            op(i, j, k, flags, density, noise, sdf, scale, sigma);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, density, noise, sdf, scale, sigma);
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
  Grid<Real> &density;
  const WaveletNoiseField &noise;
  const Grid<Real> &sdf;
  Real scale;
  Real sigma;
};

//! Init noise-modulated density inside shape

void densityInflow(const FlagGrid &flags,
                   Grid<Real> &density,
                   const WaveletNoiseField &noise,
                   Shape *shape,
                   Real scale = 1.0,
                   Real sigma = 0)
{
  Grid<Real> sdf = shape->computeLevelset();
  KnApplyNoiseInfl(flags, density, noise, sdf, scale, sigma);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "densityInflow", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Shape *shape = _args.getPtr<Shape>("shape", 3, &_lock);
      Real scale = _args.getOpt<Real>("scale", 4, 1.0, &_lock);
      Real sigma = _args.getOpt<Real>("sigma", 5, 0, &_lock);
      _retval = getPyNone();
      densityInflow(flags, density, noise, shape, scale, sigma);
      _args.check();
    }
    pbFinalizePlugin(parent, "densityInflow", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("densityInflow", e.what());
    return 0;
  }
}
static const Pb::Register _RP_densityInflow("", "densityInflow", _W_0);
extern "C" {
void PbRegister_densityInflow()
{
  KEEP_UNUSED(_RP_densityInflow);
}
}

//! Apply noise to real grid based on an SDF
struct KnAddNoise : public KernelBase {
  KnAddNoise(const FlagGrid &flags,
             Grid<Real> &density,
             const WaveletNoiseField &noise,
             const Grid<Real> *sdf,
             Real scale)
      : KernelBase(&flags, 0), flags(flags), density(density), noise(noise), sdf(sdf), scale(scale)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &density,
                 const WaveletNoiseField &noise,
                 const Grid<Real> *sdf,
                 Real scale) const
  {
    if (!flags.isFluid(i, j, k) || (sdf && (*sdf)(i, j, k) > 0.))
      return;
    density(i, j, k) += noise.evaluate(Vec3(i, j, k)) * scale;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return density;
  }
  typedef Grid<Real> type1;
  inline const WaveletNoiseField &getArg2()
  {
    return noise;
  }
  typedef WaveletNoiseField type2;
  inline const Grid<Real> *getArg3()
  {
    return sdf;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return scale;
  }
  typedef Real type4;
  void runMessage()
  {
    debMsg("Executing kernel KnAddNoise ", 3);
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
            op(i, j, k, flags, density, noise, sdf, scale);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, density, noise, sdf, scale);
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
  Grid<Real> &density;
  const WaveletNoiseField &noise;
  const Grid<Real> *sdf;
  Real scale;
};
void addNoise(const FlagGrid &flags,
              Grid<Real> &density,
              const WaveletNoiseField &noise,
              const Grid<Real> *sdf = NULL,
              Real scale = 1.0)
{
  KnAddNoise(flags, density, noise, sdf, scale);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addNoise", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      const Grid<Real> *sdf = _args.getPtrOpt<Grid<Real>>("sdf", 3, NULL, &_lock);
      Real scale = _args.getOpt<Real>("scale", 4, 1.0, &_lock);
      _retval = getPyNone();
      addNoise(flags, density, noise, sdf, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "addNoise", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addNoise", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addNoise("", "addNoise", _W_1);
extern "C" {
void PbRegister_addNoise()
{
  KEEP_UNUSED(_RP_addNoise);
}
}

//! sample noise field and set pdata with its values (for convenience, scale the noise values)

template<class T> struct knSetPdataNoise : public KernelBase {
  knSetPdataNoise(const BasicParticleSystem &parts,
                  ParticleDataImpl<T> &pdata,
                  const WaveletNoiseField &noise,
                  Real scale)
      : KernelBase(parts.size()), parts(parts), pdata(pdata), noise(noise), scale(scale)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &parts,
                 ParticleDataImpl<T> &pdata,
                 const WaveletNoiseField &noise,
                 Real scale) const
  {
    pdata[idx] = noise.evaluate(parts.getPos(idx)) * scale;
  }
  inline const BasicParticleSystem &getArg0()
  {
    return parts;
  }
  typedef BasicParticleSystem type0;
  inline ParticleDataImpl<T> &getArg1()
  {
    return pdata;
  }
  typedef ParticleDataImpl<T> type1;
  inline const WaveletNoiseField &getArg2()
  {
    return noise;
  }
  typedef WaveletNoiseField type2;
  inline Real &getArg3()
  {
    return scale;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel knSetPdataNoise ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, parts, pdata, noise, scale);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &parts;
  ParticleDataImpl<T> &pdata;
  const WaveletNoiseField &noise;
  Real scale;
};

template<class T> struct knSetPdataNoiseVec : public KernelBase {
  knSetPdataNoiseVec(const BasicParticleSystem &parts,
                     ParticleDataImpl<T> &pdata,
                     const WaveletNoiseField &noise,
                     Real scale)
      : KernelBase(parts.size()), parts(parts), pdata(pdata), noise(noise), scale(scale)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &parts,
                 ParticleDataImpl<T> &pdata,
                 const WaveletNoiseField &noise,
                 Real scale) const
  {
    pdata[idx] = noise.evaluateVec(parts.getPos(idx)) * scale;
  }
  inline const BasicParticleSystem &getArg0()
  {
    return parts;
  }
  typedef BasicParticleSystem type0;
  inline ParticleDataImpl<T> &getArg1()
  {
    return pdata;
  }
  typedef ParticleDataImpl<T> type1;
  inline const WaveletNoiseField &getArg2()
  {
    return noise;
  }
  typedef WaveletNoiseField type2;
  inline Real &getArg3()
  {
    return scale;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel knSetPdataNoiseVec ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, parts, pdata, noise, scale);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &parts;
  ParticleDataImpl<T> &pdata;
  const WaveletNoiseField &noise;
  Real scale;
};
void setNoisePdata(const BasicParticleSystem &parts,
                   ParticleDataImpl<Real> &pd,
                   const WaveletNoiseField &noise,
                   Real scale = 1.)
{
  knSetPdataNoise<Real>(parts, pd, noise, scale);
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setNoisePdata", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleDataImpl<Real> &pd = *_args.getPtr<ParticleDataImpl<Real>>("pd", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1., &_lock);
      _retval = getPyNone();
      setNoisePdata(parts, pd, noise, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "setNoisePdata", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setNoisePdata", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setNoisePdata("", "setNoisePdata", _W_2);
extern "C" {
void PbRegister_setNoisePdata()
{
  KEEP_UNUSED(_RP_setNoisePdata);
}
}

void setNoisePdataVec3(const BasicParticleSystem &parts,
                       ParticleDataImpl<Vec3> &pd,
                       const WaveletNoiseField &noise,
                       Real scale = 1.)
{
  knSetPdataNoiseVec<Vec3>(parts, pd, noise, scale);
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setNoisePdataVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleDataImpl<Vec3> &pd = *_args.getPtr<ParticleDataImpl<Vec3>>("pd", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1., &_lock);
      _retval = getPyNone();
      setNoisePdataVec3(parts, pd, noise, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "setNoisePdataVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setNoisePdataVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setNoisePdataVec3("", "setNoisePdataVec3", _W_3);
extern "C" {
void PbRegister_setNoisePdataVec3()
{
  KEEP_UNUSED(_RP_setNoisePdataVec3);
}
}

void setNoisePdataInt(const BasicParticleSystem &parts,
                      ParticleDataImpl<int> &pd,
                      const WaveletNoiseField &noise,
                      Real scale = 1.)
{
  knSetPdataNoise<int>(parts, pd, noise, scale);
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setNoisePdataInt", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleDataImpl<int> &pd = *_args.getPtr<ParticleDataImpl<int>>("pd", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1., &_lock);
      _retval = getPyNone();
      setNoisePdataInt(parts, pd, noise, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "setNoisePdataInt", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setNoisePdataInt", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setNoisePdataInt("", "setNoisePdataInt", _W_4);
extern "C" {
void PbRegister_setNoisePdataInt()
{
  KEEP_UNUSED(_RP_setNoisePdataInt);
}
}

//! SDF gradient from obstacle flags, for turbulence.py
//  FIXME, slow, without kernel...
Grid<Vec3> obstacleGradient(const FlagGrid &flags)
{
  LevelsetGrid levelset(flags.getParent(), false);
  Grid<Vec3> gradient(flags.getParent());

  // rebuild obstacle levelset
  FOR_IDX(levelset)
  {
    levelset[idx] = flags.isObstacle(idx) ? -0.5 : 0.5;
  }
  levelset.reinitMarching(flags, 6.0, 0, true, false, FlagGrid::TypeReserved);

  // build levelset gradient
  GradientOp(gradient, levelset);

  FOR_IDX(levelset)
  {
    Vec3 grad = gradient[idx];
    Real s = normalize(grad);
    if (s <= 0.1 || levelset[idx] >= 0)
      grad = Vec3(0.);
    gradient[idx] = grad * levelset[idx];
  }

  return gradient;
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "obstacleGradient", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      _retval = toPy(obstacleGradient(flags));
      _args.check();
    }
    pbFinalizePlugin(parent, "obstacleGradient", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("obstacleGradient", e.what());
    return 0;
  }
}
static const Pb::Register _RP_obstacleGradient("", "obstacleGradient", _W_5);
extern "C" {
void PbRegister_obstacleGradient()
{
  KEEP_UNUSED(_RP_obstacleGradient);
}
}

//! SDF from obstacle flags, for turbulence.py
LevelsetGrid obstacleLevelset(const FlagGrid &flags)
{
  LevelsetGrid levelset(flags.getParent(), false);

  // rebuild obstacle levelset
  FOR_IDX(levelset)
  {
    levelset[idx] = flags.isObstacle(idx) ? -0.5 : 0.5;
  }
  levelset.reinitMarching(flags, 6.0, 0, true, false, FlagGrid::TypeReserved);

  return levelset;
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "obstacleLevelset", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      _retval = toPy(obstacleLevelset(flags));
      _args.check();
    }
    pbFinalizePlugin(parent, "obstacleLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("obstacleLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_obstacleLevelset("", "obstacleLevelset", _W_6);
extern "C" {
void PbRegister_obstacleLevelset()
{
  KEEP_UNUSED(_RP_obstacleLevelset);
}
}

//*****************************************************************************
// blender init functions

struct KnApplyEmission : public KernelBase {
  KnApplyEmission(const FlagGrid &flags,
                  Grid<Real> &target,
                  const Grid<Real> &source,
                  const Grid<Real> *emissionTexture,
                  bool isAbsolute,
                  int type)
      : KernelBase(&flags, 0),
        flags(flags),
        target(target),
        source(source),
        emissionTexture(emissionTexture),
        isAbsolute(isAbsolute),
        type(type)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &target,
                 const Grid<Real> &source,
                 const Grid<Real> *emissionTexture,
                 bool isAbsolute,
                 int type) const
  {
    // if type is given, only apply emission when celltype matches type from flaggrid
    // and if emission texture is given, only apply emission when some emission is present at cell
    // (important for emit from particles)
    bool isInflow = (type & FlagGrid::TypeInflow && flags.isInflow(i, j, k));
    bool isOutflow = (type & FlagGrid::TypeOutflow && flags.isOutflow(i, j, k));
    if ((type && !isInflow && !isOutflow) && (emissionTexture && !(*emissionTexture)(i, j, k)))
      return;

    if (isAbsolute)
      target(i, j, k) = source(i, j, k);
    else
      target(i, j, k) += source(i, j, k);
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return target;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return source;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> *getArg3()
  {
    return emissionTexture;
  }
  typedef Grid<Real> type3;
  inline bool &getArg4()
  {
    return isAbsolute;
  }
  typedef bool type4;
  inline int &getArg5()
  {
    return type;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyEmission ", 3);
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
            op(i, j, k, flags, target, source, emissionTexture, isAbsolute, type);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, target, source, emissionTexture, isAbsolute, type);
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
  Grid<Real> &target;
  const Grid<Real> &source;
  const Grid<Real> *emissionTexture;
  bool isAbsolute;
  int type;
};

//! Add emission values
// isAbsolute: whether to add emission values to existing, or replace
void applyEmission(FlagGrid &flags,
                   Grid<Real> &target,
                   Grid<Real> &source,
                   Grid<Real> *emissionTexture = NULL,
                   bool isAbsolute = true,
                   int type = 0)
{
  KnApplyEmission(flags, target, source, emissionTexture, isAbsolute, type);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "applyEmission", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      Grid<Real> &source = *_args.getPtr<Grid<Real>>("source", 2, &_lock);
      Grid<Real> *emissionTexture = _args.getPtrOpt<Grid<Real>>(
          "emissionTexture", 3, NULL, &_lock);
      bool isAbsolute = _args.getOpt<bool>("isAbsolute", 4, true, &_lock);
      int type = _args.getOpt<int>("type", 5, 0, &_lock);
      _retval = getPyNone();
      applyEmission(flags, target, source, emissionTexture, isAbsolute, type);
      _args.check();
    }
    pbFinalizePlugin(parent, "applyEmission", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("applyEmission", e.what());
    return 0;
  }
}
static const Pb::Register _RP_applyEmission("", "applyEmission", _W_7);
extern "C" {
void PbRegister_applyEmission()
{
  KEEP_UNUSED(_RP_applyEmission);
}
}

// blender init functions for meshes

struct KnApplyDensity : public KernelBase {
  KnApplyDensity(
      const FlagGrid &flags, Grid<Real> &density, const Grid<Real> &sdf, Real value, Real sigma)
      : KernelBase(&flags, 0), flags(flags), density(density), sdf(sdf), value(value), sigma(sigma)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &density,
                 const Grid<Real> &sdf,
                 Real value,
                 Real sigma) const
  {
    if (!flags.isFluid(i, j, k) || sdf(i, j, k) > sigma)
      return;
    density(i, j, k) = value;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return density;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return sdf;
  }
  typedef Grid<Real> type2;
  inline Real &getArg3()
  {
    return value;
  }
  typedef Real type3;
  inline Real &getArg4()
  {
    return sigma;
  }
  typedef Real type4;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyDensity ", 3);
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
            op(i, j, k, flags, density, sdf, value, sigma);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, density, sdf, value, sigma);
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
  Grid<Real> &density;
  const Grid<Real> &sdf;
  Real value;
  Real sigma;
};
//! Init noise-modulated density inside mesh

void densityInflowMeshNoise(const FlagGrid &flags,
                            Grid<Real> &density,
                            const WaveletNoiseField &noise,
                            Mesh *mesh,
                            Real scale = 1.0,
                            Real sigma = 0)
{
  LevelsetGrid sdf(density.getParent(), false);
  mesh->computeLevelset(sdf, 1.);
  KnApplyNoiseInfl(flags, density, noise, sdf, scale, sigma);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "densityInflowMeshNoise", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Mesh *mesh = _args.getPtr<Mesh>("mesh", 3, &_lock);
      Real scale = _args.getOpt<Real>("scale", 4, 1.0, &_lock);
      Real sigma = _args.getOpt<Real>("sigma", 5, 0, &_lock);
      _retval = getPyNone();
      densityInflowMeshNoise(flags, density, noise, mesh, scale, sigma);
      _args.check();
    }
    pbFinalizePlugin(parent, "densityInflowMeshNoise", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("densityInflowMeshNoise", e.what());
    return 0;
  }
}
static const Pb::Register _RP_densityInflowMeshNoise("", "densityInflowMeshNoise", _W_8);
extern "C" {
void PbRegister_densityInflowMeshNoise()
{
  KEEP_UNUSED(_RP_densityInflowMeshNoise);
}
}

//! Init constant density inside mesh

void densityInflowMesh(const FlagGrid &flags,
                       Grid<Real> &density,
                       Mesh *mesh,
                       Real value = 1.,
                       Real cutoff = 7,
                       Real sigma = 0)
{
  LevelsetGrid sdf(density.getParent(), false);
  mesh->computeLevelset(sdf, 2., cutoff);
  KnApplyDensity(flags, density, sdf, value, sigma);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "densityInflowMesh", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      Mesh *mesh = _args.getPtr<Mesh>("mesh", 2, &_lock);
      Real value = _args.getOpt<Real>("value", 3, 1., &_lock);
      Real cutoff = _args.getOpt<Real>("cutoff", 4, 7, &_lock);
      Real sigma = _args.getOpt<Real>("sigma", 5, 0, &_lock);
      _retval = getPyNone();
      densityInflowMesh(flags, density, mesh, value, cutoff, sigma);
      _args.check();
    }
    pbFinalizePlugin(parent, "densityInflowMesh", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("densityInflowMesh", e.what());
    return 0;
  }
}
static const Pb::Register _RP_densityInflowMesh("", "densityInflowMesh", _W_9);
extern "C" {
void PbRegister_densityInflowMesh()
{
  KEEP_UNUSED(_RP_densityInflowMesh);
}
}

struct KnResetInObstacle : public KernelBase {
  KnResetInObstacle(FlagGrid &flags,
                    MACGrid &vel,
                    Grid<Real> *density,
                    Grid<Real> *heat,
                    Grid<Real> *fuel,
                    Grid<Real> *flame,
                    Grid<Real> *red,
                    Grid<Real> *green,
                    Grid<Real> *blue,
                    Real resetValue)
      : KernelBase(&flags, 0),
        flags(flags),
        vel(vel),
        density(density),
        heat(heat),
        fuel(fuel),
        flame(flame),
        red(red),
        green(green),
        blue(blue),
        resetValue(resetValue)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 FlagGrid &flags,
                 MACGrid &vel,
                 Grid<Real> *density,
                 Grid<Real> *heat,
                 Grid<Real> *fuel,
                 Grid<Real> *flame,
                 Grid<Real> *red,
                 Grid<Real> *green,
                 Grid<Real> *blue,
                 Real resetValue) const
  {
    if (!flags.isObstacle(i, j, k))
      return;
    vel(i, j, k).x = resetValue;
    vel(i, j, k).y = resetValue;
    vel(i, j, k).z = resetValue;

    if (density) {
      (*density)(i, j, k) = resetValue;
    }
    if (heat) {
      (*heat)(i, j, k) = resetValue;
    }
    if (fuel) {
      (*fuel)(i, j, k) = resetValue;
      (*flame)(i, j, k) = resetValue;
    }
    if (red) {
      (*red)(i, j, k) = resetValue;
      (*green)(i, j, k) = resetValue;
      (*blue)(i, j, k) = resetValue;
    }
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline Grid<Real> *getArg2()
  {
    return density;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> *getArg3()
  {
    return heat;
  }
  typedef Grid<Real> type3;
  inline Grid<Real> *getArg4()
  {
    return fuel;
  }
  typedef Grid<Real> type4;
  inline Grid<Real> *getArg5()
  {
    return flame;
  }
  typedef Grid<Real> type5;
  inline Grid<Real> *getArg6()
  {
    return red;
  }
  typedef Grid<Real> type6;
  inline Grid<Real> *getArg7()
  {
    return green;
  }
  typedef Grid<Real> type7;
  inline Grid<Real> *getArg8()
  {
    return blue;
  }
  typedef Grid<Real> type8;
  inline Real &getArg9()
  {
    return resetValue;
  }
  typedef Real type9;
  void runMessage()
  {
    debMsg("Executing kernel KnResetInObstacle ", 3);
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
            op(i, j, k, flags, vel, density, heat, fuel, flame, red, green, blue, resetValue);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, vel, density, heat, fuel, flame, red, green, blue, resetValue);
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
  MACGrid &vel;
  Grid<Real> *density;
  Grid<Real> *heat;
  Grid<Real> *fuel;
  Grid<Real> *flame;
  Grid<Real> *red;
  Grid<Real> *green;
  Grid<Real> *blue;
  Real resetValue;
};

void resetInObstacle(FlagGrid &flags,
                     MACGrid &vel,
                     Grid<Real> *density,
                     Grid<Real> *heat = NULL,
                     Grid<Real> *fuel = NULL,
                     Grid<Real> *flame = NULL,
                     Grid<Real> *red = NULL,
                     Grid<Real> *green = NULL,
                     Grid<Real> *blue = NULL,
                     Real resetValue = 0)
{
  KnResetInObstacle(flags, vel, density, heat, fuel, flame, red, green, blue, resetValue);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "resetInObstacle", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      Grid<Real> *density = _args.getPtr<Grid<Real>>("density", 2, &_lock);
      Grid<Real> *heat = _args.getPtrOpt<Grid<Real>>("heat", 3, NULL, &_lock);
      Grid<Real> *fuel = _args.getPtrOpt<Grid<Real>>("fuel", 4, NULL, &_lock);
      Grid<Real> *flame = _args.getPtrOpt<Grid<Real>>("flame", 5, NULL, &_lock);
      Grid<Real> *red = _args.getPtrOpt<Grid<Real>>("red", 6, NULL, &_lock);
      Grid<Real> *green = _args.getPtrOpt<Grid<Real>>("green", 7, NULL, &_lock);
      Grid<Real> *blue = _args.getPtrOpt<Grid<Real>>("blue", 8, NULL, &_lock);
      Real resetValue = _args.getOpt<Real>("resetValue", 9, 0, &_lock);
      _retval = getPyNone();
      resetInObstacle(flags, vel, density, heat, fuel, flame, red, green, blue, resetValue);
      _args.check();
    }
    pbFinalizePlugin(parent, "resetInObstacle", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("resetInObstacle", e.what());
    return 0;
  }
}
static const Pb::Register _RP_resetInObstacle("", "resetInObstacle", _W_10);
extern "C" {
void PbRegister_resetInObstacle()
{
  KEEP_UNUSED(_RP_resetInObstacle);
}
}

//*****************************************************************************

//! check for symmetry , optionally enfore by copying

void checkSymmetry(
    Grid<Real> &a, Grid<Real> *err = NULL, bool symmetrize = false, int axis = 0, int bound = 0)
{
  const int c = axis;
  const int s = a.getSize()[c];
  FOR_IJK(a)
  {
    Vec3i idx(i, j, k), mdx(i, j, k);
    mdx[c] = s - 1 - idx[c];
    if (bound > 0 && ((!a.isInBounds(idx, bound)) || (!a.isInBounds(mdx, bound))))
      continue;

    if (err)
      (*err)(idx) = fabs((double)(a(idx) - a(mdx)));
    if (symmetrize && (idx[c] < s / 2)) {
      a(idx) = a(mdx);
    }
  }
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "checkSymmetry", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &a = *_args.getPtr<Grid<Real>>("a", 0, &_lock);
      Grid<Real> *err = _args.getPtrOpt<Grid<Real>>("err", 1, NULL, &_lock);
      bool symmetrize = _args.getOpt<bool>("symmetrize", 2, false, &_lock);
      int axis = _args.getOpt<int>("axis", 3, 0, &_lock);
      int bound = _args.getOpt<int>("bound", 4, 0, &_lock);
      _retval = getPyNone();
      checkSymmetry(a, err, symmetrize, axis, bound);
      _args.check();
    }
    pbFinalizePlugin(parent, "checkSymmetry", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("checkSymmetry", e.what());
    return 0;
  }
}
static const Pb::Register _RP_checkSymmetry("", "checkSymmetry", _W_11);
extern "C" {
void PbRegister_checkSymmetry()
{
  KEEP_UNUSED(_RP_checkSymmetry);
}
}

//! check for symmetry , mac grid version

void checkSymmetryVec3(Grid<Vec3> &a,
                       Grid<Real> *err = NULL,
                       bool symmetrize = false,
                       int axis = 0,
                       int bound = 0,
                       int disable = 0)
{
  if (err)
    err->setConst(0.);

  // each dimension is measured separately for flexibility (could be combined)
  const int c = axis;
  const int o1 = (c + 1) % 3;
  const int o2 = (c + 2) % 3;

  // x
  if (!(disable & 1)) {
    const int s = a.getSize()[c] + 1;
    FOR_IJK(a)
    {
      Vec3i idx(i, j, k), mdx(i, j, k);
      mdx[c] = s - 1 - idx[c];
      if (mdx[c] >= a.getSize()[c])
        continue;
      if (bound > 0 && ((!a.isInBounds(idx, bound)) || (!a.isInBounds(mdx, bound))))
        continue;

      // special case: center "line" of values , should be zero!
      if (mdx[c] == idx[c]) {
        if (err)
          (*err)(idx) += fabs((double)(a(idx)[c]));
        if (symmetrize)
          a(idx)[c] = 0.;
        continue;
      }

      // note - the a(mdx) component needs to be inverted here!
      if (err)
        (*err)(idx) += fabs((double)(a(idx)[c] - (a(mdx)[c] * -1.)));
      if (symmetrize && (idx[c] < s / 2)) {
        a(idx)[c] = a(mdx)[c] * -1.;
      }
    }
  }

  // y
  if (!(disable & 2)) {
    const int s = a.getSize()[c];
    FOR_IJK(a)
    {
      Vec3i idx(i, j, k), mdx(i, j, k);
      mdx[c] = s - 1 - idx[c];
      if (bound > 0 && ((!a.isInBounds(idx, bound)) || (!a.isInBounds(mdx, bound))))
        continue;

      if (err)
        (*err)(idx) += fabs((double)(a(idx)[o1] - a(mdx)[o1]));
      if (symmetrize && (idx[c] < s / 2)) {
        a(idx)[o1] = a(mdx)[o1];
      }
    }
  }

  // z
  if (!(disable & 4)) {
    const int s = a.getSize()[c];
    FOR_IJK(a)
    {
      Vec3i idx(i, j, k), mdx(i, j, k);
      mdx[c] = s - 1 - idx[c];
      if (bound > 0 && ((!a.isInBounds(idx, bound)) || (!a.isInBounds(mdx, bound))))
        continue;

      if (err)
        (*err)(idx) += fabs((double)(a(idx)[o2] - a(mdx)[o2]));
      if (symmetrize && (idx[c] < s / 2)) {
        a(idx)[o2] = a(mdx)[o2];
      }
    }
  }
}
static PyObject *_W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "checkSymmetryVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &a = *_args.getPtr<Grid<Vec3>>("a", 0, &_lock);
      Grid<Real> *err = _args.getPtrOpt<Grid<Real>>("err", 1, NULL, &_lock);
      bool symmetrize = _args.getOpt<bool>("symmetrize", 2, false, &_lock);
      int axis = _args.getOpt<int>("axis", 3, 0, &_lock);
      int bound = _args.getOpt<int>("bound", 4, 0, &_lock);
      int disable = _args.getOpt<int>("disable", 5, 0, &_lock);
      _retval = getPyNone();
      checkSymmetryVec3(a, err, symmetrize, axis, bound, disable);
      _args.check();
    }
    pbFinalizePlugin(parent, "checkSymmetryVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("checkSymmetryVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_checkSymmetryVec3("", "checkSymmetryVec3", _W_12);
extern "C" {
void PbRegister_checkSymmetryVec3()
{
  KEEP_UNUSED(_RP_checkSymmetryVec3);
}
}

// from simpleimage.cpp
void projectImg(SimpleImage &img, const Grid<Real> &val, int shadeMode = 0, Real scale = 1.);

//! output shaded (all 3 axes at once for 3D)
//! shading modes: 0 smoke, 1 surfaces

void projectPpmFull(const Grid<Real> &val, string name, int shadeMode = 0, Real scale = 1.)
{
  SimpleImage img;
  projectImg(img, val, shadeMode, scale);
  img.writePpm(name);
}
static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "projectPpmFull", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid<Real> &val = *_args.getPtr<Grid<Real>>("val", 0, &_lock);
      string name = _args.get<string>("name", 1, &_lock);
      int shadeMode = _args.getOpt<int>("shadeMode", 2, 0, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1., &_lock);
      _retval = getPyNone();
      projectPpmFull(val, name, shadeMode, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "projectPpmFull", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("projectPpmFull", e.what());
    return 0;
  }
}
static const Pb::Register _RP_projectPpmFull("", "projectPpmFull", _W_13);
extern "C" {
void PbRegister_projectPpmFull()
{
  KEEP_UNUSED(_RP_projectPpmFull);
}
}

// helper functions for pdata operator tests

//! init some test particles at the origin

void addTestParts(BasicParticleSystem &parts, int num)
{
  for (int i = 0; i < num; ++i)
    parts.addBuffered(Vec3(0, 0, 0));

  parts.doCompress();
  parts.insertBufferedParticles();
}
static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addTestParts", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      int num = _args.get<int>("num", 1, &_lock);
      _retval = getPyNone();
      addTestParts(parts, num);
      _args.check();
    }
    pbFinalizePlugin(parent, "addTestParts", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addTestParts", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addTestParts("", "addTestParts", _W_14);
extern "C" {
void PbRegister_addTestParts()
{
  KEEP_UNUSED(_RP_addTestParts);
}
}

//! calculate the difference between two pdata fields (note - slow!, not parallelized)

Real pdataMaxDiff(const ParticleDataBase *a, const ParticleDataBase *b)
{
  double maxVal = 0.;
  // debMsg(" PD "<< a->getType()<<"  as"<<a->getSizeSlow()<<"  bs"<<b->getSizeSlow() , 1);
  assertMsg(a->getType() == b->getType(), "pdataMaxDiff problem - different pdata types!");
  assertMsg(a->getSizeSlow() == b->getSizeSlow(), "pdataMaxDiff problem - different pdata sizes!");

  if (a->getType() & ParticleDataBase::TypeReal) {
    const ParticleDataImpl<Real> &av = *dynamic_cast<const ParticleDataImpl<Real> *>(a);
    const ParticleDataImpl<Real> &bv = *dynamic_cast<const ParticleDataImpl<Real> *>(b);
    FOR_PARTS(av)
    {
      maxVal = std::max(maxVal, (double)fabs(av[idx] - bv[idx]));
    }
  }
  else if (a->getType() & ParticleDataBase::TypeInt) {
    const ParticleDataImpl<int> &av = *dynamic_cast<const ParticleDataImpl<int> *>(a);
    const ParticleDataImpl<int> &bv = *dynamic_cast<const ParticleDataImpl<int> *>(b);
    FOR_PARTS(av)
    {
      maxVal = std::max(maxVal, (double)fabs((double)av[idx] - bv[idx]));
    }
  }
  else if (a->getType() & ParticleDataBase::TypeVec3) {
    const ParticleDataImpl<Vec3> &av = *dynamic_cast<const ParticleDataImpl<Vec3> *>(a);
    const ParticleDataImpl<Vec3> &bv = *dynamic_cast<const ParticleDataImpl<Vec3> *>(b);
    FOR_PARTS(av)
    {
      double d = 0.;
      for (int c = 0; c < 3; ++c) {
        d += fabs((double)av[idx][c] - (double)bv[idx][c]);
      }
      maxVal = std::max(maxVal, d);
    }
  }
  else {
    errMsg("pdataMaxDiff: Grid Type is not supported (only Real, Vec3, int)");
  }

  return maxVal;
}
static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "pdataMaxDiff", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const ParticleDataBase *a = _args.getPtr<ParticleDataBase>("a", 0, &_lock);
      const ParticleDataBase *b = _args.getPtr<ParticleDataBase>("b", 1, &_lock);
      _retval = toPy(pdataMaxDiff(a, b));
      _args.check();
    }
    pbFinalizePlugin(parent, "pdataMaxDiff", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("pdataMaxDiff", e.what());
    return 0;
  }
}
static const Pb::Register _RP_pdataMaxDiff("", "pdataMaxDiff", _W_15);
extern "C" {
void PbRegister_pdataMaxDiff()
{
  KEEP_UNUSED(_RP_pdataMaxDiff);
}
}

//! calculate center of mass given density grid, for re-centering

Vec3 calcCenterOfMass(const Grid<Real> &density)
{
  Vec3 p(0.0f);
  Real w = 0.0f;
  FOR_IJK(density)
  {
    p += density(i, j, k) * Vec3(i + 0.5f, j + 0.5f, k + 0.5f);
    w += density(i, j, k);
  }
  if (w > 1e-6f)
    p /= w;
  return p;
}
static PyObject *_W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "calcCenterOfMass", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 0, &_lock);
      _retval = toPy(calcCenterOfMass(density));
      _args.check();
    }
    pbFinalizePlugin(parent, "calcCenterOfMass", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("calcCenterOfMass", e.what());
    return 0;
  }
}
static const Pb::Register _RP_calcCenterOfMass("", "calcCenterOfMass", _W_16);
extern "C" {
void PbRegister_calcCenterOfMass()
{
  KEEP_UNUSED(_RP_calcCenterOfMass);
}
}

//*****************************************************************************
// helper functions for volume fractions (which are needed for second order obstacle boundaries)

inline static Real calcFraction(Real phi1, Real phi2, Real fracThreshold)
{
  if (phi1 > 0. && phi2 > 0.)
    return 1.;
  if (phi1 < 0. && phi2 < 0.)
    return 0.;

  // make sure phi1 < phi2
  if (phi2 < phi1) {
    Real t = phi1;
    phi1 = phi2;
    phi2 = t;
  }
  Real denom = phi1 - phi2;
  if (denom > -1e-04)
    return 0.5;

  Real frac = 1. - phi1 / denom;
  if (frac < fracThreshold)
    frac = 0.;  // stomp small values , dont mark as fluid
  return std::min(Real(1), frac);
}

struct KnUpdateFractions : public KernelBase {
  KnUpdateFractions(const FlagGrid &flags,
                    const Grid<Real> &phiObs,
                    MACGrid &fractions,
                    const int &boundaryWidth,
                    const Real fracThreshold)
      : KernelBase(&flags, 1),
        flags(flags),
        phiObs(phiObs),
        fractions(fractions),
        boundaryWidth(boundaryWidth),
        fracThreshold(fracThreshold)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 const Grid<Real> &phiObs,
                 MACGrid &fractions,
                 const int &boundaryWidth,
                 const Real fracThreshold) const
  {

    // walls at domain bounds and inner objects
    fractions(i, j, k).x = calcFraction(phiObs(i, j, k), phiObs(i - 1, j, k), fracThreshold);
    fractions(i, j, k).y = calcFraction(phiObs(i, j, k), phiObs(i, j - 1, k), fracThreshold);
    if (phiObs.is3D()) {
      fractions(i, j, k).z = calcFraction(phiObs(i, j, k), phiObs(i, j, k - 1), fracThreshold);
    }

    // remaining BCs at the domain boundaries
    const int w = boundaryWidth;
    // only set if not in obstacle
    if (phiObs(i, j, k) < 0.)
      return;

    // x-direction boundaries
    if (i <= w + 1) {  // min x
      if ((flags.isInflow(i - 1, j, k)) || (flags.isOutflow(i - 1, j, k)) ||
          (flags.isOpen(i - 1, j, k))) {
        fractions(i, j, k).x = fractions(i, j, k).y = 1.;
        if (flags.is3D())
          fractions(i, j, k).z = 1.;
      }
    }
    if (i >= flags.getSizeX() - w - 2) {  // max x
      if ((flags.isInflow(i + 1, j, k)) || (flags.isOutflow(i + 1, j, k)) ||
          (flags.isOpen(i + 1, j, k))) {
        fractions(i + 1, j, k).x = fractions(i + 1, j, k).y = 1.;
        if (flags.is3D())
          fractions(i + 1, j, k).z = 1.;
      }
    }
    // y-direction boundaries
    if (j <= w + 1) {  // min y
      if ((flags.isInflow(i, j - 1, k)) || (flags.isOutflow(i, j - 1, k)) ||
          (flags.isOpen(i, j - 1, k))) {
        fractions(i, j, k).x = fractions(i, j, k).y = 1.;
        if (flags.is3D())
          fractions(i, j, k).z = 1.;
      }
    }
    if (j >= flags.getSizeY() - w - 2) {  // max y
      if ((flags.isInflow(i, j + 1, k)) || (flags.isOutflow(i, j + 1, k)) ||
          (flags.isOpen(i, j + 1, k))) {
        fractions(i, j + 1, k).x = fractions(i, j + 1, k).y = 1.;
        if (flags.is3D())
          fractions(i, j + 1, k).z = 1.;
      }
    }
    // z-direction boundaries
    if (flags.is3D()) {
      if (k <= w + 1) {  // min z
        if ((flags.isInflow(i, j, k - 1)) || (flags.isOutflow(i, j, k - 1)) ||
            (flags.isOpen(i, j, k - 1))) {
          fractions(i, j, k).x = fractions(i, j, k).y = 1.;
          if (flags.is3D())
            fractions(i, j, k).z = 1.;
        }
      }
      if (j >= flags.getSizeZ() - w - 2) {  // max z
        if ((flags.isInflow(i, j, k + 1)) || (flags.isOutflow(i, j, k + 1)) ||
            (flags.isOpen(i, j, k + 1))) {
          fractions(i, j, k + 1).x = fractions(i, j, k + 1).y = 1.;
          if (flags.is3D())
            fractions(i, j, k + 1).z = 1.;
        }
      }
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const Grid<Real> &getArg1()
  {
    return phiObs;
  }
  typedef Grid<Real> type1;
  inline MACGrid &getArg2()
  {
    return fractions;
  }
  typedef MACGrid type2;
  inline const int &getArg3()
  {
    return boundaryWidth;
  }
  typedef int type3;
  inline const Real &getArg4()
  {
    return fracThreshold;
  }
  typedef Real type4;
  void runMessage()
  {
    debMsg("Executing kernel KnUpdateFractions ", 3);
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
            op(i, j, k, flags, phiObs, fractions, boundaryWidth, fracThreshold);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, phiObs, fractions, boundaryWidth, fracThreshold);
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
  const Grid<Real> &phiObs;
  MACGrid &fractions;
  const int &boundaryWidth;
  const Real fracThreshold;
};

//! update fill fraction values
void updateFractions(const FlagGrid &flags,
                     const Grid<Real> &phiObs,
                     MACGrid &fractions,
                     const int &boundaryWidth = 0,
                     const Real fracThreshold = 0.01)
{
  fractions.setConst(Vec3(0.));
  KnUpdateFractions(flags, phiObs, fractions, boundaryWidth, fracThreshold);
}
static PyObject *_W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "updateFractions", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const Grid<Real> &phiObs = *_args.getPtr<Grid<Real>>("phiObs", 1, &_lock);
      MACGrid &fractions = *_args.getPtr<MACGrid>("fractions", 2, &_lock);
      const int &boundaryWidth = _args.getOpt<int>("boundaryWidth", 3, 0, &_lock);
      const Real fracThreshold = _args.getOpt<Real>("fracThreshold", 4, 0.01, &_lock);
      _retval = getPyNone();
      updateFractions(flags, phiObs, fractions, boundaryWidth, fracThreshold);
      _args.check();
    }
    pbFinalizePlugin(parent, "updateFractions", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("updateFractions", e.what());
    return 0;
  }
}
static const Pb::Register _RP_updateFractions("", "updateFractions", _W_17);
extern "C" {
void PbRegister_updateFractions()
{
  KEEP_UNUSED(_RP_updateFractions);
}
}

struct KnUpdateFlagsObs : public KernelBase {
  KnUpdateFlagsObs(FlagGrid &flags,
                   const MACGrid *fractions,
                   const Grid<Real> &phiObs,
                   const Grid<Real> *phiOut,
                   const Grid<Real> *phiIn,
                   int boundaryWidth)
      : KernelBase(&flags, boundaryWidth),
        flags(flags),
        fractions(fractions),
        phiObs(phiObs),
        phiOut(phiOut),
        phiIn(phiIn),
        boundaryWidth(boundaryWidth)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 FlagGrid &flags,
                 const MACGrid *fractions,
                 const Grid<Real> &phiObs,
                 const Grid<Real> *phiOut,
                 const Grid<Real> *phiIn,
                 int boundaryWidth) const
  {

    bool isObs = false;
    if (fractions) {
      Real f = 0.;
      f += fractions->get(i, j, k).x;
      f += fractions->get(i + 1, j, k).x;
      f += fractions->get(i, j, k).y;
      f += fractions->get(i, j + 1, k).y;
      if (flags.is3D()) {
        f += fractions->get(i, j, k).z;
        f += fractions->get(i, j, k + 1).z;
      }
      if (f == 0.)
        isObs = true;
    }
    else {
      if (phiObs(i, j, k) < 0.)
        isObs = true;
    }

    bool isOutflow = false;
    bool isInflow = false;
    if (phiOut && (*phiOut)(i, j, k) < 0.)
      isOutflow = true;
    if (phiIn && (*phiIn)(i, j, k) < 0.)
      isInflow = true;

    if (isObs)
      flags(i, j, k) = FlagGrid::TypeObstacle;
    else if (isInflow)
      flags(i, j, k) = (FlagGrid::TypeFluid | FlagGrid::TypeInflow);
    else if (isOutflow)
      flags(i, j, k) = (FlagGrid::TypeEmpty | FlagGrid::TypeOutflow);
    else
      flags(i, j, k) = FlagGrid::TypeEmpty;
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const MACGrid *getArg1()
  {
    return fractions;
  }
  typedef MACGrid type1;
  inline const Grid<Real> &getArg2()
  {
    return phiObs;
  }
  typedef Grid<Real> type2;
  inline const Grid<Real> *getArg3()
  {
    return phiOut;
  }
  typedef Grid<Real> type3;
  inline const Grid<Real> *getArg4()
  {
    return phiIn;
  }
  typedef Grid<Real> type4;
  inline int &getArg5()
  {
    return boundaryWidth;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnUpdateFlagsObs ", 3);
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
        for (int j = boundaryWidth; j < _maxY; j++)
          for (int i = boundaryWidth; i < _maxX; i++)
            op(i, j, k, flags, fractions, phiObs, phiOut, phiIn, boundaryWidth);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = boundaryWidth; i < _maxX; i++)
          op(i, j, k, flags, fractions, phiObs, phiOut, phiIn, boundaryWidth);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(boundaryWidth, maxY), *this);
  }
  FlagGrid &flags;
  const MACGrid *fractions;
  const Grid<Real> &phiObs;
  const Grid<Real> *phiOut;
  const Grid<Real> *phiIn;
  int boundaryWidth;
};

//! update obstacle and outflow flags from levelsets
//! optionally uses fill fractions for obstacle
void setObstacleFlags(FlagGrid &flags,
                      const Grid<Real> &phiObs,
                      const MACGrid *fractions = NULL,
                      const Grid<Real> *phiOut = NULL,
                      const Grid<Real> *phiIn = NULL,
                      int boundaryWidth = 1)
{
  KnUpdateFlagsObs(flags, fractions, phiObs, phiOut, phiIn, boundaryWidth);
}
static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setObstacleFlags", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const Grid<Real> &phiObs = *_args.getPtr<Grid<Real>>("phiObs", 1, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 2, NULL, &_lock);
      const Grid<Real> *phiOut = _args.getPtrOpt<Grid<Real>>("phiOut", 3, NULL, &_lock);
      const Grid<Real> *phiIn = _args.getPtrOpt<Grid<Real>>("phiIn", 4, NULL, &_lock);
      int boundaryWidth = _args.getOpt<int>("boundaryWidth", 5, 1, &_lock);
      _retval = getPyNone();
      setObstacleFlags(flags, phiObs, fractions, phiOut, phiIn, boundaryWidth);
      _args.check();
    }
    pbFinalizePlugin(parent, "setObstacleFlags", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setObstacleFlags", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setObstacleFlags("", "setObstacleFlags", _W_18);
extern "C" {
void PbRegister_setObstacleFlags()
{
  KEEP_UNUSED(_RP_setObstacleFlags);
}
}

//! small helper for test case test_1040_secOrderBnd.py
struct kninitVortexVelocity : public KernelBase {
  kninitVortexVelocity(const Grid<Real> &phiObs,
                       MACGrid &vel,
                       const Vec3 &center,
                       const Real &radius)
      : KernelBase(&phiObs, 0), phiObs(phiObs), vel(vel), center(center), radius(radius)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const Grid<Real> &phiObs,
                 MACGrid &vel,
                 const Vec3 &center,
                 const Real &radius) const
  {

    if (phiObs(i, j, k) >= -1.) {

      Real dx = i - center.x;
      if (dx >= 0)
        dx -= .5;
      else
        dx += .5;
      Real dy = j - center.y;
      Real r = std::sqrt(dx * dx + dy * dy);
      Real alpha = atan2(dy, dx);

      vel(i, j, k).x = -std::sin(alpha) * (r / radius);

      dx = i - center.x;
      dy = j - center.y;
      if (dy >= 0)
        dy -= .5;
      else
        dy += .5;
      r = std::sqrt(dx * dx + dy * dy);
      alpha = atan2(dy, dx);

      vel(i, j, k).y = std::cos(alpha) * (r / radius);
    }
  }
  inline const Grid<Real> &getArg0()
  {
    return phiObs;
  }
  typedef Grid<Real> type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const Vec3 &getArg2()
  {
    return center;
  }
  typedef Vec3 type2;
  inline const Real &getArg3()
  {
    return radius;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel kninitVortexVelocity ", 3);
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
            op(i, j, k, phiObs, vel, center, radius);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, phiObs, vel, center, radius);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const Grid<Real> &phiObs;
  MACGrid &vel;
  const Vec3 &center;
  const Real &radius;
};

void initVortexVelocity(const Grid<Real> &phiObs,
                        MACGrid &vel,
                        const Vec3 &center,
                        const Real &radius)
{
  kninitVortexVelocity(phiObs, vel, center, radius);
}
static PyObject *_W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "initVortexVelocity", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid<Real> &phiObs = *_args.getPtr<Grid<Real>>("phiObs", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Vec3 &center = _args.get<Vec3>("center", 2, &_lock);
      const Real &radius = _args.get<Real>("radius", 3, &_lock);
      _retval = getPyNone();
      initVortexVelocity(phiObs, vel, center, radius);
      _args.check();
    }
    pbFinalizePlugin(parent, "initVortexVelocity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("initVortexVelocity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_initVortexVelocity("", "initVortexVelocity", _W_19);
extern "C" {
void PbRegister_initVortexVelocity()
{
  KEEP_UNUSED(_RP_initVortexVelocity);
}
}

//*****************************************************************************
// helper functions for blurring

//! class for Gaussian Blur
struct GaussianKernelCreator {
 public:
  float mSigma;
  int mDim;
  float *mMat1D;

  GaussianKernelCreator() : mSigma(0.0f), mDim(0), mMat1D(NULL)
  {
  }
  GaussianKernelCreator(float sigma, int dim = 0) : mSigma(0.0f), mDim(0), mMat1D(NULL)
  {
    setGaussianSigma(sigma, dim);
  }

  Real getWeiAtDis(float disx, float disy)
  {
    float m = 1.0 / (sqrt(2.0 * M_PI) * mSigma);
    float v = m * exp(-(1.0 * disx * disx + 1.0 * disy * disy) / (2.0 * mSigma * mSigma));
    return v;
  }

  Real getWeiAtDis(float disx, float disy, float disz)
  {
    float m = 1.0 / (sqrt(2.0 * M_PI) * mSigma);
    float v = m * exp(-(1.0 * disx * disx + 1.0 * disy * disy + 1.0 * disz * disz) /
                      (2.0 * mSigma * mSigma));
    return v;
  }

  void setGaussianSigma(float sigma, int dim = 0)
  {
    mSigma = sigma;
    if (dim < 3)
      mDim = (int)(2.0 * 3.0 * sigma + 1.0f);
    else
      mDim = dim;
    if (mDim < 3)
      mDim = 3;

    if (mDim % 2 == 0)
      ++mDim;  // make dim odd

    float s2 = mSigma * mSigma;
    int c = mDim / 2;
    float m = 1.0 / (sqrt(2.0 * M_PI) * mSigma);

    // create 1D matrix
    if (mMat1D)
      delete[] mMat1D;
    mMat1D = new float[mDim];
    for (int i = 0; i < (mDim + 1) / 2; i++) {
      float v = m * exp(-(1.0 * i * i) / (2.0 * s2));
      mMat1D[c + i] = v;
      mMat1D[c - i] = v;
    }
  }

  ~GaussianKernelCreator()
  {
    if (mMat1D)
      delete[] mMat1D;
  }

  float get1DKernelValue(int off)
  {
    assertMsg(off >= 0 && off < mDim, "off exceeded boundary in Gaussian Kernel 1D!");
    return mMat1D[off];
  }
};

template<class T>
T convolveGrid(Grid<T> &originGrid, GaussianKernelCreator &gkSigma, Vec3 pos, int cdir)
{
  // pos should be the centre pos, e.g., 1.5, 4.5, 0.5 for grid pos 1,4,0
  Vec3 step(1.0, 0.0, 0.0);
  if (cdir == 1)  // todo, z
    step = Vec3(0.0, 1.0, 0.0);
  else if (cdir == 2)
    step = Vec3(0.0, 0.0, 1.0);
  T pxResult(0);
  for (int i = 0; i < gkSigma.mDim; ++i) {
    Vec3i curpos = toVec3i(pos - step * (i - gkSigma.mDim / 2));
    if (originGrid.isInBounds(curpos))
      pxResult += gkSigma.get1DKernelValue(i) * originGrid.get(curpos);
    else {  // TODO , improve...
      Vec3i curfitpos = curpos;
      if (curfitpos.x < 0)
        curfitpos.x = 0;
      else if (curfitpos.x >= originGrid.getSizeX())
        curfitpos.x = originGrid.getSizeX() - 1;
      if (curfitpos.y < 0)
        curfitpos.y = 0;
      else if (curfitpos.y >= originGrid.getSizeY())
        curfitpos.y = originGrid.getSizeY() - 1;
      if (curfitpos.z < 0)
        curfitpos.z = 0;
      else if (curfitpos.z >= originGrid.getSizeZ())
        curfitpos.z = originGrid.getSizeZ() - 1;
      pxResult += gkSigma.get1DKernelValue(i) * originGrid.get(curfitpos);
    }
  }
  return pxResult;
}

template<class T> struct knBlurGrid : public KernelBase {
  knBlurGrid(Grid<T> &originGrid, Grid<T> &targetGrid, GaussianKernelCreator &gkSigma, int cdir)
      : KernelBase(&originGrid, 0),
        originGrid(originGrid),
        targetGrid(targetGrid),
        gkSigma(gkSigma),
        cdir(cdir)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<T> &originGrid,
                 Grid<T> &targetGrid,
                 GaussianKernelCreator &gkSigma,
                 int cdir) const
  {
    targetGrid(i, j, k) = convolveGrid<T>(originGrid, gkSigma, Vec3(i, j, k), cdir);
  }
  inline Grid<T> &getArg0()
  {
    return originGrid;
  }
  typedef Grid<T> type0;
  inline Grid<T> &getArg1()
  {
    return targetGrid;
  }
  typedef Grid<T> type1;
  inline GaussianKernelCreator &getArg2()
  {
    return gkSigma;
  }
  typedef GaussianKernelCreator type2;
  inline int &getArg3()
  {
    return cdir;
  }
  typedef int type3;
  void runMessage()
  {
    debMsg("Executing kernel knBlurGrid ", 3);
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
            op(i, j, k, originGrid, targetGrid, gkSigma, cdir);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, originGrid, targetGrid, gkSigma, cdir);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> &originGrid;
  Grid<T> &targetGrid;
  GaussianKernelCreator &gkSigma;
  int cdir;
};

template<class T> int blurGrid(Grid<T> &originGrid, Grid<T> &targetGrid, float sigma)
{
  GaussianKernelCreator tmGK(sigma);
  Grid<T> tmpGrid(originGrid);
  knBlurGrid<T>(originGrid, tmpGrid, tmGK, 0);  // blur x
  knBlurGrid<T>(tmpGrid, targetGrid, tmGK, 1);  // blur y
  if (targetGrid.is3D()) {
    tmpGrid.copyFrom(targetGrid);
    knBlurGrid<T>(tmpGrid, targetGrid, tmGK, 2);
  }
  return tmGK.mDim;
}

struct KnBlurMACGridGauss : public KernelBase {
  KnBlurMACGridGauss(MACGrid &originGrid,
                     MACGrid &target,
                     GaussianKernelCreator &gkSigma,
                     int cdir)
      : KernelBase(&originGrid, 0),
        originGrid(originGrid),
        target(target),
        gkSigma(gkSigma),
        cdir(cdir)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &originGrid,
                 MACGrid &target,
                 GaussianKernelCreator &gkSigma,
                 int cdir) const
  {
    Vec3 pos(i, j, k);
    Vec3 step(1.0, 0.0, 0.0);
    if (cdir == 1)
      step = Vec3(0.0, 1.0, 0.0);
    else if (cdir == 2)
      step = Vec3(0.0, 0.0, 1.0);

    Vec3 pxResult(0.0f);
    for (int di = 0; di < gkSigma.mDim; ++di) {
      Vec3i curpos = toVec3i(pos - step * (di - gkSigma.mDim / 2));
      if (!originGrid.isInBounds(curpos)) {
        if (curpos.x < 0)
          curpos.x = 0;
        else if (curpos.x >= originGrid.getSizeX())
          curpos.x = originGrid.getSizeX() - 1;
        if (curpos.y < 0)
          curpos.y = 0;
        else if (curpos.y >= originGrid.getSizeY())
          curpos.y = originGrid.getSizeY() - 1;
        if (curpos.z < 0)
          curpos.z = 0;
        else if (curpos.z >= originGrid.getSizeZ())
          curpos.z = originGrid.getSizeZ() - 1;
      }
      pxResult += gkSigma.get1DKernelValue(di) * originGrid.get(curpos);
    }
    target(i, j, k) = pxResult;
  }
  inline MACGrid &getArg0()
  {
    return originGrid;
  }
  typedef MACGrid type0;
  inline MACGrid &getArg1()
  {
    return target;
  }
  typedef MACGrid type1;
  inline GaussianKernelCreator &getArg2()
  {
    return gkSigma;
  }
  typedef GaussianKernelCreator type2;
  inline int &getArg3()
  {
    return cdir;
  }
  typedef int type3;
  void runMessage()
  {
    debMsg("Executing kernel KnBlurMACGridGauss ", 3);
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
            op(i, j, k, originGrid, target, gkSigma, cdir);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, originGrid, target, gkSigma, cdir);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &originGrid;
  MACGrid &target;
  GaussianKernelCreator &gkSigma;
  int cdir;
};

int blurMacGrid(MACGrid &oG, MACGrid &tG, float si)
{
  GaussianKernelCreator tmGK(si);
  MACGrid tmpGrid(oG);
  KnBlurMACGridGauss(oG, tmpGrid, tmGK, 0);  // blur x
  KnBlurMACGridGauss(tmpGrid, tG, tmGK, 1);  // blur y
  if (tG.is3D()) {
    tmpGrid.copyFrom(tG);
    KnBlurMACGridGauss(tmpGrid, tG, tmGK, 2);
  }
  return tmGK.mDim;
}
static PyObject *_W_20(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "blurMacGrid", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &oG = *_args.getPtr<MACGrid>("oG", 0, &_lock);
      MACGrid &tG = *_args.getPtr<MACGrid>("tG", 1, &_lock);
      float si = _args.get<float>("si", 2, &_lock);
      _retval = toPy(blurMacGrid(oG, tG, si));
      _args.check();
    }
    pbFinalizePlugin(parent, "blurMacGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("blurMacGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_blurMacGrid("", "blurMacGrid", _W_20);
extern "C" {
void PbRegister_blurMacGrid()
{
  KEEP_UNUSED(_RP_blurMacGrid);
}
}

int blurRealGrid(Grid<Real> &oG, Grid<Real> &tG, float si)
{
  return blurGrid<Real>(oG, tG, si);
}
static PyObject *_W_21(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "blurRealGrid", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &oG = *_args.getPtr<Grid<Real>>("oG", 0, &_lock);
      Grid<Real> &tG = *_args.getPtr<Grid<Real>>("tG", 1, &_lock);
      float si = _args.get<float>("si", 2, &_lock);
      _retval = toPy(blurRealGrid(oG, tG, si));
      _args.check();
    }
    pbFinalizePlugin(parent, "blurRealGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("blurRealGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_blurRealGrid("", "blurRealGrid", _W_21);
extern "C" {
void PbRegister_blurRealGrid()
{
  KEEP_UNUSED(_RP_blurRealGrid);
}
}

}  // namespace Manta
