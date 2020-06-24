

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
 * Grid representation
 *
 ******************************************************************************/

#ifndef _GRID4D_H
#define _GRID4D_H

#include "manta.h"
#include "vectorbase.h"
#include "vector4d.h"
#include "kernel.h"

namespace Manta {

//! Base class for all grids
class Grid4dBase : public PbClass {
 public:
  enum Grid4dType { TypeNone = 0, TypeReal = 1, TypeInt = 2, TypeVec3 = 4, TypeVec4 = 8 };

  Grid4dBase(FluidSolver *parent);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "Grid4dBase::Grid4dBase", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new Grid4dBase(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "Grid4dBase::Grid4dBase", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::Grid4dBase", e.what());
      return -1;
    }
  }

  //! Get the grids X dimension
  inline int getSizeX() const
  {
    return mSize.x;
  }
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::getSizeX", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getSizeX());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::getSizeX", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::getSizeX", e.what());
      return 0;
    }
  }

  //! Get the grids Y dimension
  inline int getSizeY() const
  {
    return mSize.y;
  }
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::getSizeY", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getSizeY());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::getSizeY", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::getSizeY", e.what());
      return 0;
    }
  }

  //! Get the grids Z dimension
  inline int getSizeZ() const
  {
    return mSize.z;
  }
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::getSizeZ", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getSizeZ());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::getSizeZ", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::getSizeZ", e.what());
      return 0;
    }
  }

  //! Get the grids T dimension
  inline int getSizeT() const
  {
    return mSize.t;
  }
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::getSizeT", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getSizeT());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::getSizeT", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::getSizeT", e.what());
      return 0;
    }
  }

  //! Get the grids dimensions
  inline Vec4i getSize() const
  {
    return mSize;
  }
  static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::getSize", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getSize());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::getSize", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::getSize", e.what());
      return 0;
    }
  }

  //! Get Stride in X dimension
  inline IndexInt getStrideX() const
  {
    return 1;
  }
  //! Get Stride in Y dimension
  inline IndexInt getStrideY() const
  {
    return mSize.x;
  }
  //! Get Stride in Z dimension
  inline IndexInt getStrideZ() const
  {
    return mStrideZ;
  }
  //! Get Stride in T dimension
  inline IndexInt getStrideT() const
  {
    return mStrideT;
  }

  inline Real getDx()
  {
    return mDx;
  }

  //! Check if indices are within bounds, otherwise error (should only be called when debugging)
  inline void checkIndex(int i, int j, int k, int t) const;
  //! Check if indices are within bounds, otherwise error (should only be called when debugging)
  inline void checkIndex(IndexInt idx) const;
  //! Check if index is within given boundaries
  inline bool isInBounds(const Vec4i &p, int bnd) const;
  //! Check if index is within given boundaries
  inline bool isInBounds(const Vec4i &p) const;
  //! Check if index is within given boundaries
  inline bool isInBounds(const Vec4 &p, int bnd = 0) const
  {
    return isInBounds(toVec4i(p), bnd);
  }
  //! Check if linear index is in the range of the array
  inline bool isInBounds(IndexInt idx) const;

  //! Get the type of grid
  inline Grid4dType getType() const
  {
    return mType;
  }
  //! Check dimensionality
  inline bool is3D() const
  {
    return true;
  }
  static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::is3D", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->is3D());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::is3D", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::is3D", e.what());
      return 0;
    }
  }

  inline bool is4D() const
  {
    return true;
  }
  static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4dBase *pbo = dynamic_cast<Grid4dBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4dBase::is4D", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->is4D());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4dBase::is4D", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4dBase::is4D", e.what());
      return 0;
    }
  }

  //! 3d compatibility
  inline bool isInBounds(int i, int j, int k, int t, int bnd) const
  {
    return isInBounds(Vec4i(i, j, k, t), bnd);
  }

  //! Get index into the data
  inline IndexInt index(int i, int j, int k, int t) const
  {
    DEBUG_ONLY(checkIndex(i, j, k, t));
    return (IndexInt)i + (IndexInt)mSize.x * j + (IndexInt)mStrideZ * k + (IndexInt)mStrideT * t;
  }
  //! Get index into the data
  inline IndexInt index(const Vec4i &pos) const
  {
    DEBUG_ONLY(checkIndex(pos.x, pos.y, pos.z, pos.t));
    return (IndexInt)pos.x + (IndexInt)mSize.x * pos.y + (IndexInt)mStrideZ * pos.z +
           (IndexInt)mStrideT * pos.t;
  }

 protected:
  Grid4dType mType;
  Vec4i mSize;
  Real mDx;
  // precomputed Z,T shift: to ensure 2D compatibility, always use this instead of sx*sy !
  IndexInt mStrideZ;
  IndexInt mStrideT;
 public:
  PbArgs _args;
}
#define _C_Grid4dBase
;

