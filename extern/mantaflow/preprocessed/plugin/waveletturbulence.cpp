

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
 * Functions for calculating wavelet turbulence,
 * plus helpers to compute vorticity, and strain rate magnitude
 *
 ******************************************************************************/

#include "vectorbase.h"
#include "shapes.h"
#include "commonkernels.h"
#include "noisefield.h"

using namespace std;

namespace Manta {

//*****************************************************************************

// first some fairly generic interpolation functions for grids with multiple sizes

//! same as in grid.h , but takes an additional optional "desired" size
inline void calcGridSizeFactorMod(
    Vec3i s1, Vec3i s2, Vec3i optSize, Vec3 scale, Vec3 &sourceFactor, Vec3 &retOff)
{
  for (int c = 0; c < 3; c++) {
    if (optSize[c] > 0) {
      s2[c] = optSize[c];
    }
  }
  sourceFactor = calcGridSizeFactor(s1, s2) / scale;
  retOff = -retOff * sourceFactor + sourceFactor * 0.5;
}

void interpolateGrid(Grid<Real> &target,
                     const Grid<Real> &source,
                     Vec3 scale = Vec3(1.),
                     Vec3 offset = Vec3(0.),
                     Vec3i size = Vec3i(-1, -1, -1),
                     int orderSpace = 1)
{
  Vec3 sourceFactor(1.), off2 = offset;
  calcGridSizeFactorMod(source.getSize(), target.getSize(), size, scale, sourceFactor, off2);

  // a brief note on a mantaflow specialty: the target grid has to be the first argument here!
  // the parent fluidsolver object is taken from the first grid, and it determines the size of the
  // loop for the kernel call. as we're writing into target, it's important to loop exactly over
  // all cells of the target grid... (note, when calling the plugin in python, it doesnt matter
  // anymore).

  // sourceFactor offset necessary to shift eval points by half a small cell width
  knInterpolateGridTempl<Real>(target, source, sourceFactor, off2, orderSpace);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "interpolateGrid", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 0, &_lock);
      const Grid<Real> &source = *_args.getPtr<Grid<Real>>("source", 1, &_lock);
      Vec3 scale = _args.getOpt<Vec3>("scale", 2, Vec3(1.), &_lock);
      Vec3 offset = _args.getOpt<Vec3>("offset", 3, Vec3(0.), &_lock);
      Vec3i size = _args.getOpt<Vec3i>("size", 4, Vec3i(-1, -1, -1), &_lock);
      int orderSpace = _args.getOpt<int>("orderSpace", 5, 1, &_lock);
      _retval = getPyNone();
      interpolateGrid(target, source, scale, offset, size, orderSpace);
      _args.check();
    }
    pbFinalizePlugin(parent, "interpolateGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("interpolateGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_interpolateGrid("", "interpolateGrid", _W_0);
extern "C" {
void PbRegister_interpolateGrid()
{
  KEEP_UNUSED(_RP_interpolateGrid);
}
}

void interpolateGridVec3(Grid<Vec3> &target,
                         const Grid<Vec3> &source,
                         Vec3 scale = Vec3(1.),
                         Vec3 offset = Vec3(0.),
                         Vec3i size = Vec3i(-1, -1, -1),
                         int orderSpace = 1)
{
  Vec3 sourceFactor(1.), off2 = offset;
  calcGridSizeFactorMod(source.getSize(), target.getSize(), size, scale, sourceFactor, off2);
  knInterpolateGridTempl<Vec3>(target, source, sourceFactor, off2, orderSpace);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "interpolateGridVec3", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 0, &_lock);
      const Grid<Vec3> &source = *_args.getPtr<Grid<Vec3>>("source", 1, &_lock);
      Vec3 scale = _args.getOpt<Vec3>("scale", 2, Vec3(1.), &_lock);
      Vec3 offset = _args.getOpt<Vec3>("offset", 3, Vec3(0.), &_lock);
      Vec3i size = _args.getOpt<Vec3i>("size", 4, Vec3i(-1, -1, -1), &_lock);
      int orderSpace = _args.getOpt<int>("orderSpace", 5, 1, &_lock);
      _retval = getPyNone();
      interpolateGridVec3(target, source, scale, offset, size, orderSpace);
      _args.check();
    }
    pbFinalizePlugin(parent, "interpolateGridVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("interpolateGridVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_interpolateGridVec3("", "interpolateGridVec3", _W_1);
extern "C" {
void PbRegister_interpolateGridVec3()
{
  KEEP_UNUSED(_RP_interpolateGridVec3);
}
}

//! interpolate a mac velocity grid from one size to another size

struct KnInterpolateMACGrid : public KernelBase {
  KnInterpolateMACGrid(MACGrid &target,
                       const MACGrid &source,
                       const Vec3 &sourceFactor,
                       const Vec3 &off,
                       int orderSpace)
      : KernelBase(&target, 0),
        target(target),
        source(source),
        sourceFactor(sourceFactor),
        off(off),
        orderSpace(orderSpace)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 MACGrid &target,
                 const MACGrid &source,
                 const Vec3 &sourceFactor,
                 const Vec3 &off,
                 int orderSpace) const
  {
    Vec3 pos = Vec3(i, j, k) * sourceFactor + off;

    Real vx = source.getInterpolatedHi(pos - Vec3(0.5, 0, 0), orderSpace)[0];
    Real vy = source.getInterpolatedHi(pos - Vec3(0, 0.5, 0), orderSpace)[1];
    Real vz = 0.f;
    if (source.is3D())
      vz = source.getInterpolatedHi(pos - Vec3(0, 0, 0.5), orderSpace)[2];

    target(i, j, k) = Vec3(vx, vy, vz);
  }
  inline MACGrid &getArg0()
  {
    return target;
  }
  typedef MACGrid type0;
  inline const MACGrid &getArg1()
  {
    return source;
  }
  typedef MACGrid type1;
  inline const Vec3 &getArg2()
  {
    return sourceFactor;
  }
  typedef Vec3 type2;
  inline const Vec3 &getArg3()
  {
    return off;
  }
  typedef Vec3 type3;
  inline int &getArg4()
  {
    return orderSpace;
  }
  typedef int type4;
  void runMessage()
  {
    debMsg("Executing kernel KnInterpolateMACGrid ", 3);
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
            op(i, j, k, target, source, sourceFactor, off, orderSpace);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, target, source, sourceFactor, off, orderSpace);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &target;
  const MACGrid &source;
  const Vec3 &sourceFactor;
  const Vec3 &off;
  int orderSpace;
};

