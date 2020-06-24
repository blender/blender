

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

#include <limits>
#include <sstream>
#include <cstring>

#include "grid4d.h"
#include "levelset.h"
#include "kernel.h"
#include "mantaio.h"

using namespace std;
namespace Manta {

//******************************************************************************
// GridBase members

Grid4dBase::Grid4dBase(FluidSolver *parent) : PbClass(parent), mType(TypeNone)
{
  checkParent();
}

//******************************************************************************
// Grid4d<T> members

// helpers to set type
template<class T> inline Grid4dBase::Grid4dType typeList()
{
  return Grid4dBase::TypeNone;
}
template<> inline Grid4dBase::Grid4dType typeList<Real>()
{
  return Grid4dBase::TypeReal;
}
template<> inline Grid4dBase::Grid4dType typeList<int>()
{
  return Grid4dBase::TypeInt;
}
template<> inline Grid4dBase::Grid4dType typeList<Vec3>()
{
  return Grid4dBase::TypeVec3;
}
template<> inline Grid4dBase::Grid4dType typeList<Vec4>()
{
  return Grid4dBase::TypeVec4;
}

template<class T> Grid4d<T>::Grid4d(FluidSolver *parent, bool show) : Grid4dBase(parent)
{
  assertMsg(parent->is3D() && parent->supports4D(),
            "To use 4d grids create a 3d solver with fourthDim>0");

  mType = typeList<T>();
  Vec3i s = parent->getGridSize();
  mSize = Vec4i(s.x, s.y, s.z, parent->getFourthDim());
  mData = parent->getGrid4dPointer<T>();
  assertMsg(mData, "Couldnt allocate data pointer!");

  mStrideZ = (mSize.x * mSize.y);
  mStrideT = (mStrideZ * mSize.z);

  Real sizemax = (Real)mSize[0];
  for (int c = 1; c < 3; ++c)
    if (mSize[c] > sizemax)
      sizemax = mSize[c];
  // note - the 4d component is ignored for dx! keep same scaling as for 3d...
  mDx = 1.0 / sizemax;

  clear();
  setHidden(!show);
}

template<class T> Grid4d<T>::Grid4d(const Grid4d<T> &a) : Grid4dBase(a.getParent())
{
  mSize = a.mSize;
  mType = a.mType;
  mStrideZ = a.mStrideZ;
  mStrideT = a.mStrideT;
  mDx = a.mDx;
  FluidSolver *gp = a.getParent();
  mData = gp->getGrid4dPointer<T>();
  assertMsg(mData, "Couldnt allocate data pointer!");

  memcpy(mData, a.mData, sizeof(T) * a.mSize.x * a.mSize.y * a.mSize.z * a.mSize.t);
}

template<class T> Grid4d<T>::~Grid4d()
{
  mParent->freeGrid4dPointer<T>(mData);
}

template<class T> void Grid4d<T>::clear()
{
  memset(mData, 0, sizeof(T) * mSize.x * mSize.y * mSize.z * mSize.t);
}

template<class T> void Grid4d<T>::swap(Grid4d<T> &other)
{
  if (other.getSizeX() != getSizeX() || other.getSizeY() != getSizeY() ||
      other.getSizeZ() != getSizeZ() || other.getSizeT() != getSizeT())
    errMsg("Grid4d::swap(): Grid4d dimensions mismatch.");

  T *dswap = other.mData;
  other.mData = mData;
  mData = dswap;
}

template<class T> int Grid4d<T>::load(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".uni")
    return readGrid4dUni(name, this);
  else if (ext == ".raw")
    return readGrid4dRaw(name, this);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}

template<class T> int Grid4d<T>::save(string name)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));
  if (ext == ".uni")
    return writeGrid4dUni(name, this);
  else if (ext == ".raw")
    return writeGrid4dRaw(name, this);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}

//******************************************************************************
// Grid4d<T> operators

//! Kernel: Compute min value of Real Grid4d

struct kn4dMinReal : public KernelBase {
  kn4dMinReal(Grid4d<Real> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<Real> &val, Real &minVal)
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
  inline Grid4d<Real> &getArg0()
  {
    return val;
  }
  typedef Grid4d<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMinReal ", 3);
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
  kn4dMinReal(kn4dMinReal &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<Real>::max())
  {
  }
  void join(const kn4dMinReal &o)
  {
    minVal = min(minVal, o.minVal);
  }
  Grid4d<Real> &val;
  Real minVal;
};

//! Kernel: Compute max value of Real Grid4d