//! Grid class

template<class T> class Grid4d : public Grid4dBase {
 public:
  //! init new grid, values are set to zero
  Grid4d(FluidSolver *parent, bool show = true);
  static int _W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "Grid4d::Grid4d", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        bool show = _args.getOpt<bool>("show", 1, true, &_lock);
        obj = new Grid4d(parent, show);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "Grid4d::Grid4d", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::Grid4d", e.what());
      return -1;
    }
  }

  //! create new & copy content from another grid
  Grid4d(const Grid4d<T> &a);
  //! return memory to solver
  virtual ~Grid4d();

  typedef T BASETYPE;
  typedef Grid4dBase BASETYPE_GRID;

  int save(std::string name);
  static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::save", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->save(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::save", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::save", e.what());
      return 0;
    }
  }

  int load(std::string name);
  static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::load", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->load(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::load", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::load", e.what());
      return 0;
    }
  }

  //! set all cells to zero
  void clear();
  static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::clear", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clear();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::clear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::clear", e.what());
      return 0;
    }
  }

  //! all kinds of access functions, use grid(), grid[] or grid.get()
  //! access data
  inline T get(int i, int j, int k, int t) const
  {
    return mData[index(i, j, k, t)];
  }
  //! access data
  inline T &get(int i, int j, int k, int t)
  {
    return mData[index(i, j, k, t)];
  }
  //! access data
  inline T get(IndexInt idx) const
  {
    DEBUG_ONLY(checkIndex(idx));
    return mData[idx];
  }
  //! access data
  inline T get(const Vec4i &pos) const
  {
    return mData[index(pos)];
  }
  //! access data
  inline T &operator()(int i, int j, int k, int t)
  {
    return mData[index(i, j, k, t)];
  }
  //! access data
  inline T operator()(int i, int j, int k, int t) const
  {
    return mData[index(i, j, k, t)];
  }
  //! access data
  inline T &operator()(IndexInt idx)
  {
    DEBUG_ONLY(checkIndex(idx));
    return mData[idx];
  }
  //! access data
  inline T operator()(IndexInt idx) const
  {
    DEBUG_ONLY(checkIndex(idx));
    return mData[idx];
  }
  //! access data
  inline T &operator()(const Vec4i &pos)
  {
    return mData[index(pos)];
  }
  //! access data
  inline T operator()(const Vec4i &pos) const
  {
    return mData[index(pos)];
  }
  //! access data
  inline T &operator[](IndexInt idx)
  {
    DEBUG_ONLY(checkIndex(idx));
    return mData[idx];
  }
  //! access data
  inline const T operator[](IndexInt idx) const
  {
    DEBUG_ONLY(checkIndex(idx));
    return mData[idx];
  }

  // interpolated access
  inline T getInterpolated(const Vec4 &pos) const
  {
    return interpol4d<T>(mData, mSize, mStrideZ, mStrideT, pos);
  }

  // assignment / copy

  //! warning - do not use "=" for grids in python, this copies the reference! not the grid
  //! content...
  // Grid4d<T>& operator=(const Grid4d<T>& a);
  //! copy content from other grid (use this one instead of operator= !)
  Grid4d<T> &copyFrom(const Grid4d<T> &a, bool copyType = true);
  static PyObject *_W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::copyFrom", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Grid4d<T> &a = *_args.getPtr<Grid4d<T>>("a", 0, &_lock);
        bool copyType = _args.getOpt<bool>("copyType", 1, true, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->copyFrom(a, copyType));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::copyFrom", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::copyFrom", e.what());
      return 0;
    }
  }
  // old: { *this = a; }

  // helper functions to work with grids in scene files

  //! add/subtract other grid
  void add(const Grid4d<T> &a);
  static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::add", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Grid4d<T> &a = *_args.getPtr<Grid4d<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->add(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::add", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::add", e.what());
      return 0;
    }
  }

  void sub(const Grid4d<T> &a);
  static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::sub", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Grid4d<T> &a = *_args.getPtr<Grid4d<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->sub(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::sub", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::sub", e.what());
      return 0;
    }
  }

  //! set all cells to constant value
  void setConst(T s);
  static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::setConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::setConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::setConst", e.what());
      return 0;
    }
  }

  //! add constant to all grid cells
  void addConst(T s);
  static PyObject *_W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::addConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::addConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::addConst", e.what());
      return 0;
    }
  }

  //! add scaled other grid to current one (note, only "Real" factor, "T" type not supported here!)
  void addScaled(const Grid4d<T> &a, const T &factor);
  static PyObject *_W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::addScaled", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Grid4d<T> &a = *_args.getPtr<Grid4d<T>>("a", 0, &_lock);
        const T &factor = *_args.getPtr<T>("factor", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addScaled(a, factor);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::addScaled", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::addScaled", e.what());
      return 0;
    }
  }

  //! multiply contents of grid
  void mult(const Grid4d<T> &a);
  static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::mult", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Grid4d<T> &a = *_args.getPtr<Grid4d<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->mult(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::mult", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::mult", e.what());
      return 0;
    }
  }

  //! multiply each cell by a constant scalar value
  void multConst(T s);
  static PyObject *_W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::multConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        T s = _args.get<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->multConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::multConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::multConst", e.what());
      return 0;
    }
  }

  //! clamp content to range (for vec3, clamps each component separately)
  void clamp(Real min, Real max);
  static PyObject *_W_20(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::clamp", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        Real min = _args.get<Real>("min", 0, &_lock);
        Real max = _args.get<Real>("max", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clamp(min, max);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::clamp", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::clamp", e.what());
      return 0;
    }
  }

  // common compound operators
  //! get absolute max value in grid
  Real getMaxAbs();
  static PyObject *_W_21(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::getMaxAbs", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMaxAbs());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::getMaxAbs", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::getMaxAbs", e.what());
      return 0;
    }
  }

  //! get max value in grid
  Real getMax();
  static PyObject *_W_22(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::getMax", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMax());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::getMax", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::getMax", e.what());
      return 0;
    }
  }

  //! get min value in grid
  Real getMin();
  static PyObject *_W_23(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::getMin", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMin());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::getMin", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::getMin", e.what());
      return 0;
    }
  }

  //! set all boundary cells to constant value (Dirichlet)
  void setBound(T value, int boundaryWidth = 1);
  static PyObject *_W_24(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::setBound", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        T value = _args.get<T>("value", 0, &_lock);
        int boundaryWidth = _args.getOpt<int>("boundaryWidth", 1, 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setBound(value, boundaryWidth);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::setBound", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::setBound", e.what());
      return 0;
    }
  }

  //! set all boundary cells to last inner value (Neumann)
  void setBoundNeumann(int boundaryWidth = 1);
  static PyObject *_W_25(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::setBoundNeumann", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        int boundaryWidth = _args.getOpt<int>("boundaryWidth", 0, 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setBoundNeumann(boundaryWidth);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::setBoundNeumann", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::setBoundNeumann", e.what());
      return 0;
    }
  }

  //! debugging helper, print grid from Python
  void printGrid(int zSlice = -1, int tSlice = -1, bool printIndex = false, int bnd = 0);
  static PyObject *_W_26(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Grid4d *pbo = dynamic_cast<Grid4d *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Grid4d::printGrid", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        int zSlice = _args.getOpt<int>("zSlice", 0, -1, &_lock);
        int tSlice = _args.getOpt<int>("tSlice", 1, -1, &_lock);
        bool printIndex = _args.getOpt<bool>("printIndex", 2, false, &_lock);
        int bnd = _args.getOpt<int>("bnd", 3, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->printGrid(zSlice, tSlice, printIndex, bnd);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Grid4d::printGrid", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Grid4d::printGrid", e.what());
      return 0;
    }
  }

  // c++ only operators
  template<class S> Grid4d<T> &operator+=(const Grid4d<S> &a);
  template<class S> Grid4d<T> &operator+=(const S &a);
  template<class S> Grid4d<T> &operator-=(const Grid4d<S> &a);
  template<class S> Grid4d<T> &operator-=(const S &a);
  template<class S> Grid4d<T> &operator*=(const Grid4d<S> &a);
  template<class S> Grid4d<T> &operator*=(const S &a);
  template<class S> Grid4d<T> &operator/=(const Grid4d<S> &a);
  template<class S> Grid4d<T> &operator/=(const S &a);
  Grid4d<T> &safeDivide(const Grid4d<T> &a);

  //! Swap data with another grid (no actual data is moved)
  void swap(Grid4d<T> &other);

 protected:
  T *mData;
 public:
  PbArgs _args;
}
#define _C_Grid4d
;

