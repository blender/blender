

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
 * Fast marching
 *
 ******************************************************************************/

#ifndef _FASTMARCH_H
#define _FASTMARCH_H

#include <queue>
#include "levelset.h"

namespace Manta {

//! Fast marching. Transport certain values
//  This class exists in two versions: for scalar, and for vector values - the only difference are
//  flag checks i transpTouch (for simplicity in separate classes)

template<class GRID, class T>
inline T fmInterpolateNeighbors(GRID *mpVal, int x, int y, int z, Real *weights)
{
  T val(0.);
  if (weights[0] > 0.0)
    val += mpVal->get(x + 1, y + 0, z + 0) * weights[0];
  if (weights[1] > 0.0)
    val += mpVal->get(x - 1, y + 0, z + 0) * weights[1];
  if (weights[2] > 0.0)
    val += mpVal->get(x + 0, y + 1, z + 0) * weights[2];
  if (weights[3] > 0.0)
    val += mpVal->get(x + 0, y - 1, z + 0) * weights[3];
  if (mpVal->is3D()) {
    if (weights[4] > 0.0)
      val += mpVal->get(x + 0, y + 0, z + 1) * weights[4];
    if (weights[5] > 0.0)
      val += mpVal->get(x + 0, y + 0, z - 1) * weights[5];
  }
  return val;
}

template<class GRID, class T> class FmValueTransportScalar {
 public:
  FmValueTransportScalar() : mpVal(0), mpFlags(0){};
  ~FmValueTransportScalar(){};
  void initMarching(GRID *val, FlagGrid *flags)
  {
    mpVal = val;
    mpFlags = flags;
  }
  inline bool isInitialized()
  {
    return mpVal != 0;
  }

  //! cell is touched by marching from source cell
  inline void transpTouch(int x, int y, int z, Real *weights, Real time)
  {
    if (!mpVal || !mpFlags->isEmpty(x, y, z))
      return;
    T val = fmInterpolateNeighbors<GRID, T>(mpVal, x, y, z, weights);
    (*mpVal)(x, y, z) = val;
  };

 protected:
  GRID *mpVal;
  FlagGrid *mpFlags;
};

template<class GRID, class T> class FmValueTransportVec3 {
 public:
  FmValueTransportVec3() : mpVal(0), mpFlags(0){};
  ~FmValueTransportVec3(){};
  inline bool isInitialized()
  {
    return mpVal != 0;
  }
  void initMarching(GRID *val, const FlagGrid *flags)
  {
    mpVal = val;
    mpFlags = flags;
  }

  //! cell is touched by marching from source cell
  inline void transpTouch(int x, int y, int z, Real *weights, Real time)
  {
    if (!mpVal || !mpFlags->isEmpty(x, y, z))
      return;

    T val = fmInterpolateNeighbors<GRID, T>(mpVal, x, y, z, weights);

    // set velocity components if adjacent is empty
    if (mpFlags->isEmpty(x - 1, y, z))
      (*mpVal)(x, y, z).x = val.x;
    if (mpFlags->isEmpty(x, y - 1, z))
      (*mpVal)(x, y, z).y = val.y;
    if (mpVal->is3D()) {
      if (mpFlags->isEmpty(x, y, z - 1))
        (*mpVal)(x, y, z).z = val.z;
    }
  };

 protected:
  GRID *mpVal;
  const FlagGrid *mpFlags;
};

class FmHeapEntryOut {
 public:
  Vec3i p;
  // quick time access for sorting
  Real time;
  static inline bool compare(const Real x, const Real y)
  {
    return x > y;
  }

  inline bool operator<(const FmHeapEntryOut &o) const
  {
    const Real d = fabs((time) - ((o.time)));
    if (d > 0.)
      return (time) > ((o.time));
    if (p.z != o.p.z)
      return p.z > o.p.z;
    if (p.y != o.p.y)
      return p.y > o.p.y;
    return p.x > o.p.x;
  };
};

class FmHeapEntryIn {
 public:
  Vec3i p;
  // quick time access for sorting
  Real time;
  static inline bool compare(const Real x, const Real y)
  {
    return x < y;
  }

  inline bool operator<(const FmHeapEntryIn &o) const
  {
    const Real d = fabs((time) - ((o.time)));
    if (d > 0.)
      return (time) < ((o.time));
    if (p.z != o.p.z)
      return p.z < o.p.z;
    if (p.y != o.p.y)
      return p.y < o.p.y;
    return p.x < o.p.x;
  };
};

//! fast marching algorithm wrapper class
template<class T, int TDIR> class FastMarch {

 public:
  // MSVC doesn't allow static const variables in template classes
  static inline Real InvalidTime()
  {
    return -1000;
  }
  static inline Real InvtOffset()
  {
    return 500;
  }

  enum SpecialValues { FlagInited = 1, FlagIsOnHeap = 2 };

  FastMarch(const FlagGrid &flags,
            Grid<int> &fmFlags,
            Grid<Real> &levelset,
            Real maxTime,
            MACGrid *velTransport = nullptr);
  ~FastMarch()
  {
  }

  //! advect level set function with given velocity */
  void performMarching();

  //! test value for invalidity
  inline bool isInvalid(Real v) const
  {
    return (v <= InvalidTime());
  }

  void addToList(const Vec3i &p, const Vec3i &src);

  //! convert phi to time value
  inline Real phi2time(Real phival)
  {
    return (phival - InvalidTime() + InvtOffset()) * -1.0;
  }

  //! ... and back
  inline Real time2phi(Real tval)
  {
    return (InvalidTime() - InvtOffset() - tval);
  }

  inline Real _phi(int i, int j, int k)
  {
    return mLevelset(i, j, k);
  }

 protected:
  Grid<Real> &mLevelset;
  const FlagGrid &mFlags;
  Grid<int> &mFmFlags;

  //! velocity extrpolation
  FmValueTransportVec3<MACGrid, Vec3> mVelTransport;

  //! maximal time to march for
  Real mMaxTime;

  //! fast marching list
  std::priority_queue<T, std::vector<T>, std::less<T>> mHeap;
  Real mReheapVal;

  //! weights for touching points
  Real mWeights[6];

  template<int C> inline Real calcWeights(int &okCnt, int &invcnt, Real *v, const Vec3i &idx);

  inline Real calculateDistance(const Vec3i &pos);
};

}  // namespace Manta
#endif
