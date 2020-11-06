

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
 * Levelset
 *
 ******************************************************************************/

#ifndef _LEVELSET_H_
#define _LEVELSET_H_

#include "grid.h"

namespace Manta {
class Mesh;

//! Special function for levelsets
class LevelsetGrid : public Grid<Real> {
 public:
  LevelsetGrid(FluidSolver *parent, bool show = true);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "LevelsetGrid::LevelsetGrid", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        bool show = _args.getOpt<bool>("show", 1, true, &_lock);
        obj = new LevelsetGrid(parent, show);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "LevelsetGrid::LevelsetGrid", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::LevelsetGrid", e.what());
      return -1;
    }
  }

  LevelsetGrid(FluidSolver *parent, Real *data, bool show = true);

  //! reconstruct the levelset using fast marching

  void reinitMarching(const FlagGrid &flags,
                      Real maxTime = 4.0,
                      MACGrid *velTransport = nullptr,
                      bool ignoreWalls = false,
                      bool correctOuterLayer = true,
                      int obstacleType = FlagGrid::TypeObstacle);
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::reinitMarching", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        Real maxTime = _args.getOpt<Real>("maxTime", 1, 4.0, &_lock);
        MACGrid *velTransport = _args.getPtrOpt<MACGrid>("velTransport", 2, nullptr, &_lock);
        bool ignoreWalls = _args.getOpt<bool>("ignoreWalls", 3, false, &_lock);
        bool correctOuterLayer = _args.getOpt<bool>("correctOuterLayer", 4, true, &_lock);
        int obstacleType = _args.getOpt<int>("obstacleType", 5, FlagGrid::TypeObstacle, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->reinitMarching(
            flags, maxTime, velTransport, ignoreWalls, correctOuterLayer, obstacleType);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::reinitMarching", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::reinitMarching", e.what());
      return 0;
    }
  }

  //! create a triangle mesh from the levelset isosurface
  void createMesh(Mesh &mesh);
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::createMesh", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Mesh &mesh = *_args.getPtr<Mesh>("mesh", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->createMesh(mesh);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::createMesh", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::createMesh", e.what());
      return 0;
    }
  }

  //! union with another levelset
  void join(const LevelsetGrid &o);
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::join", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const LevelsetGrid &o = *_args.getPtr<LevelsetGrid>("o", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->join(o);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::join", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::join", e.what());
      return 0;
    }
  }

  void subtract(const LevelsetGrid &o,
                const FlagGrid *flags = nullptr,
                const int subtractType = 0);
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::subtract", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const LevelsetGrid &o = *_args.getPtr<LevelsetGrid>("o", 0, &_lock);
        const FlagGrid *flags = _args.getPtrOpt<FlagGrid>("flags", 1, nullptr, &_lock);
        const int subtractType = _args.getOpt<int>("subtractType", 2, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->subtract(o, flags, subtractType);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::subtract", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::subtract", e.what());
      return 0;
    }
  }

  //! initialize levelset from flags (+/- 0.5 heaviside)
  void initFromFlags(const FlagGrid &flags, bool ignoreWalls = false);
  static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::initFromFlags", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        bool ignoreWalls = _args.getOpt<bool>("ignoreWalls", 1, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->initFromFlags(flags, ignoreWalls);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::initFromFlags", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::initFromFlags", e.what());
      return 0;
    }
  }

  //! fill holes (pos cells enclosed by neg ones) up to given size with -0.5 (ie not preserving
  //! sdf)
  void fillHoles(int maxDepth = 10, int boundaryWidth = 1);
  static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::fillHoles", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        int maxDepth = _args.getOpt<int>("maxDepth", 0, 10, &_lock);
        int boundaryWidth = _args.getOpt<int>("boundaryWidth", 1, 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->fillHoles(maxDepth, boundaryWidth);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::fillHoles", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::fillHoles", e.what());
      return 0;
    }
  }

  //! flood-fill the levelset to ensure that closed obstacles are filled inside
  void floodFill(const Real value = -0.5, const bool outside = true, const int boundaryWidth = 1);
  static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      LevelsetGrid *pbo = dynamic_cast<LevelsetGrid *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "LevelsetGrid::floodFill", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        const Real value = _args.getOpt<Real>("value", 0, -0.5, &_lock);
        const bool outside = _args.getOpt<bool>("outside", 1, true, &_lock);
        const int boundaryWidth = _args.getOpt<int>("boundaryWidth", 2, 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->floodFill(value, outside, boundaryWidth);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "LevelsetGrid::floodFill", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("LevelsetGrid::floodFill", e.what());
      return 0;
    }
  }

  static Real invalidTimeValue();
 public:
  PbArgs _args;
}
#define _C_LevelsetGrid
;

}  // namespace Manta
#endif