void interpolateMACGrid(MACGrid &target,
                        const MACGrid &source,
                        Vec3 scale = Vec3(1.),
                        Vec3 offset = Vec3(0.),
                        Vec3i size = Vec3i(-1, -1, -1),
                        int orderSpace = 1)
{
  Vec3 sourceFactor(1.), off2 = offset;
  calcGridSizeFactorMod(source.getSize(), target.getSize(), size, scale, sourceFactor, off2);
  KnInterpolateMACGrid(target, source, sourceFactor, off2, orderSpace);
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "interpolateMACGrid", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      MACGrid &target = *_args.getPtr<MACGrid>("target", 0, &_lock);
      const MACGrid &source = *_args.getPtr<MACGrid>("source", 1, &_lock);
      Vec3 scale = _args.getOpt<Vec3>("scale", 2, Vec3(1.), &_lock);
      Vec3 offset = _args.getOpt<Vec3>("offset", 3, Vec3(0.), &_lock);
      Vec3i size = _args.getOpt<Vec3i>("size", 4, Vec3i(-1, -1, -1), &_lock);
      int orderSpace = _args.getOpt<int>("orderSpace", 5, 1, &_lock);
      _retval = getPyNone();
      interpolateMACGrid(target, source, scale, offset, size, orderSpace);
      _args.check();
    }
    pbFinalizePlugin(parent, "interpolateMACGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("interpolateMACGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_interpolateMACGrid("", "interpolateMACGrid", _W_2);
extern "C" {
void PbRegister_interpolateMACGrid()
{
  KEEP_UNUSED(_RP_interpolateMACGrid);
}
}

//*****************************************************************************

//! Apply vector noise to grid, this is a simplified version - no position scaling or UVs

struct knApplySimpleNoiseVec3 : public KernelBase {
  knApplySimpleNoiseVec3(const FlagGrid &flags,
                         Grid<Vec3> &target,
                         const WaveletNoiseField &noise,
                         Real scale,
                         const Grid<Real> *weight)
      : KernelBase(&flags, 0),
        flags(flags),
        target(target),
        noise(noise),
        scale(scale),
        weight(weight)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Vec3> &target,
                 const WaveletNoiseField &noise,
                 Real scale,
                 const Grid<Real> *weight) const
  {
    if (!flags.isFluid(i, j, k))
      return;
    Real factor = 1;
    if (weight)
      factor = (*weight)(i, j, k);
    target(i, j, k) += noise.evaluateCurl(Vec3(i, j, k) + Vec3(0.5)) * scale * factor;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Vec3> &getArg1()
  {
    return target;
  }
  typedef Grid<Vec3> type1;
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
  inline const Grid<Real> *getArg4()
  {
    return weight;
  }
  typedef Grid<Real> type4;
  void runMessage()
  {
    debMsg("Executing kernel knApplySimpleNoiseVec3 ", 3);
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
            op(i, j, k, flags, target, noise, scale, weight);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, target, noise, scale, weight);
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
  Grid<Vec3> &target;
  const WaveletNoiseField &noise;
  Real scale;
  const Grid<Real> *weight;
};

void applySimpleNoiseVec3(const FlagGrid &flags,
                          Grid<Vec3> &target,
                          const WaveletNoiseField &noise,
                          Real scale = 1.0,
                          const Grid<Real> *weight = nullptr)
{
  // note - passing a MAC grid here is slightly inaccurate, we should evaluate each component
  // separately
  knApplySimpleNoiseVec3(flags, target, noise, scale, weight);
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "applySimpleNoiseVec3", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1.0, &_lock);
      const Grid<Real> *weight = _args.getPtrOpt<Grid<Real>>("weight", 4, nullptr, &_lock);
      _retval = getPyNone();
      applySimpleNoiseVec3(flags, target, noise, scale, weight);
      _args.check();
    }
    pbFinalizePlugin(parent, "applySimpleNoiseVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("applySimpleNoiseVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_applySimpleNoiseVec3("", "applySimpleNoiseVec3", _W_3);
extern "C" {
void PbRegister_applySimpleNoiseVec3()
{
  KEEP_UNUSED(_RP_applySimpleNoiseVec3);
}
}

//! Simple noise for a real grid , follows applySimpleNoiseVec3

struct knApplySimpleNoiseReal : public KernelBase {
  knApplySimpleNoiseReal(const FlagGrid &flags,
                         Grid<Real> &target,
                         const WaveletNoiseField &noise,
                         Real scale,
                         const Grid<Real> *weight)
      : KernelBase(&flags, 0),
        flags(flags),
        target(target),
        noise(noise),
        scale(scale),
        weight(weight)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &target,
                 const WaveletNoiseField &noise,
                 Real scale,
                 const Grid<Real> *weight) const
  {
    if (!flags.isFluid(i, j, k))
      return;
    Real factor = 1;
    if (weight)
      factor = (*weight)(i, j, k);
    target(i, j, k) += noise.evaluate(Vec3(i, j, k) + Vec3(0.5)) * scale * factor;
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
  inline const Grid<Real> *getArg4()
  {
    return weight;
  }
  typedef Grid<Real> type4;
  void runMessage()
  {
    debMsg("Executing kernel knApplySimpleNoiseReal ", 3);
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
            op(i, j, k, flags, target, noise, scale, weight);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, target, noise, scale, weight);
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
  const WaveletNoiseField &noise;
  Real scale;
  const Grid<Real> *weight;
};

void applySimpleNoiseReal(const FlagGrid &flags,
                          Grid<Real> &target,
                          const WaveletNoiseField &noise,
                          Real scale = 1.0,
                          const Grid<Real> *weight = nullptr)
{
  knApplySimpleNoiseReal(flags, target, noise, scale, weight);
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "applySimpleNoiseReal", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1.0, &_lock);
      const Grid<Real> *weight = _args.getPtrOpt<Grid<Real>>("weight", 4, nullptr, &_lock);
      _retval = getPyNone();
      applySimpleNoiseReal(flags, target, noise, scale, weight);
      _args.check();
    }
    pbFinalizePlugin(parent, "applySimpleNoiseReal", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("applySimpleNoiseReal", e.what());
    return 0;
  }
}
static const Pb::Register _RP_applySimpleNoiseReal("", "applySimpleNoiseReal", _W_4);
extern "C" {
void PbRegister_applySimpleNoiseReal()
{
  KEEP_UNUSED(_RP_applySimpleNoiseReal);
}
}

