

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

#include "grid.h"
#include "levelset.h"
#include "kernel.h"
#include "mantaio.h"
#include <limits>
#include <sstream>
#include <cstring>

#include "commonkernels.h"

using namespace std;
namespace Manta {

//******************************************************************************
// GridBase members

GridBase::GridBase(FluidSolver *parent) : PbClass(parent), mType(TypeNone)
{
  checkParent();
  m3D = getParent()->is3D();
}

//******************************************************************************
// Grid<T> members

// helpers to set type
template<class T> inline GridBase::GridType typeList()
{
  return GridBase::TypeNone;
}
template<> inline GridBase::GridType typeList<Real>()
{
  return GridBase::TypeReal;
}
template<> inline GridBase::GridType typeList<int>()
{
  return GridBase::TypeInt;
}
template<> inline GridBase::GridType typeList<Vec3>()
{
  return GridBase::TypeVec3;
}

template<class T>
Grid<T>::Grid(FluidSolver *parent, bool show) : GridBase(parent), externalData(false)
{
  mType = typeList<T>();
  mSize = parent->getGridSize();
  mData = parent->getGridPointer<T>();

  mStrideZ = parent->is2D() ? 0 : (mSize.x * mSize.y);
  mDx = 1.0 / mSize.max();
  clear();
  setHidden(!show);
}

template<class T>
Grid<T>::Grid(FluidSolver *parent, T *data, bool show)
    : GridBase(parent), mData(data), externalData(true)
{
  mType = typeList<T>();
  mSize = parent->getGridSize();

  mStrideZ = parent->is2D() ? 0 : (mSize.x * mSize.y);
  mDx = 1.0 / mSize.max();

  setHidden(!show);
}

template<class T> Grid<T>::Grid(const Grid<T> &a) : GridBase(a.getParent()), externalData(false)
{
  mSize = a.mSize;
  mType = a.mType;
  mStrideZ = a.mStrideZ;
  mDx = a.mDx;
  FluidSolver *gp = a.getParent();
  mData = gp->getGridPointer<T>();
  memcpy(mData, a.mData, sizeof(T) * a.mSize.x * a.mSize.y * a.mSize.z);
}

template<class T> Grid<T>::~Grid()
{
  if (!externalData) {
    mParent->freeGridPointer<T>(mData);
  }
}

template<class T> void Grid<T>::clear()
{
  memset(mData, 0, sizeof(T) * mSize.x * mSize.y * mSize.z);
}

template<class T> void Grid<T>::swap(Grid<T> &other)
{
  if (other.getSizeX() != getSizeX() || other.getSizeY() != getSizeY() ||
      other.getSizeZ() != getSizeZ())
    errMsg("Grid::swap(): Grid dimensions mismatch.");

  if (externalData || other.externalData)
    errMsg("Grid::swap(): Cannot swap if one grid stores externalData.");

  T *dswap = other.mData;
  other.mData = mData;
  mData = dswap;
}

template<class T> int Grid<T>::load(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".raw")
    return readGridRaw(name, this);
  else if (ext == ".uni")
    return readGridUni(name, this);
  else if (ext == ".vol")
    return readGridVol(name, this);
  else if (ext == ".npz")
    return readGridNumpy(name, this);
  else if (ext == ".vdb") {
    std::vector<PbClass *> grids;
    grids.push_back(this);
    return readObjectsVDB(name, &grids);
  }
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}

template<class T> int Grid<T>::save(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".raw")
    return writeGridRaw(name, this);
  else if (ext == ".uni")
    return writeGridUni(name, this);
  else if (ext == ".vol")
    return writeGridVol(name, this);
  else if (ext == ".npz")
    return writeGridNumpy(name, this);
  else if (ext == ".vdb") {
    std::vector<PbClass *> grids;
    grids.push_back(this);
    return writeObjectsVDB(name, &grids);
  }
  else if (ext == ".txt")
    return writeGridTxt(name, this);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}

//******************************************************************************
// Grid<T> operators

//! Kernel: Compute min value of Real grid