struct kn4dMaxReal : public KernelBase {
  kn4dMaxReal(Grid4d<Real> &val)
      : KernelBase(&val, 0), val(val), maxVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<Real> &val, Real &maxVal)
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
  inline Grid4d<Real> &getArg0()
  {
    return val;
  }
  typedef Grid4d<Real> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMaxReal ", 3);
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
  kn4dMaxReal(kn4dMaxReal &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const kn4dMaxReal &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  Grid4d<Real> &val;
  Real maxVal;
};

//! Kernel: Compute min value of int Grid4d

struct kn4dMinInt : public KernelBase {
  kn4dMinInt(Grid4d<int> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<int>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<int> &val, int &minVal)
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
  inline Grid4d<int> &getArg0()
  {
    return val;
  }
  typedef Grid4d<int> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMinInt ", 3);
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
  kn4dMinInt(kn4dMinInt &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<int>::max())
  {
  }
  void join(const kn4dMinInt &o)
  {
    minVal = min(minVal, o.minVal);
  }
  Grid4d<int> &val;
  int minVal;
};

//! Kernel: Compute max value of int Grid4d

struct kn4dMaxInt : public KernelBase {
  kn4dMaxInt(Grid4d<int> &val)
      : KernelBase(&val, 0), val(val), maxVal(std::numeric_limits<int>::min())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<int> &val, int &maxVal)
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
  inline Grid4d<int> &getArg0()
  {
    return val;
  }
  typedef Grid4d<int> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMaxInt ", 3);
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
  kn4dMaxInt(kn4dMaxInt &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(std::numeric_limits<int>::min())
  {
  }
  void join(const kn4dMaxInt &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  Grid4d<int> &val;
  int maxVal;
};

//! Kernel: Compute min norm of vec Grid4d

template<class VEC> struct kn4dMinVec : public KernelBase {
  kn4dMinVec(Grid4d<VEC> &val)
      : KernelBase(&val, 0), val(val), minVal(std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<VEC> &val, Real &minVal)
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
  inline Grid4d<VEC> &getArg0()
  {
    return val;
  }
  typedef Grid4d<VEC> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMinVec ", 3);
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
  kn4dMinVec(kn4dMinVec &o, tbb::split)
      : KernelBase(o), val(o.val), minVal(std::numeric_limits<Real>::max())
  {
  }
  void join(const kn4dMinVec &o)
  {
    minVal = min(minVal, o.minVal);
  }
  Grid4d<VEC> &val;
  Real minVal;
};

//! Kernel: Compute max norm of vec Grid4d

template<class VEC> struct kn4dMaxVec : public KernelBase {
  kn4dMaxVec(Grid4d<VEC> &val)
      : KernelBase(&val, 0), val(val), maxVal(-std::numeric_limits<Real>::max())
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<VEC> &val, Real &maxVal)
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
  inline Grid4d<VEC> &getArg0()
  {
    return val;
  }
  typedef Grid4d<VEC> type0;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMaxVec ", 3);
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
  kn4dMaxVec(kn4dMaxVec &o, tbb::split)
      : KernelBase(o), val(o.val), maxVal(-std::numeric_limits<Real>::max())
  {
  }
  void join(const kn4dMaxVec &o)
  {
    maxVal = max(maxVal, o.maxVal);
  }
  Grid4d<VEC> &val;
  Real maxVal;
};

template<class T> Grid4d<T> &Grid4d<T>::safeDivide(const Grid4d<T> &a)
{
  Grid4dSafeDiv<T>(*this, a);
  return *this;
}
template<class T> Grid4d<T> &Grid4d<T>::copyFrom(const Grid4d<T> &a, bool copyType)
{
  assertMsg(a.mSize.x == mSize.x && a.mSize.y == mSize.y && a.mSize.z == mSize.z &&
                a.mSize.t == mSize.t,
            "different Grid4d resolutions " << a.mSize << " vs " << this->mSize);
  memcpy(mData, a.mData, sizeof(T) * mSize.x * mSize.y * mSize.z * mSize.t);
  if (copyType)
    mType = a.mType;  // copy type marker
  return *this;
}
/*template<class T> Grid4d<T>& Grid4d<T>::operator= (const Grid4d<T>& a) {
  note: do not use , use copyFrom instead
}*/