//! Apply vector-based wavelet noise to target grid
//! This is the version with more functionality - supports uv grids, and on-the-fly interpolation
//! of input grids.

struct knApplyNoiseVec3 : public KernelBase {
  knApplyNoiseVec3(const FlagGrid &flags,
                   Grid<Vec3> &target,
                   const WaveletNoiseField &noise,
                   Real scale,
                   Real scaleSpatial,
                   const Grid<Real> *weight,
                   const Grid<Vec3> *uv,
                   bool uvInterpol,
                   const Vec3 &sourceFactor)
      : KernelBase(&flags, 0),
        flags(flags),
        target(target),
        noise(noise),
        scale(scale),
        scaleSpatial(scaleSpatial),
        weight(weight),
        uv(uv),
        uvInterpol(uvInterpol),
        sourceFactor(sourceFactor)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Vec3> &target,
                 const WaveletNoiseField &noise,
                 Real scale,
                 Real scaleSpatial,
                 const Grid<Real> *weight,
                 const Grid<Vec3> *uv,
                 bool uvInterpol,
                 const Vec3 &sourceFactor) const
  {
    if (!flags.isFluid(i, j, k))
      return;

    // get weighting, interpolate if necessary
    Real w = 1;
    if (weight) {
      if (!uvInterpol) {
        w = (*weight)(i, j, k);
      }
      else {
        w = weight->getInterpolated(Vec3(i, j, k) * sourceFactor);
      }
    }

    // compute position where to evaluate the noise
    Vec3 pos = Vec3(i, j, k) + Vec3(0.5);
    if (uv) {
      if (!uvInterpol) {
        pos = (*uv)(i, j, k);
      }
      else {
        pos = uv->getInterpolated(Vec3(i, j, k) * sourceFactor);
        // uv coordinates are in local space - so we need to adjust the values of the positions
        pos /= sourceFactor;
      }
    }
    pos *= scaleSpatial;

    Vec3 noiseVec3 = noise.evaluateCurl(pos) * scale * w;
    // noiseVec3=pos; // debug , show interpolated positions
    target(i, j, k) += noiseVec3;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Vec3> &getArg1()
  {
    return target;
  }
  typedef Grid<Vec3> type1;
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
  inline Real &getArg4()
  {
    return scaleSpatial;
  }
  typedef Real type4;
  inline const Grid<Real> *getArg5()
  {
    return weight;
  }
  typedef Grid<Real> type5;
  inline const Grid<Vec3> *getArg6()
  {
    return uv;
  }
  typedef Grid<Vec3> type6;
  inline bool &getArg7()
  {
    return uvInterpol;
  }
  typedef bool type7;
  inline const Vec3 &getArg8()
  {
    return sourceFactor;
  }
  typedef Vec3 type8;
  void runMessage()
  {
    debMsg("Executing kernel knApplyNoiseVec3 ", 3);
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
               flags,
               target,
               noise,
               scale,
               scaleSpatial,
               weight,
               uv,
               uvInterpol,
               sourceFactor);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i,
             j,
             k,
             flags,
             target,
             noise,
             scale,
             scaleSpatial,
             weight,
             uv,
             uvInterpol,
             sourceFactor);
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
  Grid<Vec3> &target;
  const WaveletNoiseField &noise;
  Real scale;
  Real scaleSpatial;
  const Grid<Real> *weight;
  const Grid<Vec3> *uv;
  bool uvInterpol;
  const Vec3 &sourceFactor;
};