// Python doesn't know about templates: explicit aliases needed

//! helper to compute grid conversion factor between local coordinates of two grids
inline Vec4 calcGridSizeFactor4d(Vec4i s1, Vec4i s2)
{
  return Vec4(Real(s1[0]) / s2[0], Real(s1[1]) / s2[1], Real(s1[2]) / s2[2], Real(s1[3]) / s2[3]);
}
inline Vec4 calcGridSizeFactor4d(Vec4 s1, Vec4 s2)
{
  return Vec4(s1[0] / s2[0], s1[1] / s2[1], s1[2] / s2[2], s1[3] / s2[3]);
}

// prototypes for grid plugins
void getComponent4d(const Grid4d<Vec4> &src, Grid4d<Real> &dst, int c);
void setComponent4d(const Grid4d<Real> &src, Grid4d<Vec4> &dst, int c);

//******************************************************************************
// Implementation of inline functions

inline void Grid4dBase::checkIndex(int i, int j, int k, int t) const
{
  if (i < 0 || j < 0 || i >= mSize.x || j >= mSize.y || k < 0 || k >= mSize.z || t < 0 ||
      t >= mSize.t) {
    std::ostringstream s;
    s << "Grid4d " << mName << " dim " << mSize << " : index " << i << "," << j << "," << k << ","
      << t << " out of bound ";
    errMsg(s.str());
  }
}