template<class T> struct kn4dSetConstReal : public KernelBase {
  kn4dSetConstReal(Grid4d<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, T val) const
  {
    me[idx] = val;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel kn4dSetConstReal ", 3);
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
  Grid4d<T> &me;
  T val;
};
template<class T> struct kn4dAddConstReal : public KernelBase {
  kn4dAddConstReal(Grid4d<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, T val) const
  {
    me[idx] += val;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel kn4dAddConstReal ", 3);
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
  Grid4d<T> &me;
  T val;
};
template<class T> struct kn4dMultConst : public KernelBase {
  kn4dMultConst(Grid4d<T> &me, T val) : KernelBase(&me, 0), me(me), val(val)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, T val) const
  {
    me[idx] *= val;
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline T &getArg1()
  {
    return val;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel kn4dMultConst ", 3);
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
  Grid4d<T> &me;
  T val;
};
template<class T> struct kn4dClamp : public KernelBase {
  kn4dClamp(Grid4d<T> &me, T min, T max) : KernelBase(&me, 0), me(me), min(min), max(max)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid4d<T> &me, T min, T max) const
  {
    me[idx] = clamp(me[idx], min, max);
  }
  inline Grid4d<T> &getArg0()
  {
    return me;
  }
  typedef Grid4d<T> type0;
  inline T &getArg1()
  {
    return min;
  }
  typedef T type1;
  inline T &getArg2()
  {
    return max;
  }
  typedef T type2;
  void runMessage()
  {
    debMsg("Executing kernel kn4dClamp ", 3);
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
  Grid4d<T> &me;
  T min;
  T max;
};

template<class T> void Grid4d<T>::add(const Grid4d<T> &a)
{
  Grid4dAdd<T, T>(*this, a);
}
template<class T> void Grid4d<T>::sub(const Grid4d<T> &a)
{
  Grid4dSub<T, T>(*this, a);
}
template<class T> void Grid4d<T>::addScaled(const Grid4d<T> &a, const T &factor)
{
  Grid4dScaledAdd<T, T>(*this, a, factor);
}
template<class T> void Grid4d<T>::setConst(T a)
{
  kn4dSetConstReal<T>(*this, T(a));
}
template<class T> void Grid4d<T>::addConst(T a)
{
  kn4dAddConstReal<T>(*this, T(a));
}
template<class T> void Grid4d<T>::multConst(T a)
{
  kn4dMultConst<T>(*this, a);
}

template<class T> void Grid4d<T>::mult(const Grid4d<T> &a)
{
  Grid4dMult<T, T>(*this, a);
}

template<class T> void Grid4d<T>::clamp(Real min, Real max)
{
  kn4dClamp<T>(*this, T(min), T(max));
}

template<> Real Grid4d<Real>::getMax()
{
  return kn4dMaxReal(*this);
}
template<> Real Grid4d<Real>::getMin()
{
  return kn4dMinReal(*this);
}
template<> Real Grid4d<Real>::getMaxAbs()
{
  Real amin = kn4dMinReal(*this);
  Real amax = kn4dMaxReal(*this);
  return max(fabs(amin), fabs(amax));
}
template<> Real Grid4d<Vec4>::getMax()
{
  return sqrt(kn4dMaxVec<Vec4>(*this));
}
template<> Real Grid4d<Vec4>::getMin()
{
  return sqrt(kn4dMinVec<Vec4>(*this));
}
template<> Real Grid4d<Vec4>::getMaxAbs()
{
  return sqrt(kn4dMaxVec<Vec4>(*this));
}
template<> Real Grid4d<int>::getMax()
{
  return (Real)kn4dMaxInt(*this);
}
template<> Real Grid4d<int>::getMin()
{
  return (Real)kn4dMinInt(*this);
}
template<> Real Grid4d<int>::getMaxAbs()
{
  int amin = kn4dMinInt(*this);
  int amax = kn4dMaxInt(*this);
  return max(fabs((Real)amin), fabs((Real)amax));
}
template<> Real Grid4d<Vec3>::getMax()
{
  return sqrt(kn4dMaxVec<Vec3>(*this));
}
template<> Real Grid4d<Vec3>::getMin()
{
  return sqrt(kn4dMinVec<Vec3>(*this));
}
template<> Real Grid4d<Vec3>::getMaxAbs()
{
  return sqrt(kn4dMaxVec<Vec3>(*this));
}

template<class T> void Grid4d<T>::printGrid(int zSlice, int tSlice, bool printIndex, int bnd)
{
  std::ostringstream out;
  out << std::endl;
  FOR_IJKT_BND(*this, bnd)
  {
    IndexInt idx = (*this).index(i, j, k, t);
    if (((zSlice >= 0 && k == zSlice) || (zSlice < 0)) &&
        ((tSlice >= 0 && t == tSlice) || (tSlice < 0))) {
      out << " ";
      if (printIndex)
        out << "  " << i << "," << j << "," << k << "," << t << ":";
      out << (*this)[idx];
      if (i == (*this).getSizeX() - 1 - bnd) {
        out << std::endl;
        if (j == (*this).getSizeY() - 1 - bnd) {
          out << std::endl;
          if (k == (*this).getSizeZ() - 1 - bnd) {
            out << std::endl;
          }
        }
      }
    }
  }
  out << endl;
  debMsg("Printing '" << this->getName() << "' " << out.str().c_str() << " ", 1);
}

// helper to set/get components of vec4 Grids
struct knGetComp4d : public KernelBase {
  knGetComp4d(const Grid4d<Vec4> &src, Grid4d<Real> &dst, int c)
      : KernelBase(&src, 0), src(src), dst(dst), c(c)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid4d<Vec4> &src, Grid4d<Real> &dst, int c) const
  {
    dst[idx] = src[idx][c];
  }
  inline const Grid4d<Vec4> &getArg0()
  {
    return src;
  }
  typedef Grid4d<Vec4> type0;
  inline Grid4d<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid4d<Real> type1;
  inline int &getArg2()
  {
    return c;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knGetComp4d ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, src, dst, c);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const Grid4d<Vec4> &src;
  Grid4d<Real> &dst;
  int c;
};
;
struct knSetComp4d : public KernelBase {
  knSetComp4d(const Grid4d<Real> &src, Grid4d<Vec4> &dst, int c)
      : KernelBase(&src, 0), src(src), dst(dst), c(c)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid4d<Real> &src, Grid4d<Vec4> &dst, int c) const
  {
    dst[idx][c] = src[idx];
  }
  inline const Grid4d<Real> &getArg0()
  {
    return src;
  }
  typedef Grid4d<Real> type0;
  inline Grid4d<Vec4> &getArg1()
  {
    return dst;
  }
  typedef Grid4d<Vec4> type1;
  inline int &getArg2()
  {
    return c;
  }
  typedef int type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetComp4d ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, src, dst, c);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const Grid4d<Real> &src;
  Grid4d<Vec4> &dst;
  int c;
};
;
void getComp4d(const Grid4d<Vec4> &src, Grid4d<Real> &dst, int c)
{
  knGetComp4d(src, dst, c);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getComp4d", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid4d<Vec4> &src = *_args.getPtr<Grid4d<Vec4>>("src", 0, &_lock);
      Grid4d<Real> &dst = *_args.getPtr<Grid4d<Real>>("dst", 1, &_lock);
      int c = _args.get<int>("c", 2, &_lock);
      _retval = getPyNone();
      getComp4d(src, dst, c);
      _args.check();
    }
    pbFinalizePlugin(parent, "getComp4d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getComp4d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getComp4d("", "getComp4d", _W_0);
extern "C" {
void PbRegister_getComp4d()
{
  KEEP_UNUSED(_RP_getComp4d);
}
}

;
void setComp4d(const Grid4d<Real> &src, Grid4d<Vec4> &dst, int c)
{
  knSetComp4d(src, dst, c);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setComp4d", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const Grid4d<Real> &src = *_args.getPtr<Grid4d<Real>>("src", 0, &_lock);
      Grid4d<Vec4> &dst = *_args.getPtr<Grid4d<Vec4>>("dst", 1, &_lock);
      int c = _args.get<int>("c", 2, &_lock);
      _retval = getPyNone();
      setComp4d(src, dst, c);
      _args.check();
    }
    pbFinalizePlugin(parent, "setComp4d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setComp4d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setComp4d("", "setComp4d", _W_1);
extern "C" {
void PbRegister_setComp4d()
{
  KEEP_UNUSED(_RP_setComp4d);
}
}

;

template<class T> struct knSetBnd4d : public KernelBase {
  knSetBnd4d(Grid4d<T> &grid, T value, int w)
      : KernelBase(&grid, 0), grid(grid), value(value), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, int t, Grid4d<T> &grid, T value, int w) const
  {
    bool bnd = (i <= w || i >= grid.getSizeX() - 1 - w || j <= w || j >= grid.getSizeY() - 1 - w ||
                k <= w || k >= grid.getSizeZ() - 1 - w || t <= w || t >= grid.getSizeT() - 1 - w);
    if (bnd)
      grid(i, j, k, t) = value;
  }
  inline Grid4d<T> &getArg0()
  {
    return grid;
  }
  typedef Grid4d<T> type0;
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
    debMsg("Executing kernel knSetBnd4d ", 3);
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
              op(i, j, k, t, grid, value, w);
    }
    else if (maxZ > 1) {
      const int t = 0;
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < maxY; j++)
          for (int i = 0; i < maxX; i++)
            op(i, j, k, t, grid, value, w);
    }
    else {
      const int t = 0;
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < maxX; i++)
          op(i, j, k, t, grid, value, w);
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
  Grid4d<T> &grid;
  T value;
  int w;
};