struct CompMinReal : public KernelBase {
  CompMinReal(const Grid<Real> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &val, Real &minVal)
  {
    if (val[idx] < minVal)
      minVal = val[idx];
  }
  inline operator Real()
  {
    return minVal;
  }
  inline Real &getRet()
  {
    return minVal;
  }
  inline const Grid<Real> &getArg0()
  {
    return val;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMinReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, minVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMinReal(CompMinReal &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMinReal &o)
  {
    minVal = min(minVal, o.minVal);
  }
  const Grid<Real> &val;
  Real minVal;
};

//! Kernel: Compute max value of Real grid

struct CompMaxReal : public KernelBase {
  CompMaxReal(const Grid<Real> &val)
      : KernelBase(&val, 0), val(val), maxVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &val, Real &maxVal)
  {
    if (val[idx] > maxVal)
      maxVal = val[idx];
  }
  inline operator Real()
  {
    return maxVal;
  }
  inline Real &getRet()
  {
    return maxVal;
  }
  inline const Grid<Real> &getArg0()
  {
    return val;
  }
  typedef Grid<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMaxReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, maxVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMaxReal(CompMaxReal &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMaxReal &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  const Grid<Real> &val;
  Real maxVal;
};

//! Kernel: Compute min value of int grid

struct CompMinInt : public KernelBase {
  CompMinInt(const Grid<int> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<int>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<int> &val, int &minVal)
  {
    if (val[idx] < minVal)
      minVal = val[idx];
  }
  inline operator int()
  {
    return minVal;
  }
  inline int &getRet()
  {
    return minVal;
  }
  inline const Grid<int> &getArg0()
  {
    return val;
  }
  typedef Grid<int> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMinInt ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, minVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMinInt(CompMinInt &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<int>::max())
  {
  }
  void join(const CompMinInt &o)
  {
    minVal = min(minVal, o.minVal);
  }
  const Grid<int> &val;
  int minVal;
};

//! Kernel: Compute max value of int grid

struct CompMaxInt : public KernelBase {
  CompMaxInt(const Grid<int> &val)
      : KernelBase(&val, 0), val(val), maxVal(-std::numeric_limits<int>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<int> &val, int &maxVal)
  {
    if (val[idx] > maxVal)
      maxVal = val[idx];
  }
  inline operator int()
  {
    return maxVal;
  }
  inline int &getRet()
  {
    return maxVal;
  }
  inline const Grid<int> &getArg0()
  {
    return val;
  }
  typedef Grid<int> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMaxInt ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, maxVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMaxInt(CompMaxInt &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<int>::max())
  {
  }
  void join(const CompMaxInt &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  const Grid<int> &val;
  int maxVal;
};

//! Kernel: Compute min norm of vec grid

struct CompMinVec : public KernelBase {
  CompMinVec(const Grid<Vec3> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Vec3> &val, Real &minVal)
  {
    const Real s = normSquare(val[idx]);
    if (s < minVal)
      minVal = s;
  }
  inline operator Real()
  {
    return minVal;
  }
  inline Real &getRet()
  {
    return minVal;
  }
  inline const Grid<Vec3> &getArg0()
  {
    return val;
  }
  typedef Grid<Vec3> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMinVec ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, minVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMinVec(CompMinVec &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMinVec &o)
  {
    minVal = min(minVal, o.minVal);
  }
  const Grid<Vec3> &val;
  Real minVal;
};

//! Kernel: Compute max norm of vec grid

struct CompMaxVec : public KernelBase {
  CompMaxVec(const Grid<Vec3> &val)
      : KernelBase(&val, 0), val(val), maxVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Vec3> &val, Real &maxVal)
  {
    const Real s = normSquare(val[idx]);
    if (s > maxVal)
      maxVal = s;
  }
  inline operator Real()
  {
    return maxVal;
  }
  inline Real &getRet()
  {
    return maxVal;
  }
  inline const Grid<Vec3> &getArg0()
  {
    return val;
  }
  typedef Grid<Vec3> type0;
  void runMessage()
  {
    debMsg("Executing kernel CompMaxVec ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, val, maxVal);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  CompMaxVec(CompMaxVec &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const CompMaxVec &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  const Grid<Vec3> &val;
  Real maxVal;
};

template<class T> Grid<T> &Grid<T>::copyFrom(const Grid<T> &a, bool copyType)
{
  assertMsg(a.mSize.x == mSize.x && a.mSize.y == mSize.y && a.mSize.z == mSize.z,
            "different grid resolutions " << a.mSize << " vs " << this->mSize);
  memcpy(mData, a.mData, sizeof(T) * mSize.x * mSize.y * mSize.z);
  if (copyType)
    mType = a.mType;  // copy type marker
  return *this;
}
/*template<class T> Grid<T>& Grid<T>::operator= (const Grid<T>& a) {
  note: do not use , use copyFrom instead
}*/

template<class T> struct knGridSetConstReal : public KernelBase {
  knGridSetConstReal(Grid<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, T val) const
  {
    me[idx] = val;
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridSetConstReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, val);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  T val;
};
template<class T> struct knGridAddConstReal : public KernelBase {
  knGridAddConstReal(Grid<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, T val) const
  {
    me[idx] += val;
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridAddConstReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, val);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  T val;
};
template<class T> struct knGridMultConst : public KernelBase {
  knGridMultConst(Grid<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, T val) const
  {
    me[idx] *= val;
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridMultConst ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, val);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  T val;
};

template<class T> struct knGridSafeDiv : public KernelBase {
  knGridSafeDiv(Grid<T> &me, const Grid<T> &other) : KernelBase(&me, 0), me(me), other(other)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, const Grid<T> &other) const
  {
    me[idx] = safeDivide(me[idx], other[idx]);
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline const Grid<T> &getArg1()
  {
    return other;
  }
  typedef Grid<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridSafeDiv ", 3);
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
  Grid<T> &me;
  const Grid<T> &other;
};
// KERNEL(idx) template<class T> void gridSafeDiv (Grid<T>& me, const Grid<T>& other) { me[idx] =
// safeDivide(me[idx], other[idx]); }

template<class T> struct knGridClamp : public KernelBase {
  knGridClamp(Grid<T> &me, const T &min, const T &max)
      : KernelBase(&me, 0), me(me), min(min), max(max)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, const T &min, const T &max) const
  {
    me[idx] = clamp(me[idx], min, max);
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline const T &getArg1()
  {
    return min;
  }
  typedef T type1;
  inline const T &getArg2()
  {
    return max;
  }
  typedef T type2;
  void runMessage()
  {
    debMsg("Executing kernel knGridClamp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, min, max);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  const T &min;
  const T &max;
};

template<typename T> inline void stomp(T &v, const T &th)
{
  if (v < th)
    v = 0;
}
template<> inline void stomp<Vec3>(Vec3 &v, const Vec3 &th)
{
  if (v[0] < th[0])
    v[0] = 0;
  if (v[1] < th[1])
    v[1] = 0;
  if (v[2] < th[2])
    v[2] = 0;
}
template<class T> struct knGridStomp : public KernelBase {
  knGridStomp(Grid<T> &me, const T &threshold) : KernelBase(&me, 0), me(me), threshold(threshold)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<T> &me, const T &threshold) const
  {
    stomp(me[idx], threshold);
  }
  inline Grid<T> &getArg0()
  {
    return me;
  }
  typedef Grid<T> type0;
  inline const T &getArg1()
  {
    return threshold;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridStomp ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, me, threshold);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<T> &me;
  const T &threshold;
};

template<class T> struct knPermuteAxes : public KernelBase {
  knPermuteAxes(Grid<T> &self, Grid<T> &target, int axis0, int axis1, int axis2)
      : KernelBase(&self, 0), self(self), target(target), axis0(axis0), axis1(axis1), axis2(axis2)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, Grid<T> &self, Grid<T> &target, int axis0, int axis1, int axis2) const
  {
    int i0 = axis0 == 0 ? i : (axis0 == 1 ? j : k);
    int i1 = axis1 == 0 ? i : (axis1 == 1 ? j : k);
    int i2 = axis2 == 0 ? i : (axis2 == 1 ? j : k);
    target(i0, i1, i2) = self(i, j, k);
  }
  inline Grid<T> &getArg0()
  {
    return self;
  }
  typedef Grid<T> type0;
  inline Grid<T> &getArg1()
  {
    return target;
  }
  typedef Grid<T> type1;
  inline int &getArg2()
  {
    return axis0;
  }
  typedef int type2;
  inline int &getArg3()
  {
    return axis1;
  }
  typedef int type3;
  inline int &getArg4()
  {
    return axis2;
  }
  typedef int type4;
  void runMessage()
  {
    debMsg("Executing kernel knPermuteAxes ", 3);
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
            op(i, j, k, self, target, axis0, axis1, axis2);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, self, target, axis0, axis1, axis2);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> &self;
  Grid<T> &target;
  int axis0;
  int axis1;
  int axis2;
};

struct knJoinVec : public KernelBase {
  knJoinVec(Grid<Vec3> &a, const Grid<Vec3> &b, bool keepMax)
      : KernelBase(&a, 0), a(a), b(b), keepMax(keepMax)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Vec3> &a, const Grid<Vec3> &b, bool keepMax) const
  {
    Real a1 = normSquare(a[idx]);
    Real b1 = normSquare(b[idx]);
    a[idx] = (keepMax) ? max(a1, b1) : min(a1, b1);
  }
  inline Grid<Vec3> &getArg0()
  {
    return a;
  }
  typedef Grid<Vec3> type0;
  inline const Grid<Vec3> &getArg1()
  {
    return b;
  }
  typedef Grid<Vec3> type1;
  inline bool &getArg2()
  {
    return keepMax;
  }
  typedef bool type2;
  void runMessage()
  {
    debMsg("Executing kernel knJoinVec ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, a, b, keepMax);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Vec3> &a;
  const Grid<Vec3> &b;
  bool keepMax;
};
struct knJoinInt : public KernelBase {
  knJoinInt(Grid<int> &a, const Grid<int> &b, bool keepMax)
      : KernelBase(&a, 0), a(a), b(b), keepMax(keepMax)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<int> &a, const Grid<int> &b, bool keepMax) const
  {
    a[idx] = (keepMax) ? max(a[idx], b[idx]) : min(a[idx], b[idx]);
  }
  inline Grid<int> &getArg0()
  {
    return a;
  }
  typedef Grid<int> type0;
  inline const Grid<int> &getArg1()
  {
    return b;
  }
  typedef Grid<int> type1;
  inline bool &getArg2()
  {
    return keepMax;
  }
  typedef bool type2;
  void runMessage()
  {
    debMsg("Executing kernel knJoinInt ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, a, b, keepMax);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<int> &a;
  const Grid<int> &b;
  bool keepMax;
};
struct knJoinReal : public KernelBase {
  knJoinReal(Grid<Real> &a, const Grid<Real> &b, bool keepMax)
      : KernelBase(&a, 0), a(a), b(b), keepMax(keepMax)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Real> &a, const Grid<Real> &b, bool keepMax) const
  {
    a[idx] = (keepMax) ? max(a[idx], b[idx]) : min(a[idx], b[idx]);
  }
  inline Grid<Real> &getArg0()
  {
    return a;
  }
  typedef Grid<Real> type0;
  inline const Grid<Real> &getArg1()
  {
    return b;
  }
  typedef Grid<Real> type1;
  inline bool &getArg2()
  {
    return keepMax;
  }
  typedef bool type2;
  void runMessage()
  {
    debMsg("Executing kernel knJoinReal ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, a, b, keepMax);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &a;
  const Grid<Real> &b;
  bool keepMax;
};

template<class T> Grid<T> &Grid<T>::safeDivide(const Grid<T> &a)
{
  knGridSafeDiv<T>(*this, a);
  return *this;
}

template<class T> int Grid<T>::getGridType()
{
  return static_cast<int>(mType);
}

template<class T> void Grid<T>::add(const Grid<T> &a)
{
  gridAdd<T, T>(*this, a);
}
template<class T> void Grid<T>::sub(const Grid<T> &a)
{
  gridSub<T, T>(*this, a);
}
template<class T> void Grid<T>::addScaled(const Grid<T> &a, const T &factor)
{
  gridScaledAdd<T, T>(*this, a, factor);
}
template<class T> void Grid<T>::setConst(T a)
{
  knGridSetConstReal<T>(*this, T(a));
}
template<class T> void Grid<T>::addConst(T a)
{
  knGridAddConstReal<T>(*this, T(a));
}
template<class T> void Grid<T>::multConst(T a)
{
  knGridMultConst<T>(*this, a);
}

template<class T> void Grid<T>::mult(const Grid<T> &a)
{
  gridMult<T, T>(*this, a);
}

template<class T> void Grid<T>::clamp(Real min, Real max)
{
  knGridClamp<T>(*this, T(min), T(max));
}
template<class T> void Grid<T>::stomp(const T &threshold)
{
  knGridStomp<T>(*this, threshold);
}
template<class T> void Grid<T>::permuteAxes(int axis0, int axis1, int axis2)
{
  if (axis0 == axis1 || axis0 == axis2 || axis1 == axis2 || axis0 > 2 || axis1 > 2 || axis2 > 2 ||
      axis0 < 0 || axis1 < 0 || axis2 < 0)
    return;
  Vec3i size = mParent->getGridSize();
  assertMsg(mParent->is2D() ? size.x == size.y : size.x == size.y && size.y == size.z,
            "Grid must be cubic!");
  Grid<T> tmp(mParent);
  knPermuteAxes<T>(*this, tmp, axis0, axis1, axis2);
  this->swap(tmp);
}
template<class T>
void Grid<T>::permuteAxesCopyToGrid(int axis0, int axis1, int axis2, Grid<T> &out)
{
  if (axis0 == axis1 || axis0 == axis2 || axis1 == axis2 || axis0 > 2 || axis1 > 2 || axis2 > 2 ||
      axis0 < 0 || axis1 < 0 || axis2 < 0)
    return;
  assertMsg(this->getGridType() == out.getGridType(), "Grids must have same data type!");
  Vec3i size = mParent->getGridSize();
  Vec3i sizeTarget = out.getParent()->getGridSize();
  assertMsg(sizeTarget[axis0] == size[0] && sizeTarget[axis1] == size[1] &&
                sizeTarget[axis2] == size[2],
            "Permuted grids must have the same dimensions!");
  knPermuteAxes<T>(*this, out, axis0, axis1, axis2);
}
template<> void Grid<Vec3>::join(const Grid<Vec3> &a, bool keepMax)
{
  knJoinVec(*this, a, keepMax);
}
template<> void Grid<int>::join(const Grid<int> &a, bool keepMax)
{
  knJoinInt(*this, a, keepMax);
}
template<> void Grid<Real>::join(const Grid<Real> &a, bool keepMax)
{
  knJoinReal(*this, a, keepMax);
}

template<> Real Grid<Real>::getMax() const
{
  return CompMaxReal(*this);
}
template<> Real Grid<Real>::getMin() const
{
  return CompMinReal(*this);
}
template<> Real Grid<Real>::getMaxAbs() const
{
  Real amin = CompMinReal(*this);
  Real amax = CompMaxReal(*this);
  return max(fabs(amin), fabs(amax));
}
template<> Real Grid<Vec3>::getMax() const
{
  return sqrt(CompMaxVec(*this));
}
template<> Real Grid<Vec3>::getMin() const
{
  return sqrt(CompMinVec(*this));
}
template<> Real Grid<Vec3>::getMaxAbs() const
{
  return sqrt(CompMaxVec(*this));
}
template<> Real Grid<int>::getMax() const
{
  return (Real)CompMaxInt(*this);
}
template<> Real Grid<int>::getMin() const
{
  return (Real)CompMinInt(*this);
}
template<> Real Grid<int>::getMaxAbs() const
{
  int amin = CompMinInt(*this);
  int amax = CompMaxInt(*this);
  return max(fabs((Real)amin), fabs((Real)amax));
}
template<class T> std::string Grid<T>::getDataPointer()
{
  std::ostringstream out;
  out << mData;
  return out.str();
}

// L1 / L2 functions

//! calculate L1 norm for whole grid with non-parallelized loop
template<class GRID> Real loop_calcL1Grid(const GRID &grid, int bnd)
{
  double accu = 0.;
  FOR_IJKT_BND(grid, bnd)
  {
    accu += norm(grid(i, j, k, t));
  }
  return (Real)accu;
}

//! calculate L2 norm for whole grid with non-parallelized loop
// note - kernels "could" be used here, but they can't be templated at the moment (also, that would
// mean the bnd parameter is fixed)
template<class GRID> Real loop_calcL2Grid(const GRID &grid, int bnd)
{
  double accu = 0.;
  FOR_IJKT_BND(grid, bnd)
  {
    accu += normSquare(grid(i, j, k, t));  // supported for real and vec3,4 types
  }
  return (Real)sqrt(accu);
}

//! compute L1 norm of whole grid content (note, not parallel at the moment)
template<class T> Real Grid<T>::getL1(int bnd)
{
  return loop_calcL1Grid<Grid<T>>(*this, bnd);
}
//! compute L2 norm of whole grid content (note, not parallel at the moment)
template<class T> Real Grid<T>::getL2(int bnd)
{
  return loop_calcL2Grid<Grid<T>>(*this, bnd);
}

struct knCountCells : public KernelBase {
  knCountCells(const FlagGrid &flags, int flag, int bnd, Grid<Real> *mask)
      : KernelBase(&flags, 0), flags(flags), flag(flag), bnd(bnd), mask(mask), cnt(0)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const FlagGrid &flags, int flag, int bnd, Grid<Real> *mask, int &cnt)
  {
    if (mask)
      (*mask)(i, j, k) = 0.;
    if (bnd > 0 && (!flags.isInBounds(Vec3i(i, j, k), bnd)))
      return;
    if (flags(i, j, k) & flag) {
      cnt++;
      if (mask)
        (*mask)(i, j, k) = 1.;
    }
  }
  inline operator int()
  {
    return cnt;
  }
  inline int &getRet()
  {
    return cnt;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline int &getArg1()
  {
    return flag;
  }
  typedef int type1;
  inline int &getArg2()
  {
    return bnd;
  }
  typedef int type2;
  inline Grid<Real> *getArg3()
  {
    return mask;
  }
  typedef Grid<Real> type3;
  void runMessage()
  {
    debMsg("Executing kernel knCountCells ", 3);
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
        for (int j = 0; j < _maxY; j++)
          for (int i = 0; i < _maxX; i++)
            op(i, j, k, flags, flag, bnd, mask, cnt);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, flag, bnd, mask, cnt);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  knCountCells(knCountCells &o, tbb::split)
      : KernelBase(o), flags(o.flags), flag(o.flag), bnd(o.bnd), mask(o.mask), cnt(0)
  {
  }
  void join(const knCountCells &o)
  {
    cnt += o.cnt;
  }
  const FlagGrid &flags;
  int flag;
  int bnd;
  Grid<Real> *mask;
  int cnt;
};

//! count number of cells of a certain type flag (can contain multiple bits, checks if any one of
//! them is set - not all!)
int FlagGrid::countCells(int flag, int bnd, Grid<Real> *mask)
{
  return knCountCells(*this, flag, bnd, mask);
}

// compute maximal diference of two cells in the grid
// used for testing system

Real gridMaxDiff(Grid<Real> &g1, Grid<Real> &g2)
{
  double maxVal = 0.;
  FOR_IJK(g1)
  {
    maxVal = std::max(maxVal, (double)fabs(g1(i, j, k) - g2(i, j, k)));
  }
  return maxVal;
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "gridMaxDiff", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &g1 = *_args.getPtr<Grid<Real>>("g1", 0, &_lock);
      Grid<Real> &g2 = *_args.getPtr<Grid<Real>>("g2", 1, &_lock);
      _retval = toPy(gridMaxDiff(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "gridMaxDiff", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("gridMaxDiff", e.what());
    return 0;
  }
}
static const Pb::Register _RP_gridMaxDiff("", "gridMaxDiff", _W_0);
extern "C" {
void PbRegister_gridMaxDiff()
{
  KEEP_UNUSED(_RP_gridMaxDiff);
}
}

Real gridMaxDiffInt(Grid<int> &g1, Grid<int> &g2)
{
  double maxVal = 0.;
  FOR_IJK(g1)
  {
    maxVal = std::max(maxVal, (double)fabs((double)g1(i, j, k) - g2(i, j, k)));
  }
  return maxVal;
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "gridMaxDiffInt", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<int> &g1 = *_args.getPtr<Grid<int>>("g1", 0, &_lock);
      Grid<int> &g2 = *_args.getPtr<Grid<int>>("g2", 1, &_lock);
      _retval = toPy(gridMaxDiffInt(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "gridMaxDiffInt", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("gridMaxDiffInt", e.what());
    return 0;
  }
}
static const Pb::Register _RP_gridMaxDiffInt("", "gridMaxDiffInt", _W_1);
extern "C" {
void PbRegister_gridMaxDiffInt()
{
  KEEP_UNUSED(_RP_gridMaxDiffInt);
}
}

Real gridMaxDiffVec3(Grid<Vec3> &g1, Grid<Vec3> &g2)
{
  double maxVal = 0.;
  FOR_IJK(g1)
  {
    // accumulate differences with double precision
    // note - don't use norm here! should be as precise as possible...
    double d = 0.;
    for (int c = 0; c < 3; ++c) {
      d += fabs((double)g1(i, j, k)[c] - (double)g2(i, j, k)[c]);
    }
    maxVal = std::max(maxVal, d);
    // maxVal = std::max(maxVal, (double)fabs( norm(g1(i,j,k)-g2(i,j,k)) ));
  }
  return maxVal;
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "gridMaxDiffVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &g1 = *_args.getPtr<Grid<Vec3>>("g1", 0, &_lock);
      Grid<Vec3> &g2 = *_args.getPtr<Grid<Vec3>>("g2", 1, &_lock);
      _retval = toPy(gridMaxDiffVec3(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "gridMaxDiffVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("gridMaxDiffVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_gridMaxDiffVec3("", "gridMaxDiffVec3", _W_2);
extern "C" {
void PbRegister_gridMaxDiffVec3()
{
  KEEP_UNUSED(_RP_gridMaxDiffVec3);
}
}

struct knCopyMacToVec3 : public KernelBase {
  knCopyMacToVec3(MACGrid &source, Grid<Vec3> &target)
      : KernelBase(&source, 0), source(source), target(target)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &source, Grid<Vec3> &target) const
  {
    target(i, j, k) = source(i, j, k);
  }
  inline MACGrid &getArg0()
  {
    return source;
  }
  typedef MACGrid type0;
  inline Grid<Vec3> &getArg1()
  {
    return target;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel knCopyMacToVec3 ", 3);
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
            op(i, j, k, source, target);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, source, target);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &source;
  Grid<Vec3> &target;
};
// simple helper functions to copy (convert) mac to vec3 , and levelset to real grids
// (are assumed to be the same for running the test cases - in general they're not!)

void copyMacToVec3(MACGrid &source, Grid<Vec3> &target)
{
  knCopyMacToVec3(source, target);
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "copyMacToVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &source = *_args.getPtr<MACGrid>("source", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      _retval = getPyNone();
      copyMacToVec3(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "copyMacToVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("copyMacToVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_copyMacToVec3("", "copyMacToVec3", _W_3);
extern "C" {
void PbRegister_copyMacToVec3()
{
  KEEP_UNUSED(_RP_copyMacToVec3);
}
}

void convertMacToVec3(MACGrid &source, Grid<Vec3> &target)
{
  debMsg("Deprecated - do not use convertMacToVec3... use copyMacToVec3 instead", 1);
  copyMacToVec3(source, target);
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "convertMacToVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &source = *_args.getPtr<MACGrid>("source", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      _retval = getPyNone();
      convertMacToVec3(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "convertMacToVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("convertMacToVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_convertMacToVec3("", "convertMacToVec3", _W_4);
extern "C" {
void PbRegister_convertMacToVec3()
{
  KEEP_UNUSED(_RP_convertMacToVec3);
}
}

struct knResampleVec3ToMac : public KernelBase {
  knResampleVec3ToMac(Grid<Vec3> &source, MACGrid &target)
      : KernelBase(&source, 1), source(source), target(target)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &source, MACGrid &target) const
  {
    target(i, j, k)[0] = 0.5 * (source(i - 1, j, k)[0] + source(i, j, k))[0];
    target(i, j, k)[1] = 0.5 * (source(i, j - 1, k)[1] + source(i, j, k))[1];
    if (target.is3D()) {
      target(i, j, k)[2] = 0.5 * (source(i, j, k - 1)[2] + source(i, j, k))[2];
    }
  }
  inline Grid<Vec3> &getArg0()
  {
    return source;
  }
  typedef Grid<Vec3> type0;
  inline MACGrid &getArg1()
  {
    return target;
  }
  typedef MACGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel knResampleVec3ToMac ", 3);
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
            op(i, j, k, source, target);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, source, target);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Vec3> &source;
  MACGrid &target;
};
//! vec3->mac grid conversion , but with full resampling

void resampleVec3ToMac(Grid<Vec3> &source, MACGrid &target)
{
  knResampleVec3ToMac(source, target);
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "resampleVec3ToMac", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &source = *_args.getPtr<Grid<Vec3>>("source", 0, &_lock);
      MACGrid &target = *_args.getPtr<MACGrid>("target", 1, &_lock);
      _retval = getPyNone();
      resampleVec3ToMac(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "resampleVec3ToMac", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("resampleVec3ToMac", e.what());
    return 0;
  }
}
static const Pb::Register _RP_resampleVec3ToMac("", "resampleVec3ToMac", _W_5);
extern "C" {
void PbRegister_resampleVec3ToMac()
{
  KEEP_UNUSED(_RP_resampleVec3ToMac);
}
}

struct knResampleMacToVec3 : public KernelBase {
  knResampleMacToVec3(MACGrid &source, Grid<Vec3> &target)
      : KernelBase(&source, 1), source(source), target(target)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &source, Grid<Vec3> &target) const
  {
    target(i, j, k) = source.getCentered(i, j, k);
  }
  inline MACGrid &getArg0()
  {
    return source;
  }
  typedef MACGrid type0;
  inline Grid<Vec3> &getArg1()
  {
    return target;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel knResampleMacToVec3 ", 3);
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
            op(i, j, k, source, target);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, source, target);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  MACGrid &source;
  Grid<Vec3> &target;
};
//! mac->vec3 grid conversion , with full resampling

void resampleMacToVec3(MACGrid &source, Grid<Vec3> &target)
{
  knResampleMacToVec3(source, target);
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "resampleMacToVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &source = *_args.getPtr<MACGrid>("source", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      _retval = getPyNone();
      resampleMacToVec3(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "resampleMacToVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("resampleMacToVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_resampleMacToVec3("", "resampleMacToVec3", _W_6);
extern "C" {
void PbRegister_resampleMacToVec3()
{
  KEEP_UNUSED(_RP_resampleMacToVec3);
}
}

struct knCopyLevelsetToReal : public KernelBase {
  knCopyLevelsetToReal(LevelsetGrid &source, Grid<Real> &target)
      : KernelBase(&source, 0), source(source), target(target)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, LevelsetGrid &source, Grid<Real> &target) const
  {
    target(i, j, k) = source(i, j, k);
  }
  inline LevelsetGrid &getArg0()
  {
    return source;
  }
  typedef LevelsetGrid type0;
  inline Grid<Real> &getArg1()
  {
    return target;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel knCopyLevelsetToReal ", 3);
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
            op(i, j, k, source, target);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, source, target);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  LevelsetGrid &source;
  Grid<Real> &target;
};

void copyLevelsetToReal(LevelsetGrid &source, Grid<Real> &target)
{
  knCopyLevelsetToReal(source, target);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "copyLevelsetToReal", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      LevelsetGrid &source = *_args.getPtr<LevelsetGrid>("source", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      _retval = getPyNone();
      copyLevelsetToReal(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "copyLevelsetToReal", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("copyLevelsetToReal", e.what());
    return 0;
  }
}
static const Pb::Register _RP_copyLevelsetToReal("", "copyLevelsetToReal", _W_7);
extern "C" {
void PbRegister_copyLevelsetToReal()
{
  KEEP_UNUSED(_RP_copyLevelsetToReal);
}
}

struct knCopyVec3ToReal : public KernelBase {
  knCopyVec3ToReal(Grid<Vec3> &source,
                   Grid<Real> &targetX,
                   Grid<Real> &targetY,
                   Grid<Real> &targetZ)
      : KernelBase(&source, 0),
        source(source),
        targetX(targetX),
        targetY(targetY),
        targetZ(targetZ)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Vec3> &source,
                 Grid<Real> &targetX,
                 Grid<Real> &targetY,
                 Grid<Real> &targetZ) const
  {
    targetX(i, j, k) = source(i, j, k).x;
    targetY(i, j, k) = source(i, j, k).y;
    targetZ(i, j, k) = source(i, j, k).z;
  }
  inline Grid<Vec3> &getArg0()
  {
    return source;
  }
  typedef Grid<Vec3> type0;
  inline Grid<Real> &getArg1()
  {
    return targetX;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> &getArg2()
  {
    return targetY;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> &getArg3()
  {
    return targetZ;
  }
  typedef Grid<Real> type3;
  void runMessage()
  {
    debMsg("Executing kernel knCopyVec3ToReal ", 3);
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
            op(i, j, k, source, targetX, targetY, targetZ);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, source, targetX, targetY, targetZ);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Vec3> &source;
  Grid<Real> &targetX;
  Grid<Real> &targetY;
  Grid<Real> &targetZ;
};

void copyVec3ToReal(Grid<Vec3> &source,
                    Grid<Real> &targetX,
                    Grid<Real> &targetY,
                    Grid<Real> &targetZ)
{
  knCopyVec3ToReal(source, targetX, targetY, targetZ);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "copyVec3ToReal", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &source = *_args.getPtr<Grid<Vec3>>("source", 0, &_lock);
      Grid<Real> &targetX = *_args.getPtr<Grid<Real>>("targetX", 1, &_lock);
      Grid<Real> &targetY = *_args.getPtr<Grid<Real>>("targetY", 2, &_lock);
      Grid<Real> &targetZ = *_args.getPtr<Grid<Real>>("targetZ", 3, &_lock);
      _retval = getPyNone();
      copyVec3ToReal(source, targetX, targetY, targetZ);
      _args.check();
    }
    pbFinalizePlugin(parent, "copyVec3ToReal", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("copyVec3ToReal", e.what());
    return 0;
  }
}
static const Pb::Register _RP_copyVec3ToReal("", "copyVec3ToReal", _W_8);
extern "C" {
void PbRegister_copyVec3ToReal()
{
  KEEP_UNUSED(_RP_copyVec3ToReal);
}
}

struct knCopyRealToVec3 : public KernelBase {
  knCopyRealToVec3(Grid<Real> &sourceX,
                   Grid<Real> &sourceY,
                   Grid<Real> &sourceZ,
                   Grid<Vec3> &target)
      : KernelBase(&sourceX, 0),
        sourceX(sourceX),
        sourceY(sourceY),
        sourceZ(sourceZ),
        target(target)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &sourceX,
                 Grid<Real> &sourceY,
                 Grid<Real> &sourceZ,
                 Grid<Vec3> &target) const
  {
    target(i, j, k).x = sourceX(i, j, k);
    target(i, j, k).y = sourceY(i, j, k);
    target(i, j, k).z = sourceZ(i, j, k);
  }
  inline Grid<Real> &getArg0()
  {
    return sourceX;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return sourceY;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> &getArg2()
  {
    return sourceZ;
  }
  typedef Grid<Real> type2;
  inline Grid<Vec3> &getArg3()
  {
    return target;
  }
  typedef Grid<Vec3> type3;
  void runMessage()
  {
    debMsg("Executing kernel knCopyRealToVec3 ", 3);
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
            op(i, j, k, sourceX, sourceY, sourceZ, target);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, sourceX, sourceY, sourceZ, target);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &sourceX;
  Grid<Real> &sourceY;
  Grid<Real> &sourceZ;
  Grid<Vec3> &target;
};

void copyRealToVec3(Grid<Real> &sourceX,
                    Grid<Real> &sourceY,
                    Grid<Real> &sourceZ,
                    Grid<Vec3> &target)
{
  knCopyRealToVec3(sourceX, sourceY, sourceZ, target);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "copyRealToVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &sourceX = *_args.getPtr<Grid<Real>>("sourceX", 0, &_lock);
      Grid<Real> &sourceY = *_args.getPtr<Grid<Real>>("sourceY", 1, &_lock);
      Grid<Real> &sourceZ = *_args.getPtr<Grid<Real>>("sourceZ", 2, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 3, &_lock);
      _retval = getPyNone();
      copyRealToVec3(sourceX, sourceY, sourceZ, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "copyRealToVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("copyRealToVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_copyRealToVec3("", "copyRealToVec3", _W_9);
extern "C" {
void PbRegister_copyRealToVec3()
{
  KEEP_UNUSED(_RP_copyRealToVec3);
}
}

void convertLevelsetToReal(LevelsetGrid &source, Grid<Real> &target)
{
  debMsg("Deprecated - do not use convertLevelsetToReal... use copyLevelsetToReal instead", 1);
  copyLevelsetToReal(source, target);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "convertLevelsetToReal", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      LevelsetGrid &source = *_args.getPtr<LevelsetGrid>("source", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      _retval = getPyNone();
      convertLevelsetToReal(source, target);
      _args.check();
    }
    pbFinalizePlugin(parent, "convertLevelsetToReal", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("convertLevelsetToReal", e.what());
    return 0;
  }
}
static const Pb::Register _RP_convertLevelsetToReal("", "convertLevelsetToReal", _W_10);
extern "C" {
void PbRegister_convertLevelsetToReal()
{
  KEEP_UNUSED(_RP_convertLevelsetToReal);
}
}

template<class T> void Grid<T>::printGrid(int zSlice, bool printIndex, int bnd)
{
  std::ostringstream out;
  out << std::endl;
  FOR_IJK_BND(*this, bnd)
  {
    IndexInt idx = (*this).index(i, j, k);
    if ((zSlice >= 0 && k == zSlice) || (zSlice < 0)) {
      out << " ";
      if (printIndex && this->is3D())
        out << "  " << i << "," << j << "," << k << ":";
      if (printIndex && !this->is3D())
        out << "  " << i << "," << j << ":";
      out << (*this)[idx];
      if (i == (*this).getSizeX() - 1 - bnd)
        out << std::endl;
    }
  }
  out << endl;
  debMsg("Printing " << this->getName() << out.str().c_str(), 1);
}

//! helper to swap components of a grid (eg for data import)
void swapComponents(Grid<Vec3> &vel, int c1 = 0, int c2 = 1, int c3 = 2)
{
  FOR_IJK(vel)
  {
    Vec3 v = vel(i, j, k);
    vel(i, j, k)[0] = v[c1];
    vel(i, j, k)[1] = v[c2];
    vel(i, j, k)[2] = v[c3];
  }
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "swapComponents", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &vel = *_args.getPtr<Grid<Vec3>>("vel", 0, &_lock);
      int c1 = _args.getOpt<int>("c1", 1, 0, &_lock);
      int c2 = _args.getOpt<int>("c2", 2, 1, &_lock);
      int c3 = _args.getOpt<int>("c3", 3, 2, &_lock);
      _retval = getPyNone();
      swapComponents(vel, c1, c2, c3);
      _args.check();
    }
    pbFinalizePlugin(parent, "swapComponents", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("swapComponents", e.what());
    return 0;
  }
}
static const Pb::Register _RP_swapComponents("", "swapComponents", _W_11);
extern "C" {
void PbRegister_swapComponents()
{
  KEEP_UNUSED(_RP_swapComponents);
}
}

// helper functions for UV grid data (stored grid coordinates as Vec3 values, and uv weight in
// entry zero)

// make uv weight accesible in python
Real getUvWeight(Grid<Vec3> &uv)
{
  return uv[0][0];
}
static PyObject *_W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getUvWeight", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &uv = *_args.getPtr<Grid<Vec3>>("uv", 0, &_lock);
      _retval = toPy(getUvWeight(uv));
      _args.check();
    }
    pbFinalizePlugin(parent, "getUvWeight", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getUvWeight", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getUvWeight("", "getUvWeight", _W_12);
extern "C" {
void PbRegister_getUvWeight()
{
  KEEP_UNUSED(_RP_getUvWeight);
}
}

// note - right now the UV grids have 0 values at the border after advection... could be fixed with
// an extrapolation step...

// compute normalized modulo interval
static inline Real computeUvGridTime(Real t, Real resetTime)
{
  return fmod((t / resetTime), (Real)1.);
}
// create ramp function in 0..1 range with half frequency
static inline Real computeUvRamp(Real t)
{
  Real uvWeight = 2. * t;
  if (uvWeight > 1.)
    uvWeight = 2. - uvWeight;
  return uvWeight;
}

struct knResetUvGrid : public KernelBase {
  knResetUvGrid(Grid<Vec3> &target, const Vec3 *offset)
      : KernelBase(&target, 0), target(target), offset(offset)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &target, const Vec3 *offset) const
  {
    Vec3 coord = Vec3((Real)i, (Real)j, (Real)k);
    if (offset)
      coord += (*offset);
    target(i, j, k) = coord;
  }
  inline Grid<Vec3> &getArg0()
  {
    return target;
  }
  typedef Grid<Vec3> type0;
  inline const Vec3 *getArg1()
  {
    return offset;
  }
  typedef Vec3 type1;
  void runMessage()
  {
    debMsg("Executing kernel knResetUvGrid ", 3);
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
            op(i, j, k, target, offset);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, target, offset);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Vec3> &target;
  const Vec3 *offset;
};

void resetUvGrid(Grid<Vec3> &target, const Vec3 *offset = NULL)
{
  knResetUvGrid reset(target,
                      offset);  // note, llvm complains about anonymous declaration here... ?
}
static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "resetUvGrid", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 0, &_lock);
      const Vec3 *offset = _args.getPtrOpt<Vec3>("offset", 1, NULL, &_lock);
      _retval = getPyNone();
      resetUvGrid(target, offset);
      _args.check();
    }
    pbFinalizePlugin(parent, "resetUvGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("resetUvGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_resetUvGrid("", "resetUvGrid", _W_13);
extern "C" {
void PbRegister_resetUvGrid()
{
  KEEP_UNUSED(_RP_resetUvGrid);
}
}

void updateUvWeight(
    Real resetTime, int index, int numUvs, Grid<Vec3> &uv, const Vec3 *offset = NULL)
{
  const Real t = uv.getParent()->getTime();
  Real timeOff = resetTime / (Real)numUvs;

  Real lastt = computeUvGridTime(t + (Real)index * timeOff - uv.getParent()->getDt(), resetTime);
  Real currt = computeUvGridTime(t + (Real)index * timeOff, resetTime);
  Real uvWeight = computeUvRamp(currt);

  // normalize the uvw weights , note: this is a bit wasteful...
  Real uvWTotal = 0.;
  for (int i = 0; i < numUvs; ++i) {
    uvWTotal += computeUvRamp(computeUvGridTime(t + (Real)i * timeOff, resetTime));
  }
  if (uvWTotal <= VECTOR_EPSILON) {
    uvWeight = uvWTotal = 1.;
  }
  else
    uvWeight /= uvWTotal;

  // check for reset
  if (currt < lastt)
    knResetUvGrid reset(uv, offset);

  // write new weight value to grid
  uv[0] = Vec3(uvWeight, 0., 0.);

  // print info about uv weights?
  debMsg("Uv grid " << index << "/" << numUvs << " t=" << currt << " w=" << uvWeight
                    << ", reset:" << (int)(currt < lastt),
         2);
}
static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "updateUvWeight", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Real resetTime = _args.get<Real>("resetTime", 0, &_lock);
      int index = _args.get<int>("index", 1, &_lock);
      int numUvs = _args.get<int>("numUvs", 2, &_lock);
      Grid<Vec3> &uv = *_args.getPtr<Grid<Vec3>>("uv", 3, &_lock);
      const Vec3 *offset = _args.getPtrOpt<Vec3>("offset", 4, NULL, &_lock);
      _retval = getPyNone();
      updateUvWeight(resetTime, index, numUvs, uv, offset);
      _args.check();
    }
    pbFinalizePlugin(parent, "updateUvWeight", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("updateUvWeight", e.what());
    return 0;
  }
}
static const Pb::Register _RP_updateUvWeight("", "updateUvWeight", _W_14);
extern "C" {
void PbRegister_updateUvWeight()
{
  KEEP_UNUSED(_RP_updateUvWeight);
}
}

template<class T> struct knSetBoundary : public KernelBase {
  knSetBoundary(Grid<T> &grid, T value, int w)
      : KernelBase(&grid, 0), grid(grid), value(value), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<T> &grid, T value, int w) const
  {
    bool bnd = (i <= w || i >= grid.getSizeX() - 1 - w || j <= w || j >= grid.getSizeY() - 1 - w ||
                (grid.is3D() && (k <= w || k >= grid.getSizeZ() - 1 - w)));
    if (bnd)
      grid(i, j, k) = value;
  }
  inline Grid<T> &getArg0()
  {
    return grid;
  }
  typedef Grid<T> type0;
  inline T &getArg1()
  {
    return value;
  }
  typedef T type1;
  inline int &getArg2()
  {
    return w;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetBoundary ", 3);
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
            op(i, j, k, grid, value, w);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, value, w);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> &grid;
  T value;
  int w;
};

template<class T> void Grid<T>::setBound(T value, int boundaryWidth)
{
  knSetBoundary<T>(*this, value, boundaryWidth);
}

template<class T> struct knSetBoundaryNeumann : public KernelBase {
  knSetBoundaryNeumann(Grid<T> &grid, int w) : KernelBase(&grid, 0), grid(grid), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<T> &grid, int w) const
  {
    bool set = false;
    int si = i, sj = j, sk = k;
    if (i <= w) {
      si = w + 1;
      set = true;
    }
    if (i >= grid.getSizeX() - 1 - w) {
      si = grid.getSizeX() - 1 - w - 1;
      set = true;
    }
    if (j <= w) {
      sj = w + 1;
      set = true;
    }
    if (j >= grid.getSizeY() - 1 - w) {
      sj = grid.getSizeY() - 1 - w - 1;
      set = true;
    }
    if (grid.is3D()) {
      if (k <= w) {
        sk = w + 1;
        set = true;
      }
      if (k >= grid.getSizeZ() - 1 - w) {
        sk = grid.getSizeZ() - 1 - w - 1;
        set = true;
      }
    }
    if (set)
      grid(i, j, k) = grid(si, sj, sk);
  }
  inline Grid<T> &getArg0()
  {
    return grid;
  }
  typedef Grid<T> type0;
  inline int &getArg1()
  {
    return w;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel knSetBoundaryNeumann ", 3);
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
            op(i, j, k, grid, w);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, w);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> &grid;
  int w;
};

template<class T> void Grid<T>::setBoundNeumann(int boundaryWidth)
{
  knSetBoundaryNeumann<T>(*this, boundaryWidth);
}

//! kernel to set velocity components of mac grid to value for a boundary of w cells
struct knSetBoundaryMAC : public KernelBase {
  knSetBoundaryMAC(Grid<Vec3> &grid, Vec3 value, int w)
      : KernelBase(&grid, 0), grid(grid), value(value), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &grid, Vec3 value, int w) const
  {
    if (i <= w || i >= grid.getSizeX() - w || j <= w - 1 || j >= grid.getSizeY() - 1 - w ||
        (grid.is3D() && (k <= w - 1 || k >= grid.getSizeZ() - 1 - w)))
      grid(i, j, k).x = value.x;
    if (i <= w - 1 || i >= grid.getSizeX() - 1 - w || j <= w || j >= grid.getSizeY() - w ||
        (grid.is3D() && (k <= w - 1 || k >= grid.getSizeZ() - 1 - w)))
      grid(i, j, k).y = value.y;
    if (i <= w - 1 || i >= grid.getSizeX() - 1 - w || j <= w - 1 || j >= grid.getSizeY() - 1 - w ||
        (grid.is3D() && (k <= w || k >= grid.getSizeZ() - w)))
      grid(i, j, k).z = value.z;
  }
  inline Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Vec3 &getArg1()
  {
    return value;
  }
  typedef Vec3 type1;
  inline int &getArg2()
  {
    return w;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetBoundaryMAC ", 3);
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
            op(i, j, k, grid, value, w);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, value, w);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Vec3> &grid;
  Vec3 value;
  int w;
};

//! only set normal velocity components of mac grid to value for a boundary of w cells
struct knSetBoundaryMACNorm : public KernelBase {
  knSetBoundaryMACNorm(Grid<Vec3> &grid, Vec3 value, int w)
      : KernelBase(&grid, 0), grid(grid), value(value), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Vec3> &grid, Vec3 value, int w) const
  {
    if (i <= w || i >= grid.getSizeX() - w)
      grid(i, j, k).x = value.x;
    if (j <= w || j >= grid.getSizeY() - w)
      grid(i, j, k).y = value.y;
    if ((grid.is3D() && (k <= w || k >= grid.getSizeZ() - w)))
      grid(i, j, k).z = value.z;
  }
  inline Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Vec3 &getArg1()
  {
    return value;
  }
  typedef Vec3 type1;
  inline int &getArg2()
  {
    return w;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetBoundaryMACNorm ", 3);
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
            op(i, j, k, grid, value, w);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, value, w);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Vec3> &grid;
  Vec3 value;
  int w;
};

//! set velocity components of mac grid to value for a boundary of w cells (optionally only normal
//! values)
void MACGrid::setBoundMAC(Vec3 value, int boundaryWidth, bool normalOnly)
{
  if (!normalOnly)
    knSetBoundaryMAC(*this, value, boundaryWidth);
  else
    knSetBoundaryMACNorm(*this, value, boundaryWidth);
}

//! helper kernels for getGridAvg

struct knGridTotalSum : public KernelBase {
  knGridTotalSum(const Grid<Real> &a, FlagGrid *flags)
      : KernelBase(&a, 0), a(a), flags(flags), result(0.0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &a, FlagGrid *flags, double &result)
  {
    if (flags) {
      if (flags->isFluid(idx))
        result += a[idx];
    }
    else {
      result += a[idx];
    }
  }
  inline operator double()
  {
    return result;
  }
  inline double &getRet()
  {
    return result;
  }
  inline const Grid<Real> &getArg0()
  {
    return a;
  }
  typedef Grid<Real> type0;
  inline FlagGrid *getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel knGridTotalSum ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, a, flags, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  knGridTotalSum(knGridTotalSum &o, tbb::split)
      : KernelBase(o), a(o.a), flags(o.flags), result(0.0)
  {
  }
  void join(const knGridTotalSum &o)
  {
    result += o.result;
  }
  const Grid<Real> &a;
  FlagGrid *flags;
  double result;
};

struct knCountFluidCells : public KernelBase {
  knCountFluidCells(FlagGrid &flags) : KernelBase(&flags, 0), flags(flags), numEmpty(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, FlagGrid &flags, int &numEmpty)
  {
    if (flags.isFluid(idx))
      numEmpty++;
  }
  inline operator int()
  {
    return numEmpty;
  }
  inline int &getRet()
  {
    return numEmpty;
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  void runMessage()
  {
    debMsg("Executing kernel knCountFluidCells ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, numEmpty);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  knCountFluidCells(knCountFluidCells &o, tbb::split) : KernelBase(o), flags(o.flags), numEmpty(0)
  {
  }
  void join(const knCountFluidCells &o)
  {
    numEmpty += o.numEmpty;
  }
  FlagGrid &flags;
  int numEmpty;
};

//! averaged value for all cells (if flags are given, only for fluid cells)

Real getGridAvg(Grid<Real> &source, FlagGrid *flags = NULL)
{
  double sum = knGridTotalSum(source, flags);

  double cells;
  if (flags) {
    cells = knCountFluidCells(*flags);
  }
  else {
    cells = source.getSizeX() * source.getSizeY() * source.getSizeZ();
  }

  if (cells > 0.)
    sum *= 1. / cells;
  else
    sum = -1.;
  return sum;
}
static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getGridAvg", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &source = *_args.getPtr<Grid<Real>>("source", 0, &_lock);
      FlagGrid *flags = _args.getPtrOpt<FlagGrid>("flags", 1, NULL, &_lock);
      _retval = toPy(getGridAvg(source, flags));
      _args.check();
    }
    pbFinalizePlugin(parent, "getGridAvg", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getGridAvg", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getGridAvg("", "getGridAvg", _W_15);
extern "C" {
void PbRegister_getGridAvg()
{
  KEEP_UNUSED(_RP_getGridAvg);
}
}

//! transfer data between real and vec3 grids

struct knGetComponent : public KernelBase {
  knGetComponent(const Grid<Vec3> &source, Grid<Real> &target, int component)
      : KernelBase(&source, 0), source(source), target(target), component(component)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Vec3> &source, Grid<Real> &target, int component) const
  {
    target[idx] = source[idx][component];
  }
  inline const Grid<Vec3> &getArg0()
  {
    return source;
  }
  typedef Grid<Vec3> type0;
  inline Grid<Real> &getArg1()
  {
    return target;
  }
  typedef Grid<Real> type1;
  inline int &getArg2()
  {
    return component;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knGetComponent ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, source, target, component);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const Grid<Vec3> &source;
  Grid<Real> &target;
  int component;
};
void getComponent(const Grid<Vec3> &source, Grid<Real> &target, int component)
{
  knGetComponent(source, target, component);
}
static PyObject *_W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getComponent", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid<Vec3> &source = *_args.getPtr<Grid<Vec3>>("source", 0, &_lock);
      Grid<Real> &target = *_args.getPtr<Grid<Real>>("target", 1, &_lock);
      int component = _args.get<int>("component", 2, &_lock);
      _retval = getPyNone();
      getComponent(source, target, component);
      _args.check();
    }
    pbFinalizePlugin(parent, "getComponent", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getComponent", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getComponent("", "getComponent", _W_16);
extern "C" {
void PbRegister_getComponent()
{
  KEEP_UNUSED(_RP_getComponent);
}
}

struct knSetComponent : public KernelBase {
  knSetComponent(const Grid<Real> &source, Grid<Vec3> &target, int component)
      : KernelBase(&source, 0), source(source), target(target), component(component)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &source, Grid<Vec3> &target, int component) const
  {
    target[idx][component] = source[idx];
  }
  inline const Grid<Real> &getArg0()
  {
    return source;
  }
  typedef Grid<Real> type0;
  inline Grid<Vec3> &getArg1()
  {
    return target;
  }
  typedef Grid<Vec3> type1;
  inline int &getArg2()
  {
    return component;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetComponent ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, source, target, component);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const Grid<Real> &source;
  Grid<Vec3> &target;
  int component;
};
void setComponent(const Grid<Real> &source, Grid<Vec3> &target, int component)
{
  knSetComponent(source, target, component);
}
static PyObject *_W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setComponent", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid<Real> &source = *_args.getPtr<Grid<Real>>("source", 0, &_lock);
      Grid<Vec3> &target = *_args.getPtr<Grid<Vec3>>("target", 1, &_lock);
      int component = _args.get<int>("component", 2, &_lock);
      _retval = getPyNone();
      setComponent(source, target, component);
      _args.check();
    }
    pbFinalizePlugin(parent, "setComponent", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setComponent", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setComponent("", "setComponent", _W_17);
extern "C" {
void PbRegister_setComponent()
{
  KEEP_UNUSED(_RP_setComponent);
}
}

//******************************************************************************
// Specialization classes

void FlagGrid::InitMinXWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(i - w - .5, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::InitMaxXWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(mSize.x - i - 1.5 - w, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::InitMinYWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(j - w - .5, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::InitMaxYWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(mSize.y - j - 1.5 - w, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::InitMinZWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(k - w - .5, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::InitMaxZWall(const int &boundaryWidth, Grid<Real> &phiWalls)
{
  const int w = boundaryWidth;
  FOR_IJK(phiWalls)
  {
    phiWalls(i, j, k) = std::min(mSize.z - k - 1.5 - w, (double)phiWalls(i, j, k));
  }
}

void FlagGrid::initDomain(const int &boundaryWidth,
                          const string &wallIn,
                          const string &openIn,
                          const string &inflowIn,
                          const string &outflowIn,
                          Grid<Real> *phiWalls)
{

  int types[6] = {0};
  bool set[6] = {false};
  // make sure we have at least 6 entries
  string wall = wallIn;
  wall.append("      ");
  string open = openIn;
  open.append("      ");
  string inflow = inflowIn;
  inflow.append("      ");
  string outflow = outflowIn;
  outflow.append("      ");

  if (phiWalls)
    phiWalls->setConst(1000000000);

  for (int i = 0; i < 6; ++i) {
    // min x-direction
    if (!set[0]) {
      if (open[i] == 'x') {
        types[0] = TypeOpen;
        set[0] = true;
      }
      else if (inflow[i] == 'x') {
        types[0] = TypeInflow;
        set[0] = true;
      }
      else if (outflow[i] == 'x') {
        types[0] = TypeOutflow;
        set[0] = true;
      }
      else if (wall[i] == 'x') {
        types[0] = TypeObstacle;
        if (phiWalls)
          InitMinXWall(boundaryWidth, *phiWalls);
        set[0] = true;
      }
    }
    // max x-direction
    if (!set[1]) {
      if (open[i] == 'X') {
        types[1] = TypeOpen;
        set[1] = true;
      }
      else if (inflow[i] == 'X') {
        types[1] = TypeInflow;
        set[1] = true;
      }
      else if (outflow[i] == 'X') {
        types[1] = TypeOutflow;
        set[1] = true;
      }
      else if (wall[i] == 'X') {
        types[1] = TypeObstacle;
        if (phiWalls)
          InitMaxXWall(boundaryWidth, *phiWalls);
        set[1] = true;
      }
    }
    // min y-direction
    if (!set[2]) {
      if (open[i] == 'y') {
        types[2] = TypeOpen;
        set[2] = true;
      }
      else if (inflow[i] == 'y') {
        types[2] = TypeInflow;
        set[2] = true;
      }
      else if (outflow[i] == 'y') {
        types[2] = TypeOutflow;
        set[2] = true;
      }
      else if (wall[i] == 'y') {
        types[2] = TypeObstacle;
        if (phiWalls)
          InitMinYWall(boundaryWidth, *phiWalls);
        set[2] = true;
      }
    }
    // max y-direction
    if (!set[3]) {
      if (open[i] == 'Y') {
        types[3] = TypeOpen;
        set[3] = true;
      }
      else if (inflow[i] == 'Y') {
        types[3] = TypeInflow;
        set[3] = true;
      }
      else if (outflow[i] == 'Y') {
        types[3] = TypeOutflow;
        set[3] = true;
      }
      else if (wall[i] == 'Y') {
        types[3] = TypeObstacle;
        if (phiWalls)
          InitMaxYWall(boundaryWidth, *phiWalls);
        set[3] = true;
      }
    }
    if (this->is3D()) {
      // min z-direction
      if (!set[4]) {
        if (open[i] == 'z') {
          types[4] = TypeOpen;
          set[4] = true;
        }
        else if (inflow[i] == 'z') {
          types[4] = TypeInflow;
          set[4] = true;
        }
        else if (outflow[i] == 'z') {
          types[4] = TypeOutflow;
          set[4] = true;
        }
        else if (wall[i] == 'z') {
          types[4] = TypeObstacle;
          if (phiWalls)
            InitMinZWall(boundaryWidth, *phiWalls);
          set[4] = true;
        }
      }
      // max z-direction
      if (!set[5]) {
        if (open[i] == 'Z') {
          types[5] = TypeOpen;
          set[5] = true;
        }
        else if (inflow[i] == 'Z') {
          types[5] = TypeInflow;
          set[5] = true;
        }
        else if (outflow[i] == 'Z') {
          types[5] = TypeOutflow;
          set[5] = true;
        }
        else if (wall[i] == 'Z') {
          types[5] = TypeObstacle;
          if (phiWalls)
            InitMaxZWall(boundaryWidth, *phiWalls);
          set[5] = true;
        }
      }
    }
  }

  setConst(TypeEmpty);
  initBoundaries(boundaryWidth, types);
}

void FlagGrid::initBoundaries(const int &boundaryWidth, const int *types)
{
  const int w = boundaryWidth;
  FOR_IJK(*this)
  {
    bool bnd = (i <= w);
    if (bnd)
      mData[index(i, j, k)] = types[0];
    bnd = (i >= mSize.x - 1 - w);
    if (bnd)
      mData[index(i, j, k)] = types[1];
    bnd = (j <= w);
    if (bnd)
      mData[index(i, j, k)] = types[2];
    bnd = (j >= mSize.y - 1 - w);
    if (bnd)
      mData[index(i, j, k)] = types[3];
    if (is3D()) {
      bnd = (k <= w);
      if (bnd)
        mData[index(i, j, k)] = types[4];
      bnd = (k >= mSize.z - 1 - w);
      if (bnd)
        mData[index(i, j, k)] = types[5];
    }
  }
}

void FlagGrid::updateFromLevelset(LevelsetGrid &levelset)
{
  FOR_IDX(*this)
  {
    if (!isObstacle(idx) && !isOutflow(idx)) {
      const Real phi = levelset[idx];
      if (phi <= levelset.invalidTimeValue())
        continue;

      mData[idx] &= ~(TypeEmpty | TypeFluid);            // clear empty/fluid flags
      mData[idx] |= (phi <= 0) ? TypeFluid : TypeEmpty;  // set resepctive flag
    }
  }
}

void FlagGrid::fillGrid(int type)
{
  FOR_IDX(*this)
  {
    if ((mData[idx] & TypeObstacle) == 0 && (mData[idx] & TypeInflow) == 0 &&
        (mData[idx] & TypeOutflow) == 0 && (mData[idx] & TypeOpen) == 0)
      mData[idx] = (mData[idx] & ~(TypeEmpty | TypeFluid)) | type;
  }
}

// flag grid helper

bool isIsolatedFluidCell(const IndexInt idx, const FlagGrid &flags)
{
  if (!flags.isFluid(idx))
    return false;
  if (flags.isFluid(idx - flags.getStrideX()))
    return false;
  if (flags.isFluid(idx + flags.getStrideX()))
    return false;
  if (flags.isFluid(idx - flags.getStrideY()))
    return false;
  if (flags.isFluid(idx + flags.getStrideY()))
    return false;
  if (!flags.is3D())
    return true;
  if (flags.isFluid(idx - flags.getStrideZ()))
    return false;
  if (flags.isFluid(idx + flags.getStrideZ()))
    return false;
  return true;
}

struct knMarkIsolatedFluidCell : public KernelBase {
  knMarkIsolatedFluidCell(FlagGrid &flags, const int mark)
      : KernelBase(&flags, 0), flags(flags), mark(mark)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, FlagGrid &flags, const int mark) const
  {
    if (isIsolatedFluidCell(idx, flags))
      flags[idx] = mark;
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const int &getArg1()
  {
    return mark;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel knMarkIsolatedFluidCell ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, mark);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  FlagGrid &flags;
  const int mark;
};

void markIsolatedFluidCell(FlagGrid &flags, const int mark)
{
  knMarkIsolatedFluidCell(flags, mark);
}
static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "markIsolatedFluidCell", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const int mark = _args.get<int>("mark", 1, &_lock);
      _retval = getPyNone();
      markIsolatedFluidCell(flags, mark);
      _args.check();
    }
    pbFinalizePlugin(parent, "markIsolatedFluidCell", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("markIsolatedFluidCell", e.what());
    return 0;
  }
}
static const Pb::Register _RP_markIsolatedFluidCell("", "markIsolatedFluidCell", _W_18);
extern "C" {
void PbRegister_markIsolatedFluidCell()
{
  KEEP_UNUSED(_RP_markIsolatedFluidCell);
}
}

void copyMACData(
    const MACGrid &source, MACGrid &target, const FlagGrid &flags, const int flag, const int bnd)
{
  assertMsg(source.getSize().x == target.getSize().x && source.getSize().y == target.getSize().y &&
                source.getSize().z == target.getSize().z,
            "different grid resolutions " << source.getSize() << " vs " << target.getSize());

  // Grid<Real> divGrid(target.getParent());
  // DivergenceOpMAC(divGrid, target);
  // Real fDivOrig = GridSumSqr(divGrid);

  FOR_IJK_BND(target, bnd)
  {
    if (flags.get(i, j, k) & flag) {
      target(i, j, k) = source(i, j, k);
    }
  }

  // DivergenceOpMAC(divGrid, target);
  // Real fDivTransfer = GridSumSqr(divGrid);
  // std::cout << "Divergence: " << fDivOrig << " -> " << fDivTransfer << std::endl;
}
static PyObject *_W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "copyMACData", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const MACGrid &source = *_args.getPtr<MACGrid>("source", 0, &_lock);
      MACGrid &target = *_args.getPtr<MACGrid>("target", 1, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 2, &_lock);
      const int flag = _args.get<int>("flag", 3, &_lock);
      const int bnd = _args.get<int>("bnd", 4, &_lock);
      _retval = getPyNone();
      copyMACData(source, target, flags, flag, bnd);
      _args.check();
    }
    pbFinalizePlugin(parent, "copyMACData", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("copyMACData", e.what());
    return 0;
  }
}
static const Pb::Register _RP_copyMACData("", "copyMACData", _W_19);
extern "C" {
void PbRegister_copyMACData()
{
  KEEP_UNUSED(_RP_copyMACData);
}
}

// explicit instantiation
template class Grid<int>;
template class Grid<Real>;
template class Grid<Vec3>;

}  // namespace Manta