void applyNoiseVec3(const FlagGrid &flags,
                    Grid<Vec3> &target,
                    const WaveletNoiseField &noise,
                    Real scale = 1.0,
                    Real scaleSpatial = 1.0,
                    const Grid<Real> *weight = nullptr,
                    const Grid<Vec3> *uv = nullptr)
{
  // check whether the uv grid has a different resolution
  bool uvInterpol = false;
  // and pre-compute conversion (only used if uvInterpol==true)
  // used for both uv and weight grid...
  Vec3 sourceFactor = Vec3(1.);
  if (uv) {
    uvInterpol = (target.getSize() != uv->getSize());
    sourceFactor = calcGridSizeFactor(uv->getSize(), target.getSize());
  }
  else if (weight) {
    uvInterpol = (target.getSize() != weight->getSize());
    sourceFactor = calcGridSizeFactor(weight->getSize(), target.getSize());
  }
  if (uv && weight)
    assertMsg(uv->getSize() == weight->getSize(), "UV and weight grid have to match!");

  // note - passing a MAC grid here is slightly inaccurate, we should evaluate each component
  // separately
  knApplyNoiseVec3(
      flags, target, noise, scale, scaleSpatial, weight, uv, uvInterpol, sourceFactor);
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "applyNoiseVec3", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      const WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 2, &_lock);
      Real scale = _args.getOpt<Real>("scale", 3, 1.0, &_lock);
      Real scaleSpatial = _args.getOpt<Real>("scaleSpatial", 4, 1.0, &_lock);
      const Grid<Real> *weight = _args.getPtrOpt<Grid<Real>>("weight", 5, nullptr, &_lock);
      const Grid<Vec3> *uv = _args.getPtrOpt<Grid<Vec3>>("uv", 6, nullptr, &_lock);
      _retval = getPyNone();
      applyNoiseVec3(flags, target, noise, scale, scaleSpatial, weight, uv);
      _args.check();
    }
    pbFinalizePlugin(parent, "applyNoiseVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("applyNoiseVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_applyNoiseVec3("", "applyNoiseVec3", _W_5);
extern "C" {
void PbRegister_applyNoiseVec3()
{
  KEEP_UNUSED(_RP_applyNoiseVec3);
}
}

//! Compute energy of a staggered velocity field (at cell center)

struct KnApplyComputeEnergy : public KernelBase {
  KnApplyComputeEnergy(const FlagGrid &flags, const MACGrid &vel, Grid<Real> &energy)
      : KernelBase(&flags, 0), flags(flags), vel(vel), energy(energy)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const FlagGrid &flags, const MACGrid &vel, Grid<Real> &energy) const
  {
    Real e = 0.f;
    if (flags.isFluid(i, j, k)) {
      Vec3 v = vel.getCentered(i, j, k);
      e = 0.5 * (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    }
    energy(i, j, k) = e;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline Grid<Real> &getArg2()
  {
    return energy;
  }
  typedef Grid<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyComputeEnergy ", 3);
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
            op(i, j, k, flags, vel, energy);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, vel, energy);
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
  const MACGrid &vel;
  Grid<Real> &energy;
};

void computeEnergy(const FlagGrid &flags, const MACGrid &vel, Grid<Real> &energy)
{
  KnApplyComputeEnergy(flags, vel, energy);
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "computeEnergy", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      Grid<Real> &energy = *_args.getPtr<Grid<Real>>("energy", 2, &_lock);
      _retval = getPyNone();
      computeEnergy(flags, vel, energy);
      _args.check();
    }
    pbFinalizePlugin(parent, "computeEnergy", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("computeEnergy", e.what());
    return 0;
  }
}
static const Pb::Register _RP_computeEnergy("", "computeEnergy", _W_6);
extern "C" {
void PbRegister_computeEnergy()
{
  KEEP_UNUSED(_RP_computeEnergy);
}
}