template<class T> void Grid4d<T>::setBound(T value, int boundaryWidth)
{
  knSetBnd4d<T>(*this, value, boundaryWidth);
}

template<class T> struct knSetBnd4dNeumann : public KernelBase {
  knSetBnd4dNeumann(Grid4d<T> &grid, int w) : KernelBase(&grid, 0), grid(grid), w(w)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, int t, Grid4d<T> &grid, int w) const
  {
    bool set = false;
    int si = i, sj = j, sk = k, st = t;
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
    if (k <= w) {
      sk = w + 1;
      set = true;
    }
    if (k >= grid.getSizeZ() - 1 - w) {
      sk = grid.getSizeZ() - 1 - w - 1;
      set = true;
    }
    if (t <= w) {
      st = w + 1;
      set = true;
    }
    if (t >= grid.getSizeT() - 1 - w) {
      st = grid.getSizeT() - 1 - w - 1;
      set = true;
    }
    if (set)
      grid(i, j, k, t) = grid(si, sj, sk, st);
  }
  inline Grid4d<T> &getArg0()
  {
    return grid;
  }
  typedef Grid4d<T> type0;
  inline int &getArg1()
  {
    return w;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel knSetBnd4dNeumann ", 3);
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
              op(i, j, k, t, grid, w);
    }
    else if (maxZ > 1) {
      const int t = 0;
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < maxY; j++)
          for (int i = 0; i < maxX; i++)
            op(i, j, k, t, grid, w);
    }
    else {
      const int t = 0;
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < maxX; i++)
          op(i, j, k, t, grid, w);
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
  Grid4d<T> &grid;
  int w;
};

