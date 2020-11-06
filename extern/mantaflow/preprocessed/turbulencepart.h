

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
 * Turbulence particles
 *
 ******************************************************************************/

#ifndef _TURBULENCEPART_H_
#define _TURBULENCEPART_H_

#include "particle.h"
#include "noisefield.h"

namespace Manta {
class Shape;

struct TurbulenceParticleData {
  TurbulenceParticleData() : pos(0.0), color(1.), tex0(0.0), tex1(0.0), flag(0)
  {
  }
  TurbulenceParticleData(const Vec3 &p, const Vec3 &color = Vec3(1.))
      : pos(p), color(color), tex0(p), tex1(p), flag(0)
  {
  }
  Vec3 pos, color;
  Vec3 tex0, tex1;
  int flag;
  static ParticleBase::SystemType getType()
  {
    return ParticleBase::TURBULENCE;
  }
};

//! Turbulence particles
class TurbulenceParticleSystem : public ParticleSystem<TurbulenceParticleData> {
 public:
  TurbulenceParticleSystem(FluidSolver *parent, WaveletNoiseField &noise);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "TurbulenceParticleSystem::TurbulenceParticleSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        WaveletNoiseField &noise = *_args.getPtr<WaveletNoiseField>("noise", 1, &_lock);
        obj = new TurbulenceParticleSystem(parent, noise);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(
          obj->getParent(), "TurbulenceParticleSystem::TurbulenceParticleSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("TurbulenceParticleSystem::TurbulenceParticleSystem", e.what());
      return -1;
    }
  }

  void resetTexCoords(int num, const Vec3 &inflow);
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      TurbulenceParticleSystem *pbo = dynamic_cast<TurbulenceParticleSystem *>(
          Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "TurbulenceParticleSystem::resetTexCoords", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        int num = _args.get<int>("num", 0, &_lock);
        const Vec3 &inflow = _args.get<Vec3>("inflow", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->resetTexCoords(num, inflow);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "TurbulenceParticleSystem::resetTexCoords", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("TurbulenceParticleSystem::resetTexCoords", e.what());
      return 0;
    }
  }

  void seed(Shape *source, int num);
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      TurbulenceParticleSystem *pbo = dynamic_cast<TurbulenceParticleSystem *>(
          Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "TurbulenceParticleSystem::seed", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Shape *source = _args.getPtr<Shape>("source", 0, &_lock);
        int num = _args.get<int>("num", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->seed(source, num);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "TurbulenceParticleSystem::seed", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("TurbulenceParticleSystem::seed", e.what());
      return 0;
    }
  }

  void synthesize(FlagGrid &flags,
                  Grid<Real> &k,
                  int octaves = 2,
                  Real switchLength = 10.0,
                  Real L0 = 0.1,
                  Real scale = 1.0,
                  Vec3 inflowBias = 0.0);
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      TurbulenceParticleSystem *pbo = dynamic_cast<TurbulenceParticleSystem *>(
          Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "TurbulenceParticleSystem::synthesize", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        Grid<Real> &k = *_args.getPtr<Grid<Real>>("k", 1, &_lock);
        int octaves = _args.getOpt<int>("octaves", 2, 2, &_lock);
        Real switchLength = _args.getOpt<Real>("switchLength", 3, 10.0, &_lock);
        Real L0 = _args.getOpt<Real>("L0", 4, 0.1, &_lock);
        Real scale = _args.getOpt<Real>("scale", 5, 1.0, &_lock);
        Vec3 inflowBias = _args.getOpt<Vec3>("inflowBias", 6, 0.0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->synthesize(flags, k, octaves, switchLength, L0, scale, inflowBias);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "TurbulenceParticleSystem::synthesize", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("TurbulenceParticleSystem::synthesize", e.what());
      return 0;
    }
  }

  void deleteInObstacle(FlagGrid &flags);
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      TurbulenceParticleSystem *pbo = dynamic_cast<TurbulenceParticleSystem *>(
          Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "TurbulenceParticleSystem::deleteInObstacle", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->deleteInObstacle(flags);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "TurbulenceParticleSystem::deleteInObstacle", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("TurbulenceParticleSystem::deleteInObstacle", e.what());
      return 0;
    }
  }

  virtual ParticleBase *clone();

 private:
  WaveletNoiseField &noise;
 public:
  PbArgs _args;
}
#define _C_TurbulenceParticleSystem
;

}  // namespace Manta

#endif