void computeWaveletCoeffs(Grid<Real> &input)
{
  Grid<Real> temp1(input.getParent()), temp2(input.getParent());
  WaveletNoiseField::computeCoefficients(input, temp1, temp2);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "computeWaveletCoeffs", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      Grid<Real> &input = *_args.getPtr<Grid<Real>>("input", 0, &_lock);
      _retval = getPyNone();
      computeWaveletCoeffs(input);
      _args.check();
    }
    pbFinalizePlugin(parent, "computeWaveletCoeffs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("computeWaveletCoeffs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_computeWaveletCoeffs("", "computeWaveletCoeffs", _W_7);
extern "C" {
void PbRegister_computeWaveletCoeffs()
{
  KEEP_UNUSED(_RP_computeWaveletCoeffs);
}
}

// note - alomst the same as for vorticity confinement
void computeVorticity(const MACGrid &vel, Grid<Vec3> &vorticity, Grid<Real> *norm = nullptr)
{
  Grid<Vec3> velCenter(vel.getParent());
  GetCentered(velCenter, vel);
  CurlOp(velCenter, vorticity);
  if (norm)
    GridNorm(*norm, vorticity);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "computeVorticity", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Vec3> &vorticity = *_args.getPtr<Grid<Vec3>>("vorticity", 1, &_lock);
      Grid<Real> *norm = _args.getPtrOpt<Grid<Real>>("norm", 2, nullptr, &_lock);
      _retval = getPyNone();
      computeVorticity(vel, vorticity, norm);
      _args.check();
    }
    pbFinalizePlugin(parent, "computeVorticity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("computeVorticity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_computeVorticity("", "computeVorticity", _W_8);
extern "C" {
void PbRegister_computeVorticity()
{
  KEEP_UNUSED(_RP_computeVorticity);
}
}

// note - very similar to KnComputeProductionStrain, but for use as wavelet turb weighting

struct KnComputeStrainRateMag : public KernelBase {
  KnComputeStrainRateMag(const MACGrid &vel, const Grid<Vec3> &velCenter, Grid<Real> &prod)
      : KernelBase(&vel, 1), vel(vel), velCenter(velCenter), prod(prod)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const MACGrid &vel, const Grid<Vec3> &velCenter, Grid<Real> &prod) const
  {
    // compute Sij = 1/2 * (dU_i/dx_j + dU_j/dx_i)
    Vec3 diag = Vec3(vel(i + 1, j, k).x, vel(i, j + 1, k).y, 0.) - vel(i, j, k);
    if (vel.is3D())
      diag[2] += vel(i, j, k + 1).z;
    else
      diag[2] = 0.;

    Vec3 ux = 0.5 * (velCenter(i + 1, j, k) - velCenter(i - 1, j, k));
    Vec3 uy = 0.5 * (velCenter(i, j + 1, k) - velCenter(i, j - 1, k));
    Vec3 uz;
    if (vel.is3D())
      uz = 0.5 * (velCenter(i, j, k + 1) - velCenter(i, j, k - 1));

    Real S12 = 0.5 * (ux.y + uy.x);
    Real S13 = 0.5 * (ux.z + uz.x);
    Real S23 = 0.5 * (uy.z + uz.y);
    Real S2 = square(diag.x) + square(diag.y) + square(diag.z) + 2.0 * square(S12) +
              2.0 * square(S13) + 2.0 * square(S23);
    prod(i, j, k) = S2;
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
  inline Grid<Real> &getArg2()
  {
    return prod;
  }
  typedef Grid<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel KnComputeStrainRateMag ", 3);
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
            op(i, j, k, vel, velCenter, prod);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, vel, velCenter, prod);
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
  Grid<Real> &prod;
};
void computeStrainRateMag(const MACGrid &vel, Grid<Real> &mag)
{
  Grid<Vec3> velCenter(vel.getParent());
  GetCentered(velCenter, vel);
  KnComputeStrainRateMag(vel, velCenter, mag);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "computeStrainRateMag", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Real> &mag = *_args.getPtr<Grid<Real>>("mag", 1, &_lock);
      _retval = getPyNone();
      computeStrainRateMag(vel, mag);
      _args.check();
    }
    pbFinalizePlugin(parent, "computeStrainRateMag", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("computeStrainRateMag", e.what());
    return 0;
  }
}
static const Pb::Register _RP_computeStrainRateMag("", "computeStrainRateMag", _W_9);
extern "C" {
void PbRegister_computeStrainRateMag()
{
  KEEP_UNUSED(_RP_computeStrainRateMag);
}
}