template<class T> void Grid4d<T>::setBoundNeumann(int boundaryWidth)
{
  knSetBnd4dNeumann<T>(*this, boundaryWidth);
}

//******************************************************************************
// testing helpers

//! compute maximal diference of two cells in the grid, needed for testing system

Real grid4dMaxDiff(Grid4d<Real> &g1, Grid4d<Real> &g2)
{
  double maxVal = 0.;
  FOR_IJKT_BND(g1, 0)
  {
    maxVal = std::max(maxVal, (double)fabs(g1(i, j, k, t) - g2(i, j, k, t)));
  }
  return maxVal;
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "grid4dMaxDiff", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Real> &g1 = *_args.getPtr<Grid4d<Real>>("g1", 0, &_lock);
      Grid4d<Real> &g2 = *_args.getPtr<Grid4d<Real>>("g2", 1, &_lock);
      _retval = toPy(grid4dMaxDiff(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "grid4dMaxDiff", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("grid4dMaxDiff", e.what());
    return 0;
  }
}
static const Pb::Register _RP_grid4dMaxDiff("", "grid4dMaxDiff", _W_2);
extern "C" {
void PbRegister_grid4dMaxDiff()
{
  KEEP_UNUSED(_RP_grid4dMaxDiff);
}
}

Real grid4dMaxDiffInt(Grid4d<int> &g1, Grid4d<int> &g2)
{
  double maxVal = 0.;
  FOR_IJKT_BND(g1, 0)
  {
    maxVal = std::max(maxVal, (double)fabs((double)g1(i, j, k, t) - g2(i, j, k, t)));
  }
  return maxVal;
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "grid4dMaxDiffInt", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<int> &g1 = *_args.getPtr<Grid4d<int>>("g1", 0, &_lock);
      Grid4d<int> &g2 = *_args.getPtr<Grid4d<int>>("g2", 1, &_lock);
      _retval = toPy(grid4dMaxDiffInt(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "grid4dMaxDiffInt", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("grid4dMaxDiffInt", e.what());
    return 0;
  }
}
static const Pb::Register _RP_grid4dMaxDiffInt("", "grid4dMaxDiffInt", _W_3);
extern "C" {
void PbRegister_grid4dMaxDiffInt()
{
  KEEP_UNUSED(_RP_grid4dMaxDiffInt);
}
}

Real grid4dMaxDiffVec3(Grid4d<Vec3> &g1, Grid4d<Vec3> &g2)
{
  double maxVal = 0.;
  FOR_IJKT_BND(g1, 0)
  {
    double d = 0.;
    for (int c = 0; c < 3; ++c) {
      d += fabs((double)g1(i, j, k, t)[c] - (double)g2(i, j, k, t)[c]);
    }
    maxVal = std::max(maxVal, d);
  }
  return maxVal;
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "grid4dMaxDiffVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Vec3> &g1 = *_args.getPtr<Grid4d<Vec3>>("g1", 0, &_lock);
      Grid4d<Vec3> &g2 = *_args.getPtr<Grid4d<Vec3>>("g2", 1, &_lock);
      _retval = toPy(grid4dMaxDiffVec3(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "grid4dMaxDiffVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("grid4dMaxDiffVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_grid4dMaxDiffVec3("", "grid4dMaxDiffVec3", _W_4);
extern "C" {
void PbRegister_grid4dMaxDiffVec3()
{
  KEEP_UNUSED(_RP_grid4dMaxDiffVec3);
}
}

Real grid4dMaxDiffVec4(Grid4d<Vec4> &g1, Grid4d<Vec4> &g2)
{
  double maxVal = 0.;
  FOR_IJKT_BND(g1, 0)
  {
    double d = 0.;
    for (int c = 0; c < 4; ++c) {
      d += fabs((double)g1(i, j, k, t)[c] - (double)g2(i, j, k, t)[c]);
    }
    maxVal = std::max(maxVal, d);
  }
  return maxVal;
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "grid4dMaxDiffVec4", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Vec4> &g1 = *_args.getPtr<Grid4d<Vec4>>("g1", 0, &_lock);
      Grid4d<Vec4> &g2 = *_args.getPtr<Grid4d<Vec4>>("g2", 1, &_lock);
      _retval = toPy(grid4dMaxDiffVec4(g1, g2));
      _args.check();
    }
    pbFinalizePlugin(parent, "grid4dMaxDiffVec4", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("grid4dMaxDiffVec4", e.what());
    return 0;
  }
}
static const Pb::Register _RP_grid4dMaxDiffVec4("", "grid4dMaxDiffVec4", _W_5);
extern "C" {
void PbRegister_grid4dMaxDiffVec4()
{
  KEEP_UNUSED(_RP_grid4dMaxDiffVec4);
}
}

// set a region to some value

template<class S> struct knSetRegion4d : public KernelBase {
  knSetRegion4d(Grid4d<S> &dst, Vec4 start, Vec4 end, S value)
      : KernelBase(&dst, 0), dst(dst), start(start), end(end), value(value)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, int t, Grid4d<S> &dst, Vec4 start, Vec4 end, S value) const
  {
    Vec4 p(i, j, k, t);
    for (int c = 0; c < 4; ++c)
      if (p[c] < start[c] || p[c] > end[c])
        return;
    dst(i, j, k, t) = value;
  }
  inline Grid4d<S> &getArg0()
  {
    return dst;
  }
  typedef Grid4d<S> type0;
  inline Vec4 &getArg1()
  {
    return start;
  }
  typedef Vec4 type1;
  inline Vec4 &getArg2()
  {
    return end;
  }
  typedef Vec4 type2;
  inline S &getArg3()
  {
    return value;
  }
  typedef S type3;
  void runMessage()
  {
    debMsg("Executing kernel knSetRegion4d ", 3);
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
              op(i, j, k, t, dst, start, end, value);
    }
    else if (maxZ > 1) {
      const int t = 0;
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < maxY; j++)
          for (int i = 0; i < maxX; i++)
            op(i, j, k, t, dst, start, end, value);
    }
    else {
      const int t = 0;
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < maxX; i++)
          op(i, j, k, t, dst, start, end, value);
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
  Grid4d<S> &dst;
  Vec4 start;
  Vec4 end;
  S value;
};
//! simple init functions in 4d
void setRegion4d(Grid4d<Real> &dst, Vec4 start, Vec4 end, Real value)
{
  knSetRegion4d<Real>(dst, start, end, value);
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setRegion4d", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Real> &dst = *_args.getPtr<Grid4d<Real>>("dst", 0, &_lock);
      Vec4 start = _args.get<Vec4>("start", 1, &_lock);
      Vec4 end = _args.get<Vec4>("end", 2, &_lock);
      Real value = _args.get<Real>("value", 3, &_lock);
      _retval = getPyNone();
      setRegion4d(dst, start, end, value);
      _args.check();
    }
    pbFinalizePlugin(parent, "setRegion4d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setRegion4d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setRegion4d("", "setRegion4d", _W_6);
extern "C" {
void PbRegister_setRegion4d()
{
  KEEP_UNUSED(_RP_setRegion4d);
}
}

//! simple init functions in 4d, vec4
void setRegion4dVec4(Grid4d<Vec4> &dst, Vec4 start, Vec4 end, Vec4 value)
{
  knSetRegion4d<Vec4>(dst, start, end, value);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setRegion4dVec4", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Vec4> &dst = *_args.getPtr<Grid4d<Vec4>>("dst", 0, &_lock);
      Vec4 start = _args.get<Vec4>("start", 1, &_lock);
      Vec4 end = _args.get<Vec4>("end", 2, &_lock);
      Vec4 value = _args.get<Vec4>("value", 3, &_lock);
      _retval = getPyNone();
      setRegion4dVec4(dst, start, end, value);
      _args.check();
    }
    pbFinalizePlugin(parent, "setRegion4dVec4", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setRegion4dVec4", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setRegion4dVec4("", "setRegion4dVec4", _W_7);
extern "C" {
void PbRegister_setRegion4dVec4()
{
  KEEP_UNUSED(_RP_setRegion4dVec4);
}
}

//! slow helper to visualize tests, get a 3d slice of a 4d grid
void getSliceFrom4d(Grid4d<Real> &src, int srct, Grid<Real> &dst)
{
  const int bnd = 0;
  if (!src.isInBounds(Vec4i(bnd, bnd, bnd, srct)))
    return;

  for (int k = bnd; k < src.getSizeZ() - bnd; k++)
    for (int j = bnd; j < src.getSizeY() - bnd; j++)
      for (int i = bnd; i < src.getSizeX() - bnd; i++) {
        if (!dst.isInBounds(Vec3i(i, j, k)))
          continue;
        dst(i, j, k) = src(i, j, k, srct);
      }
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getSliceFrom4d", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Real> &src = *_args.getPtr<Grid4d<Real>>("src", 0, &_lock);
      int srct = _args.get<int>("srct", 1, &_lock);
      Grid<Real> &dst = *_args.getPtr<Grid<Real>>("dst", 2, &_lock);
      _retval = getPyNone();
      getSliceFrom4d(src, srct, dst);
      _args.check();
    }
    pbFinalizePlugin(parent, "getSliceFrom4d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getSliceFrom4d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getSliceFrom4d("", "getSliceFrom4d", _W_8);
extern "C" {
void PbRegister_getSliceFrom4d()
{
  KEEP_UNUSED(_RP_getSliceFrom4d);
}
}

//! slow helper to visualize tests, get a 3d slice of a 4d vec4 grid
void getSliceFrom4dVec(Grid4d<Vec4> &src, int srct, Grid<Vec3> &dst, Grid<Real> *dstt = NULL)
{
  const int bnd = 0;
  if (!src.isInBounds(Vec4i(bnd, bnd, bnd, srct)))
    return;

  for (int k = bnd; k < src.getSizeZ() - bnd; k++)
    for (int j = bnd; j < src.getSizeY() - bnd; j++)
      for (int i = bnd; i < src.getSizeX() - bnd; i++) {
        if (!dst.isInBounds(Vec3i(i, j, k)))
          continue;
        for (int c = 0; c < 3; ++c)
          dst(i, j, k)[c] = src(i, j, k, srct)[c];
        if (dstt)
          (*dstt)(i, j, k) = src(i, j, k, srct)[3];
      }
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getSliceFrom4dVec", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Vec4> &src = *_args.getPtr<Grid4d<Vec4>>("src", 0, &_lock);
      int srct = _args.get<int>("srct", 1, &_lock);
      Grid<Vec3> &dst = *_args.getPtr<Grid<Vec3>>("dst", 2, &_lock);
      Grid<Real> *dstt = _args.getPtrOpt<Grid<Real>>("dstt", 3, NULL, &_lock);
      _retval = getPyNone();
      getSliceFrom4dVec(src, srct, dst, dstt);
      _args.check();
    }
    pbFinalizePlugin(parent, "getSliceFrom4dVec", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getSliceFrom4dVec", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getSliceFrom4dVec("", "getSliceFrom4dVec", _W_9);
extern "C" {
void PbRegister_getSliceFrom4dVec()
{
  KEEP_UNUSED(_RP_getSliceFrom4dVec);
}
}

//******************************************************************************
// interpolation

//! same as in grid.h , but takes an additional optional "desired" size
static inline void gridFactor4d(
    Vec4 s1, Vec4 s2, Vec4 optSize, Vec4 scale, Vec4 &srcFac, Vec4 &retOff)
{
  for (int c = 0; c < 4; c++) {
    if (optSize[c] > 0.) {
      s2[c] = optSize[c];
    }
  }
  srcFac = calcGridSizeFactor4d(s1, s2) / scale;
  retOff = -retOff * srcFac + srcFac * 0.5;
}

//! interpolate 4d grid from one size to another size
// real valued offsets & scale

template<class S> struct knInterpol4d : public KernelBase {
  knInterpol4d(Grid4d<S> &target, Grid4d<S> &source, const Vec4 &srcFac, const Vec4 &offset)
      : KernelBase(&target, 0), target(target), source(source), srcFac(srcFac), offset(offset)
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
                 const Vec4 &srcFac,
                 const Vec4 &offset) const
  {
    Vec4 pos = Vec4(i, j, k, t) * srcFac + offset;
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
    return srcFac;
  }
  typedef Vec4 type2;
  inline const Vec4 &getArg3()
  {
    return offset;
  }
  typedef Vec4 type3;
  void runMessage()
  {
    debMsg("Executing kernel knInterpol4d ", 3);
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
              op(i, j, k, t, target, source, srcFac, offset);
    }
    else if (maxZ > 1) {
      const int t = 0;
      for (int k = __r.begin(); k != (int)__r.end(); k++)
        for (int j = 0; j < maxY; j++)
          for (int i = 0; i < maxX; i++)
            op(i, j, k, t, target, source, srcFac, offset);
    }
    else {
      const int t = 0;
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < maxX; i++)
          op(i, j, k, t, target, source, srcFac, offset);
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
  const Vec4 &srcFac;
  const Vec4 &offset;
};
//! linearly interpolate data of a 4d grid

void interpolateGrid4d(Grid4d<Real> &target,
                       Grid4d<Real> &source,
                       Vec4 offset = Vec4(0.),
                       Vec4 scale = Vec4(1.),
                       Vec4 size = Vec4(-1.))
{
  Vec4 srcFac(1.), off2 = offset;
  gridFactor4d(toVec4(source.getSize()), toVec4(target.getSize()), size, scale, srcFac, off2);
  knInterpol4d<Real>(target, source, srcFac, off2);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "interpolateGrid4d", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Real> &target = *_args.getPtr<Grid4d<Real>>("target", 0, &_lock);
      Grid4d<Real> &source = *_args.getPtr<Grid4d<Real>>("source", 1, &_lock);
      Vec4 offset = _args.getOpt<Vec4>("offset", 2, Vec4(0.), &_lock);
      Vec4 scale = _args.getOpt<Vec4>("scale", 3, Vec4(1.), &_lock);
      Vec4 size = _args.getOpt<Vec4>("size", 4, Vec4(-1.), &_lock);
      _retval = getPyNone();
      interpolateGrid4d(target, source, offset, scale, size);
      _args.check();
    }
    pbFinalizePlugin(parent, "interpolateGrid4d", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("interpolateGrid4d", e.what());
    return 0;
  }
}
static const Pb::Register _RP_interpolateGrid4d("", "interpolateGrid4d", _W_10);
extern "C" {
void PbRegister_interpolateGrid4d()
{
  KEEP_UNUSED(_RP_interpolateGrid4d);
}
}

//! linearly interpolate vec4 data of a 4d grid

void interpolateGrid4dVec(Grid4d<Vec4> &target,
                          Grid4d<Vec4> &source,
                          Vec4 offset = Vec4(0.),
                          Vec4 scale = Vec4(1.),
                          Vec4 size = Vec4(-1.))
{
  Vec4 srcFac(1.), off2 = offset;
  gridFactor4d(toVec4(source.getSize()), toVec4(target.getSize()), size, scale, srcFac, off2);
  knInterpol4d<Vec4>(target, source, srcFac, off2);
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "interpolateGrid4dVec", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid4d<Vec4> &target = *_args.getPtr<Grid4d<Vec4>>("target", 0, &_lock);
      Grid4d<Vec4> &source = *_args.getPtr<Grid4d<Vec4>>("source", 1, &_lock);
      Vec4 offset = _args.getOpt<Vec4>("offset", 2, Vec4(0.), &_lock);
      Vec4 scale = _args.getOpt<Vec4>("scale", 3, Vec4(1.), &_lock);
      Vec4 size = _args.getOpt<Vec4>("size", 4, Vec4(-1.), &_lock);
      _retval = getPyNone();
      interpolateGrid4dVec(target, source, offset, scale, size);
      _args.check();
    }
    pbFinalizePlugin(parent, "interpolateGrid4dVec", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("interpolateGrid4dVec", e.what());
    return 0;
  }
}
static const Pb::Register _RP_interpolateGrid4dVec("", "interpolateGrid4dVec", _W_11);
extern "C" {
void PbRegister_interpolateGrid4dVec()
{
  KEEP_UNUSED(_RP_interpolateGrid4dVec);
}
}

// explicit instantiation
template class Grid4d<int>;
template class Grid4d<Real>;
template class Grid4d<Vec3>;
template class Grid4d<Vec4>;

}  // namespace Manta
