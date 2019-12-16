

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
 * Multigrid solver by Florian Ferstl (florian.ferstl.ff@gmail.com)
 *
 * This is an implementation of the solver developed by Dick et al. [1]
 * without topology awareness (= vertex duplication on coarser levels). This
 * simplification allows us to use regular grids for all levels of the multigrid
 * hierarchy and works well for moderately complex domains.
 *
 * [1] Solving the Fluid Pressure Poisson Equation Using Multigrid-Evaluation
 *     and Improvements, C. Dick, M. Rogowsky, R. Westermann, IEEE TVCG 2015
 *
 ******************************************************************************/

#ifndef _MULTIGRID_H
#define _MULTIGRID_H

#include "vectorbase.h"
#include "grid.h"

namespace Manta {

//! Multigrid solver
class GridMg {
 public:
  //! constructor: preallocates most of required memory for multigrid hierarchy
  GridMg(const Vec3i &gridSize);
  ~GridMg(){};

  //! update system matrix A from symmetric 7-point stencil
  void setA(const Grid<Real> *pA0,
            const Grid<Real> *pAi,
            const Grid<Real> *pAj,
            const Grid<Real> *pAk);

  //! set right-hand side after setting A
  void setRhs(const Grid<Real> &rhs);

  bool isASet() const
  {
    return mIsASet;
  }
  bool isRhsSet() const
  {
    return mIsRhsSet;
  }

  //! perform VCycle iteration
  // - if src is null, then a zero vector is used instead
  // - returns norm of residual after VCylcle
  Real doVCycle(Grid<Real> &dst, const Grid<Real> *src = nullptr);

  // access
  void setCoarsestLevelAccuracy(Real accuracy)
  {
    mCoarsestLevelAccuracy = accuracy;
  }
  Real getCoarsestLevelAccuracy() const
  {
    return mCoarsestLevelAccuracy;
  }
  void setSmoothing(int numPreSmooth, int numPostSmooth)
  {
    mNumPreSmooth = numPreSmooth;
    mNumPostSmooth = numPostSmooth;
  }
  int getNumPreSmooth() const
  {
    return mNumPreSmooth;
  }
  int getNumPostSmooth() const
  {
    return mNumPostSmooth;
  }

  //! Set factor for automated downscaling of trivial equations:
  // 1*x_i = b_i  --->  trivialEquationScale*x_i = trivialEquationScale*b_i
  // Factor should be significantly smaller than the scale of the entries in A.
  //     Info: Trivial equations of the form x_i = b_i can have a negative
  //     effect on the coarse grid operators of the multigrid hierarchy (due
  //     to scaling mismatches), which can lead to slow multigrid convergence.
  //     To avoid this, the solver checks for such equations when updating A
  //     (and rhs) and scales these equations by a fixed factor < 1.
  void setTrivialEquationScale(Real scale)
  {
    mTrivialEquationScale = scale;
  }

 private:
  Vec3i vecIdx(int v, int l) const
  {
    return Vec3i(v % mSize[l].x,
                 (v % (mSize[l].x * mSize[l].y)) / mSize[l].x,
                 v / (mSize[l].x * mSize[l].y));
  }
  int linIdx(Vec3i V, int l) const
  {
    return V.x + V.y * mPitch[l].y + V.z * mPitch[l].z;
  }
  bool inGrid(Vec3i V, int l) const
  {
    return V.x >= 0 && V.y >= 0 && V.z >= 0 && V.x < mSize[l].x && V.y < mSize[l].y &&
           V.z < mSize[l].z;
  }

  void analyzeStencil(int v, bool is3D, bool &isStencilSumNonZero, bool &isEquationTrivial) const;

  void genCoarseGrid(int l);
  void genCoraseGridOperator(int l);

  void smoothGS(int l, bool reversedOrder);
  void calcResidual(int l);
  Real calcResidualNorm(int l);
  void solveCG(int l);

  void restrict(int l_dst, const std::vector<Real> &src, std::vector<Real> &dst) const;
  void interpolate(int l_dst, const std::vector<Real> &src, std::vector<Real> &dst) const;

 private:
  enum VertexType : char {
    vtInactive = 0,
    vtActive = 1,
    vtActiveTrivial = 2,  // only on finest level 0
    vtRemoved = 3,        //-+
    vtZero = 4,           // +-- only during coarse grid generation
    vtFree = 5            //-+
  };

  struct CoarseningPath {
    Vec3i U, W, N;
    int sc, sf;
    Real rw, iw;
    bool inUStencil;
  };

  int mNumPreSmooth;
  int mNumPostSmooth;
  Real mCoarsestLevelAccuracy;
  Real mTrivialEquationScale;

  std::vector<std::vector<Real>> mA;
  std::vector<std::vector<Real>> mx;
  std::vector<std::vector<Real>> mb;
  std::vector<std::vector<Real>> mr;
  std::vector<std::vector<VertexType>> mType;
  std::vector<std::vector<double>> mCGtmp1, mCGtmp2, mCGtmp3, mCGtmp4;
  std::vector<Vec3i> mSize, mPitch;
  std::vector<CoarseningPath> mCoarseningPaths0;

  bool mIs3D;
  int mDim;
  int mStencilSize;
  int mStencilSize0;
  Vec3i mStencilMin;
  Vec3i mStencilMax;

  bool mIsASet;
  bool mIsRhsSet;

  // provide kernels with access
  friend struct knActivateVertices;
  friend struct knActivateCoarseVertices;
  friend struct knSetRhs;
  friend struct knGenCoarseGridOperator;
  friend struct knSmoothColor;
  friend struct knCalcResidual;
  friend struct knResidualNormSumSqr;
  friend struct knRestrict;
  friend struct knInterpolate;
};  // GridMg

}  // namespace Manta

#endif
