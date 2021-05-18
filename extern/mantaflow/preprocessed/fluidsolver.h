

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
 * Main class for the fluid solver
 *
 ******************************************************************************/

#ifndef _FLUIDSOLVER_H
#define _FLUIDSOLVER_H

#include "manta.h"
#include "vectorbase.h"
#include "vector4d.h"
#include <vector>
#include <map>

namespace Manta {

//! Encodes grid size, timstep etc.

class FluidSolver : public PbClass {
 public:
  FluidSolver(Vec3i gridSize, int dim = 3, int fourthDim = -1);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "FluidSolver::FluidSolver", !noTiming);
      {
        ArgLocker _lock;
        Vec3i gridSize = _args.get<Vec3i>("gridSize", 0, &_lock);
        int dim = _args.getOpt<int>("dim", 1, 3, &_lock);
        int fourthDim = _args.getOpt<int>("fourthDim", 2, -1, &_lock);
        obj = new FluidSolver(gridSize, dim, fourthDim);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "FluidSolver::FluidSolver", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::FluidSolver", e.what());
      return -1;
    }
  }

  virtual ~FluidSolver();

  // accessors
  Vec3i getGridSize()
  {
    return mGridSize;
  }
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "FluidSolver::getGridSize", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getGridSize());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "FluidSolver::getGridSize", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::getGridSize", e.what());
      return 0;
    }
  }

  inline Real getDt() const
  {
    return mDt;
  }
  inline Real getDx() const
  {
    return 1.0 / mGridSize.max();
  }
  inline Real getTime() const
  {
    return mTimeTotal;
  }

  //! Check dimensionality
  inline bool is2D() const
  {
    return mDim == 2;
  }
  //! Check dimensionality (3d or above)
  inline bool is3D() const
  {
    return mDim == 3;
  }

  void printMemInfo();
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "FluidSolver::printMemInfo", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->printMemInfo();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "FluidSolver::printMemInfo", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::printMemInfo", e.what());
      return 0;
    }
  }

  //! Advance the solver one timestep, update GUI if present
  void step();
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "FluidSolver::step", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->step();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "FluidSolver::step", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::step", e.what());
      return 0;
    }
  }

  //! Update the timestep size based on given maximal velocity magnitude
  void adaptTimestep(Real maxVel);
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "FluidSolver::adaptTimestep", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        Real maxVel = _args.get<Real>("maxVel", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->adaptTimestep(maxVel);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "FluidSolver::adaptTimestep", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::adaptTimestep", e.what());
      return 0;
    }
  }

  //! create a object with the solver as its parent
  PbClass *create(PbType type, PbTypeVec T = PbTypeVec(), const std::string &name = "");
  static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "FluidSolver::create", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        PbType type = _args.get<PbType>("type", 0, &_lock);
        PbTypeVec T = _args.getOpt<PbTypeVec>("T", 1, PbTypeVec(), &_lock);
        const std::string &name = _args.getOpt<std::string>("name", 2, "", &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->create(type, T, name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "FluidSolver::create", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("FluidSolver::create", e.what());
      return 0;
    }
  }

  // temp grid and plugin functions: you shouldn't call this manually
  template<class T> T *getGridPointer();
  template<class T> void freeGridPointer(T *ptr);

  //! expose animation time to python
  Real mDt;
  static PyObject *_GET_mDt(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mDt);
  }
  static int _SET_mDt(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mDt = fromPy<Real>(val);
    return 0;
  }

  Real mTimeTotal;
  static PyObject *_GET_mTimeTotal(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mTimeTotal);
  }
  static int _SET_mTimeTotal(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mTimeTotal = fromPy<Real>(val);
    return 0;
  }

  int mFrame;
  static PyObject *_GET_mFrame(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mFrame);
  }
  static int _SET_mFrame(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mFrame = fromPy<int>(val);
    return 0;
  }

  //! parameters for adaptive time stepping
  Real mCflCond;
  static PyObject *_GET_mCflCond(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mCflCond);
  }
  static int _SET_mCflCond(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mCflCond = fromPy<Real>(val);
    return 0;
  }

  Real mDtMin;
  static PyObject *_GET_mDtMin(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mDtMin);
  }
  static int _SET_mDtMin(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mDtMin = fromPy<Real>(val);
    return 0;
  }

  Real mDtMax;
  static PyObject *_GET_mDtMax(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mDtMax);
  }
  static int _SET_mDtMax(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mDtMax = fromPy<Real>(val);
    return 0;
  }

  Real mFrameLength;
  static PyObject *_GET_mFrameLength(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mFrameLength);
  }
  static int _SET_mFrameLength(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mFrameLength = fromPy<Real>(val);
    return 0;
  }

  //! Per frame duration. Blender needs access in order to restore value in new solver object
  Real mTimePerFrame;
  static PyObject *_GET_mTimePerFrame(PyObject *self, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    return toPy(pbo->mTimePerFrame);
  }
  static int _SET_mTimePerFrame(PyObject *self, PyObject *val, void *cl)
  {
    FluidSolver *pbo = dynamic_cast<FluidSolver *>(Pb::objFromPy(self));
    pbo->mTimePerFrame = fromPy<Real>(val);
    return 0;
  }

 protected:
  Vec3i mGridSize;
  const int mDim;
  bool mLockDt;

  //! subclass for managing grid memory
  //! stored as a stack to allow fast allocation
  template<class T> struct GridStorage {
    GridStorage() : used(0)
    {
    }
    T *get(Vec3i size);
    void free();
    void release(T *ptr);

    std::vector<T *> grids;
    int used;
  };

  //! memory for regular (3d) grids
  GridStorage<int> mGridsInt;
  GridStorage<Real> mGridsReal;
  GridStorage<Vec3> mGridsVec;

  //! 4d data section, only required for simulations working with space-time data

 public:
  //! 4D enabled? note, there's intentionally no "is4D" function, there are only 3D solvers that
  //! also support 4D of a certain size
  inline bool supports4D() const
  {
    return mFourthDim > 0;
  }
  //! fourth dimension size
  inline int getFourthDim() const
  {
    return mFourthDim;
  }
  //! 4d data allocation
  template<class T> T *getGrid4dPointer();
  template<class T> void freeGrid4dPointer(T *ptr);

 protected:
  //! 4d size. Note - 4d is not treated like going from 2d to 3d! 4D grids are a separate data
  //! type. Normally all grids are forced to have the same size. In contrast, a solver can create
  //! and work with 3D as well as 4D grids, when fourth-dim is >0.
  int mFourthDim;

  //! 4d grid storage
  GridStorage<Vec4> mGridsVec4;
  GridStorage<int> mGrids4dInt;
  GridStorage<Real> mGrids4dReal;
  GridStorage<Vec3> mGrids4dVec;
  GridStorage<Vec4> mGrids4dVec4;

 public:
  PbArgs _args;
}
#define _C_FluidSolver
;

}  // namespace Manta

#endif