inline void Grid4dBase::checkIndex(IndexInt idx) const
{
  if (idx < 0 || idx >= mSize.x * mSize.y * mSize.z * mSize.t) {
    std::ostringstream s;
    s << "Grid4d " << mName << " dim " << mSize << " : index " << idx << " out of bound ";
    errMsg(s.str());
  }
}

bool Grid4dBase::isInBounds(const Vec4i &p) const
{
  return (p.x >= 0 && p.y >= 0 && p.z >= 0 && p.t >= 0 && p.x < mSize.x && p.y < mSize.y &&
          p.z < mSize.z && p.t < mSize.t);
}

bool Grid4dBase::isInBounds(const Vec4i &p, int bnd) const
{
  bool ret = (p.x >= bnd && p.y >= bnd && p.x < mSize.x - bnd && p.y < mSize.y - bnd);
  ret &= (p.z >= bnd && p.z < mSize.z - bnd);
  ret &= (p.t >= bnd && p.t < mSize.t - bnd);
  return ret;
}
//! Check if linear index is in the range of the array
bool Grid4dBase::isInBounds(IndexInt idx) const
{
  if (idx < 0 || idx >= mSize.x * mSize.y * mSize.z * mSize.t) {
    return false;
  }
  return true;
}

// note - ugly, mostly copied from normal GRID!