// extrapolate a real grid into a flagged region (based on initial flags)
// by default extrapolates from fluid to obstacle cells
template<class T>
void extrapolSimpleFlagsHelper(const FlagGrid &flags,
                               Grid<T> &val,
                               int distance = 4,
                               int flagFrom = FlagGrid::TypeFluid,
                               int flagTo = FlagGrid::TypeObstacle)
{
  Grid<int> tmp(flags.getParent());
  int dim = (flags.is3D() ? 3 : 2);
  const Vec3i nb[6] = {Vec3i(1, 0, 0),
                       Vec3i(-1, 0, 0),
                       Vec3i(0, 1, 0),
                       Vec3i(0, -1, 0),
                       Vec3i(0, 0, 1),
                       Vec3i(0, 0, -1)};

  // remove all fluid cells (set to 1)
  tmp.clear();
  bool foundTarget = false;
  FOR_IJK_BND(flags, 0)
  {
    if (flags(i, j, k) & flagFrom)
      tmp(Vec3i(i, j, k)) = 1;
    if (!foundTarget && (flags(i, j, k) & flagTo))
      foundTarget = true;
  }
  // optimization, skip extrapolation if we dont have any cells to extrapolate to
  if (!foundTarget) {
    debMsg("No target cells found, skipping extrapolation", 1);
    return;
  }

  // extrapolate for given distance
  for (int d = 1; d < 1 + distance; ++d) {

    // TODO, parallelize
    FOR_IJK_BND(flags, 1)
    {
      if (tmp(i, j, k) != 0)
        continue;
      if (!(flags(i, j, k) & flagTo))
        continue;

      // copy from initialized neighbors
      Vec3i p(i, j, k);
      int nbs = 0;
      T avgVal = 0.;
      for (int n = 0; n < 2 * dim; ++n) {
        if (tmp(p + nb[n]) == d) {
          avgVal += val(p + nb[n]);
          nbs++;
        }
      }

      if (nbs > 0) {
        tmp(p) = d + 1;
        val(p) = avgVal / nbs;
      }
    }

  }  // distance
}

