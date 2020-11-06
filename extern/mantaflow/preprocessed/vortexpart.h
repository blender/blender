

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
 * Vortex particles
 * (warning, the vortex methods are currently experimental, and not fully supported!)
 *
 ******************************************************************************/

#ifndef _VORTEXPART_H
#define _VORTEXPART_H

#include "particle.h"

namespace Manta {
class Mesh;

struct VortexParticleData {
  VortexParticleData() : pos(0.0), vorticity(0.0), sigma(0), flag(0)
  {
  }
  VortexParticleData(const Vec3 &p, const Vec3 &v, Real sig)
      : pos(p), vorticity(v), sigma(sig), flag(0)
  {
  }
  Vec3 pos, vorticity;
  Real sigma;
  int flag;
  static ParticleBase::SystemType getType()
  {
    return ParticleBase::VORTEX;
  }
};

//! Vortex particles
class VortexParticleSystem : public ParticleSystem<VortexParticleData> {
 public:
  VortexParticleSystem(FluidSolver *parent);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "VortexParticleSystem::VortexParticleSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new VortexParticleSystem(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "VortexParticleSystem::VortexParticleSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("VortexParticleSystem::VortexParticleSystem", e.what());
      return -1;
    }
  }

  void advectSelf(Real scale = 1.0, int integrationMode = IntRK4);
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      VortexParticleSystem *pbo = dynamic_cast<VortexParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "VortexParticleSystem::advectSelf", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real scale = _args.getOpt<Real>("scale", 0, 1.0, &_lock);
        int integrationMode = _args.getOpt<int>("integrationMode", 1, IntRK4, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->advectSelf(scale, integrationMode);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "VortexParticleSystem::advectSelf", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("VortexParticleSystem::advectSelf", e.what());
      return 0;
    }
  }

  void applyToMesh(Mesh &mesh, Real scale = 1.0, int integrationMode = IntRK4);
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      VortexParticleSystem *pbo = dynamic_cast<VortexParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "VortexParticleSystem::applyToMesh", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Mesh &mesh = *_args.getPtr<Mesh>("mesh", 0, &_lock);
        Real scale = _args.getOpt<Real>("scale", 1, 1.0, &_lock);
        int integrationMode = _args.getOpt<int>("integrationMode", 2, IntRK4, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->applyToMesh(mesh, scale, integrationMode);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "VortexParticleSystem::applyToMesh", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("VortexParticleSystem::applyToMesh", e.what());
      return 0;
    }
  }

  virtual ParticleBase *clone();
 public:
  PbArgs _args;
}
#define _C_VortexParticleSystem
;

}  // namespace Manta

#endif