template<class T, class S> struct Grid4dAdd : public KernelBase {
  Grid4dAdd(Grid4d<T> &me, const Grid4d<S> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<S> &other) const
  {
    me[idx] += other[idx];
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<S> &getArg1()
  {
    return other;
  }
  typedef Grid4d<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dAdd ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<S> &other;
};
template<class T, class S> struct Grid4dSub : public KernelBase {
  Grid4dSub(Grid4d<T> &me, const Grid4d<S> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<S> &other) const
  {
    me[idx] -= other[idx];
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<S> &getArg1()
  {
    return other;
  }
  typedef Grid4d<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dSub ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<S> &other;
};
template<class T, class S> struct Grid4dMult : public KernelBase {
  Grid4dMult(Grid4d<T> &me, const Grid4d<S> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<S> &other) const
  {
    me[idx] *= other[idx];
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<S> &getArg1()
  {
    return other;
  }
  typedef Grid4d<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dMult ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<S> &other;
};
template<class T, class S> struct Grid4dDiv : public KernelBase {
  Grid4dDiv(Grid4d<T> &me, const Grid4d<S> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<S> &other) const
  {
    me[idx] /= other[idx];
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<S> &getArg1()
  {
    return other;
  }
  typedef Grid4d<S> type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dDiv ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<S> &other;
};
template<class T, class S> struct Grid4dAddScalar : public KernelBase {
  Grid4dAddScalar(Grid4d<T> &me, const S &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const S &other) const
  {
    me[idx] += other;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dAddScalar ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const S &other;
};
template<class T, class S> struct Grid4dMultScalar : public KernelBase {
  Grid4dMultScalar(Grid4d<T> &me, const S &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const S &other) const
  {
    me[idx] *= other;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const S &getArg1()
  {
    return other;
  }
  typedef S type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dMultScalar ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const S &other;
};
template<class T, class S> struct Grid4dScaledAdd : public KernelBase {
  Grid4dScaledAdd(Grid4d<T> &me, const Grid4d<T> &other, const S &factor)
      : KernelBase(&me, 0), me(me), other(other), factor(factor)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<T> &other, const S &factor) const
  {
    me[idx] += factor * other[idx];
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<T> &getArg1()
  {
    return other;
  }
  typedef Grid4d<T> type1;
  inline const S &getArg2()
  {
    return factor;
  }
  typedef S type2;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dScaledAdd ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other, factor);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<T> &other;
  const S &factor;
};

template<class T> struct Grid4dSafeDiv : public KernelBase {
  Grid4dSafeDiv(Grid4d<T> &me, const Grid4d<T> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, const Grid4d<T> &other) const
  {
    me[idx] = safeDivide(me[idx], other[idx]);
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline const Grid4d<T> &getArg1()
  {
    return other;
  }
  typedef Grid4d<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dSafeDiv ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, other);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  const Grid4d<T> &other;
};
template<class T> struct Grid4dSetConst : public KernelBase {
  Grid4dSetConst(Grid4d<T> &me, T value) : KernelBase(&me, 0), me(me), value(value)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, T value) const
  {
    me[idx] = value;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline T &getArg1()
  {
    return value;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel Grid4dSetConst ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, value);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid4d<T> &me;
  T value;
};

template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator+=(const Grid4d<S> &a)
{
  Grid4dAdd<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator+=(const S &a)
{
  Grid4dAddScalar<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator-=(const Grid4d<S> &a)
{
  Grid4dSub<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator-=(const S &a)
{
  Grid4dAddScalar<T, S>(*this, -a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator*=(const Grid4d<S> &a)
{
  Grid4dMult<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator*=(const S &a)
{
  Grid4dMultScalar<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator/=(const Grid4d<S> &a)
{
  Grid4dDiv<T, S>(*this, a);
  return *this;
}
template<class T> template<class S> Grid4d<T> &Grid4d<T>::operator/=(const S &a)
{
  S rez((S)1.0 / a);
  Grid4dMultScalar<T, S>(*this, rez);
  return *this;
}

//******************************************************************************
// Other helper functions

inline Vec4 getGradient4d(const Grid4d<Real> &data, int i, int j, int k, int t)
{
  Vec4 v;
  if (i > data.getSizeX() - 2)
    i = data.getSizeX() - 2;
  if (j > data.getSizeY() - 2)
    j = data.getSizeY() - 2;
  if (k > data.getSizeZ() - 2)
    k = data.getSizeZ() - 2;
  if (t > data.getSizeT() - 2)
    t = data.getSizeT() - 2;
  if (i < 1)
    i = 1;
  if (j < 1)
    j = 1;
  if (k < 1)
    k = 1;
  if (t < 1)
    t = 1;
  v = Vec4(data(i + 1, j, k, t) - data(i - 1, j, k, t),
           data(i, j + 1, k, t) - data(i, j - 1, k, t),
           data(i, j, k + 1, t) - data(i, j, k - 1, t),
           data(i, j, k, t + 1) - data(i, j, k, t - 1));
  return v;
}

template<class S> struct KnInterpolateGrid4dTempl : public KernelBase {
  KnInterpolateGrid4dTempl(Grid4d<S> &target,
                           Grid4d<S> &source,
                           const Vec4 &sourceFactor,
                           Vec4 offset)
      : KernelBase(&target, 0),
        target(target),
        source(source),
        sourceFactor(sourceFactor),
        offset(offset)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 int t,
                 Grid4d<S> &target,
                 Grid4d<S> &source,
                 const Vec4 &sourceFactor,
                 Vec4 offset) const
  {
    Vec4 pos = Vec4(i, j, k, t) * sourceFactor + offset;
    if (!source.is3D())
      pos[2] = 0.;  // allow 2d -> 3d
    if (!source.is4D())
      pos[3] = 0.;  // allow 3d -> 4d
    target(i, j, k, t) = source.getInterpolated(pos);
  }
  inline Grid4d<S> &getArg0()
  {
    return target;
  }
  typedef Grid4d<S> type0;
  inline Grid4d<S> &getArg1()
  {
    return source;
  }
  typedef Grid4d<S> type1;
  inline const Vec4 &getArg2()
  {
    return sourceFactor;
  }
  typedef Vec4 type2;
  inline Vec4 &getArg3()
  {
    return offset;
  }
  typedef Vec4 type3;
  void runMessage()
  {
    debMsg("Executing kernel KnInterpolateGrid4dTempl ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ
               << " "
                  " t "
               << minT << " - " << maxT,
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    if (maxT > 1) {
      for (int t = __r.begin(); t != (int)__r.end(); t++)
        for (int k = 0; k < maxZ; k++)
          for (int j = 0; j < maxY; j++)
            for (int i = 0; i < maxX; i++)
              op(i, j, k, t, target, source, sourceFactor, offset);
    }
    else if (maxZ > 1) {
      const int t = 0;
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < maxY; j++)
          for (int i = 0; i < maxX; i++)
            op(i, j, k, t, target, source, sourceFactor, offset);
    }
    else {
      const int t = 0;
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < maxX; i++)
          op(i, j, k, t, target, source, sourceFactor, offset);
    }
  }
  void run()
  {
    if (maxT > 1) {
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minT, maxT), *this);
    }
    else if (maxZ > 1) {
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    }
    else {
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
    }
  }
  Grid4d<S> &target;
  Grid4d<S> &source;
  const Vec4 &sourceFactor;
  Vec4 offset;
};

}  // namespace Manta
#endif