void extrapolateSimpleFlags(const FlagGrid &flags,
                            GridBase *val,
                            int distance = 4,
                            int flagFrom = FlagGrid::TypeFluid,
                            int flagTo = FlagGrid::TypeObstacle)
{
  if (val->getType() & GridBase::TypeReal) {
    extrapolSimpleFlagsHelper<Real>(flags, *((Grid<Real> *)val), distance, flagFrom, flagTo);
  }
  else if (val->getType() & GridBase::TypeInt) {
    extrapolSimpleFlagsHelper<int>(flags, *((Grid<int> *)val), distance, flagFrom, flagTo);
  }
  else if (val->getType() & GridBase::TypeVec3) {
    extrapolSimpleFlagsHelper<Vec3>(flags, *((Grid<Vec3> *)val), distance, flagFrom, flagTo);
  }
  else
    errMsg("extrapolateSimpleFlags: Grid Type is not supported (only int, Real, Vec3)");
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "extrapolateSimpleFlags", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      GridBase *val = _args.getPtr<GridBase>("val", 1, &_lock);
      int distance = _args.getOpt<int>("distance", 2, 4, &_lock);
      int flagFrom = _args.getOpt<int>("flagFrom", 3, FlagGrid::TypeFluid, &_lock);
      int flagTo = _args.getOpt<int>("flagTo", 4, FlagGrid::TypeObstacle, &_lock);
      _retval = getPyNone();
      extrapolateSimpleFlags(flags, val, distance, flagFrom, flagTo);
      _args.check();
    }
    pbFinalizePlugin(parent, "extrapolateSimpleFlags", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("extrapolateSimpleFlags", e.what());
    return 0;
  }
}
static const Pb::Register _RP_extrapolateSimpleFlags("", "extrapolateSimpleFlags", _W_10);
extern "C" {
void PbRegister_extrapolateSimpleFlags()
{
  KEEP_UNUSED(_RP_extrapolateSimpleFlags);
}
}

//! convert vel to a centered grid, then compute its curl
void getCurl(const MACGrid &vel, Grid<Real> &vort, int comp)
{
  Grid<Vec3> velCenter(vel.getParent()), curl(vel.getParent());

  GetCentered(velCenter, vel);
  CurlOp(velCenter, curl);
  GetComponent(curl, vort, comp);
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getCurl", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      Grid<Real> &vort = *_args.getPtr<Grid<Real>>("vort", 1, &_lock);
      int comp = _args.get<int>("comp", 2, &_lock);
      _retval = getPyNone();
      getCurl(vel, vort, comp);
      _args.check();
    }
    pbFinalizePlugin(parent, "getCurl", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getCurl", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getCurl("", "getCurl", _W_11);
extern "C" {
void PbRegister_getCurl()
{
  KEEP_UNUSED(_RP_getCurl);
}
}

}  // namespace Manta
