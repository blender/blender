

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
 * moving obstacles
 *
 ******************************************************************************/

#ifndef _MOVINGOBS_H
#define _MOVINGOBS_H

#include "shapes.h"
#include "particle.h"

namespace Manta {

//! Moving obstacle composed of basic shapes
class MovingObstacle : public PbClass {
 public:
  MovingObstacle(FluidSolver *parent, int emptyType = FlagGrid::TypeEmpty);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "MovingObstacle::MovingObstacle", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        int emptyType = _args.getOpt<int>("emptyType", 1, FlagGrid::TypeEmpty, &_lock);
        obj = new MovingObstacle(parent, emptyType);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "MovingObstacle::MovingObstacle", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("MovingObstacle::MovingObstacle", e.what());
      return -1;
    }
  }

  void add(Shape *shape);
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MovingObstacle *pbo = dynamic_cast<MovingObstacle *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MovingObstacle::add", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Shape *shape = _args.getPtr<Shape>("shape", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->add(shape);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MovingObstacle::add", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MovingObstacle::add", e.what());
      return 0;
    }
  }

  //! If t in [t0,t1], apply linear motion path from p0 to p1
  void moveLinear(Real t,
                  Real t0,
                  Real t1,
                  Vec3 p0,
                  Vec3 p1,
                  FlagGrid &flags,
                  MACGrid &vel,
                  bool smooth = true);
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MovingObstacle *pbo = dynamic_cast<MovingObstacle *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MovingObstacle::moveLinear", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real t = _args.get<Real>("t", 0, &_lock);
        Real t0 = _args.get<Real>("t0", 1, &_lock);
        Real t1 = _args.get<Real>("t1", 2, &_lock);
        Vec3 p0 = _args.get<Vec3>("p0", 3, &_lock);
        Vec3 p1 = _args.get<Vec3>("p1", 4, &_lock);
        FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 5, &_lock);
        MACGrid &vel = *_args.getPtr<MACGrid>("vel", 6, &_lock);
        bool smooth = _args.getOpt<bool>("smooth", 7, true, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->moveLinear(t, t0, t1, p0, p1, flags, vel, smooth);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MovingObstacle::moveLinear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MovingObstacle::moveLinear", e.what());
      return 0;
    }
  }

  //! Compute levelset, and project FLIP particles outside obstacles
  void projectOutside(FlagGrid &flags, BasicParticleSystem &flip);
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      MovingObstacle *pbo = dynamic_cast<MovingObstacle *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "MovingObstacle::projectOutside", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        BasicParticleSystem &flip = *_args.getPtr<BasicParticleSystem>("flip", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->projectOutside(flags, flip);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "MovingObstacle::projectOutside", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("MovingObstacle::projectOutside", e.what());
      return 0;
    }
  }

 protected:
  std::vector<Shape *> mShapes;
  int mEmptyType;
  int mID;
  static int sIDcnt;

 public:
  PbArgs _args;
}
#define _C_MovingObstacle
;

}  // namespace Manta
#endif
