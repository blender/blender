

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
 * Conjugate gradient solver
 *
 ******************************************************************************/

#ifndef _CONJUGATEGRADIENT_H
#define _CONJUGATEGRADIENT_H

#include "vectorbase.h"
#include "grid.h"
#include "kernel.h"
#include "multigrid.h"

namespace Manta {

static const bool CG_DEBUG = false;

//! Basic CG interface
class GridCgInterface {
 public:
  enum PreconditionType { PC_None = 0, PC_ICP, PC_mICP, PC_MGP };

  GridCgInterface() : mUseL2Norm(true){};
  virtual ~GridCgInterface(){};

  // solving functions
  virtual bool iterate() = 0;
  virtual void solve(int maxIter) = 0;

  // precond
  virtual void setICPreconditioner(
      PreconditionType method, Grid<Real> *A0, Grid<Real> *Ai, Grid<Real> *Aj, Grid<Real> *Ak) = 0;
  virtual void setMGPreconditioner(PreconditionType method, GridMg *MG) = 0;

  // access
  virtual Real getSigma() const = 0;
  virtual Real getIterations() const = 0;
  virtual Real getResNorm() const = 0;
  virtual void setAccuracy(Real set) = 0;
  virtual Real getAccuracy() const = 0;

  //! force reinit upon next iterate() call, can be used for doing multiple solves
  virtual void forceReinit() = 0;

  void setUseL2Norm(bool set)
  {
    mUseL2Norm = set;
  }

 protected:
  // use l2 norm of residualfor threshold? (otherwise uses max norm)
  bool mUseL2Norm;
};

//! Run single iteration of the cg solver
/*! the template argument determines the type of matrix multiplication,
  typically a ApplyMatrix kernel, another one is needed e.g. for the
  mesh-based wave equation solver */
template<class APPLYMAT> class GridCg : public GridCgInterface {
 public:
  //! constructor
  GridCg(Grid<Real> &dst,
         Grid<Real> &rhs,
         Grid<Real> &residual,
         Grid<Real> &search,
         const FlagGrid &flags,
         Grid<Real> &tmp,
         std::vector<Grid<Real> *> matrixAVec,
         std::vector<Grid<Real> *> rhsVec = {});
  ~GridCg(){};

  void doInit();
  bool iterate();
  void solve(int maxIter);
  //! init pointers, and copy values from "normal" matrix
  void setICPreconditioner(
      PreconditionType method, Grid<Real> *A0, Grid<Real> *Ai, Grid<Real> *Aj, Grid<Real> *Ak);
  void setMGPreconditioner(PreconditionType method, GridMg *MG);
  void forceReinit()
  {
    mInited = false;
  }

  // Accessors
  Real getSigma() const
  {
    return mSigma;
  }
  Real getIterations() const
  {
    return mIterations;
  }

  Real getResNorm() const
  {
    return mResNorm;
  }

  void setAccuracy(Real set)
  {
    mAccuracy = set;
  }
  Real getAccuracy() const
  {
    return mAccuracy;
  }

 protected:
  bool mInited;
  int mIterations;
  // grids
  Grid<Real> &mDst;
  Grid<Real> &mRhs;
  Grid<Real> &mResidual;
  Grid<Real> &mSearch;
  const FlagGrid &mFlags;
  Grid<Real> &mTmp;

  //! shape of A matrix defined here (e.g. diagonal,  positive neighbor cells, etc)
  std::vector<Grid<Real> *> mMatrixA;
  //! shape of rhs vector defined here (e.g. 1 rhs for regular fluids solve, 3 rhs for viscosity)
  std::vector<Grid<Real> *> mVecRhs;

  PreconditionType mPcMethod;
  //! preconditioning grids
  Grid<Real> *mpPCA0, *mpPCAi, *mpPCAj, *mpPCAk;
  GridMg *mMG;

  //! sigma / residual
  Real mSigma;
  //! accuracy of solver (max. residuum)
  Real mAccuracy;
  //! norm of the residual
  Real mResNorm;
};  // GridCg

//! Kernel: Apply symmetric stored Matrix

struct ApplyMatrix : public KernelBase {
  ApplyMatrix(const FlagGrid &flags,
              Grid<Real> &dst,
              const Grid<Real> &src,
              const std::vector<Grid<Real> *> matrixA,
              const std::vector<Grid<Real> *> vecRhs)
      : KernelBase(&flags, 0), flags(flags), dst(dst), src(src), matrixA(matrixA), vecRhs(vecRhs)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 const Grid<Real> &src,
                 const std::vector<Grid<Real> *> matrixA,
                 const std::vector<Grid<Real> *> vecRhs) const
  {
    unusedParameter(vecRhs);  // Not needed in this matrix application

    if (matrixA.size() != 4)
      errMsg("ConjugateGrad: Invalid A matrix in apply matrix step");
    Grid<Real> &A0 = *matrixA[0];
    Grid<Real> &Ai = *matrixA[1];
    Grid<Real> &Aj = *matrixA[2];
    Grid<Real> &Ak = *matrixA[3];

    if (!flags.isFluid(idx)) {
      dst[idx] = src[idx];
      return;
    }

    dst[idx] = src[idx] * A0[idx] + src[idx - X] * Ai[idx - X] + src[idx + X] * Ai[idx] +
               src[idx - Y] * Aj[idx - Y] + src[idx + Y] * Aj[idx] + src[idx - Z] * Ak[idx - Z] +
               src[idx + Z] * Ak[idx];
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return src;
  }
  typedef Grid<Real> type2;
  inline const std::vector<Grid<Real> *> &getArg3()
  {
    return matrixA;
  }
  typedef std::vector<Grid<Real> *> type3;
  inline const std::vector<Grid<Real> *> &getArg4()
  {
    return vecRhs;
  }
  typedef std::vector<Grid<Real> *> type4;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMatrix ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, dst, src, matrixA, vecRhs);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  const Grid<Real> &src;
  const std::vector<Grid<Real> *> matrixA;
  const std::vector<Grid<Real> *> vecRhs;
};

//! Kernel: Apply symmetric stored Matrix. 2D version

struct ApplyMatrix2D : public KernelBase {
  ApplyMatrix2D(const FlagGrid &flags,
                Grid<Real> &dst,
                const Grid<Real> &src,
                const std::vector<Grid<Real> *> matrixA,
                const std::vector<Grid<Real> *> vecRhs)
      : KernelBase(&flags, 0), flags(flags), dst(dst), src(src), matrixA(matrixA), vecRhs(vecRhs)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 const Grid<Real> &src,
                 const std::vector<Grid<Real> *> matrixA,
                 const std::vector<Grid<Real> *> vecRhs) const
  {
    unusedParameter(vecRhs);  // Not needed in this matrix application

    if (matrixA.size() != 3)
      errMsg("ConjugateGrad: Invalid A matrix in apply matrix step");
    Grid<Real> &A0 = *matrixA[0];
    Grid<Real> &Ai = *matrixA[1];
    Grid<Real> &Aj = *matrixA[2];

    if (!flags.isFluid(idx)) {
      dst[idx] = src[idx];
      return;
    }

    dst[idx] = src[idx] * A0[idx] + src[idx - X] * Ai[idx - X] + src[idx + X] * Ai[idx] +
               src[idx - Y] * Aj[idx - Y] + src[idx + Y] * Aj[idx];
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return src;
  }
  typedef Grid<Real> type2;
  inline const std::vector<Grid<Real> *> &getArg3()
  {
    return matrixA;
  }
  typedef std::vector<Grid<Real> *> type3;
  inline const std::vector<Grid<Real> *> &getArg4()
  {
    return vecRhs;
  }
  typedef std::vector<Grid<Real> *> type4;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMatrix2D ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, dst, src, matrixA, vecRhs);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  const Grid<Real> &src;
  const std::vector<Grid<Real> *> matrixA;
  const std::vector<Grid<Real> *> vecRhs;
};

struct ApplyMatrixViscosityU : public KernelBase {
  ApplyMatrixViscosityU(const FlagGrid &flags,
                        Grid<Real> &dst,
                        const Grid<Real> &src,
                        const std::vector<Grid<Real> *> matrixA,
                        const std::vector<Grid<Real> *> vecRhs)
      : KernelBase(&flags, 1), flags(flags), dst(dst), src(src), matrixA(matrixA), vecRhs(vecRhs)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 const Grid<Real> &src,
                 const std::vector<Grid<Real> *> matrixA,
                 const std::vector<Grid<Real> *> vecRhs) const
  {
    if (matrixA.size() != 15)
      errMsg("ConjugateGrad: Invalid A matrix in apply matrix step");
    Grid<Real> &A0 = *matrixA[0];
    Grid<Real> &Aplusi = *matrixA[1];
    Grid<Real> &Aplusj = *matrixA[2];
    Grid<Real> &Aplusk = *matrixA[3];
    Grid<Real> &Aminusi = *matrixA[4];
    Grid<Real> &Aminusj = *matrixA[5];
    Grid<Real> &Aminusk = *matrixA[6];

    if (vecRhs.size() != 2)
      errMsg("ConjugateGrad: Invalid rhs vector in apply matrix step");
    Grid<Real> &srcV = *vecRhs[0];
    Grid<Real> &srcW = *vecRhs[1];

    dst(i, j, k) = src(i, j, k) * A0(i, j, k) + src(i + 1, j, k) * Aplusi(i, j, k) +
                   src(i, j + 1, k) * Aplusj(i, j, k) + src(i, j, k + 1) * Aplusk(i, j, k) +
                   src(i - 1, j, k) * Aminusi(i, j, k) + src(i, j - 1, k) * Aminusj(i, j, k) +
                   src(i, j, k - 1) * Aminusk(i, j, k);

    dst(i, j, k) += srcV(i, j + 1, k) * (*matrixA[7])(i, j, k) +
                    srcV(i - 1, j + 1, k) * (*matrixA[8])(i, j, k) +
                    srcV(i, j, k) * (*matrixA[9])(i, j, k) +
                    srcV(i - 1, j, k) * (*matrixA[10])(i, j, k) +
                    srcW(i, j, k + 1) * (*matrixA[11])(i, j, k) +
                    srcW(i - 1, j, k + 1) * (*matrixA[12])(i, j, k) +
                    srcW(i, j, k) * (*matrixA[13])(i, j, k) +
                    srcW(i - 1, j, k) * (*matrixA[14])(i, j, k);
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return src;
  }
  typedef Grid<Real> type2;
  inline const std::vector<Grid<Real> *> &getArg3()
  {
    return matrixA;
  }
  typedef std::vector<Grid<Real> *> type3;
  inline const std::vector<Grid<Real> *> &getArg4()
  {
    return vecRhs;
  }
  typedef std::vector<Grid<Real> *> type4;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMatrixViscosityU ", 3);
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
            op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  const Grid<Real> &src;
  const std::vector<Grid<Real> *> matrixA;
  const std::vector<Grid<Real> *> vecRhs;
};

struct ApplyMatrixViscosityV : public KernelBase {
  ApplyMatrixViscosityV(const FlagGrid &flags,
                        Grid<Real> &dst,
                        const Grid<Real> &src,
                        const std::vector<Grid<Real> *> matrixA,
                        const std::vector<Grid<Real> *> vecRhs)
      : KernelBase(&flags, 1), flags(flags), dst(dst), src(src), matrixA(matrixA), vecRhs(vecRhs)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 const Grid<Real> &src,
                 const std::vector<Grid<Real> *> matrixA,
                 const std::vector<Grid<Real> *> vecRhs) const
  {
    if (matrixA.size() != 15)
      errMsg("ConjugateGrad: Invalid A matrix in apply matrix step");
    Grid<Real> &A0 = *matrixA[0];
    Grid<Real> &Aplusi = *matrixA[1];
    Grid<Real> &Aplusj = *matrixA[2];
    Grid<Real> &Aplusk = *matrixA[3];
    Grid<Real> &Aminusi = *matrixA[4];
    Grid<Real> &Aminusj = *matrixA[5];
    Grid<Real> &Aminusk = *matrixA[6];

    if (vecRhs.size() != 2)
      errMsg("ConjugateGrad: Invalid rhs vector in apply matrix step");
    Grid<Real> &srcU = *vecRhs[0];
    Grid<Real> &srcW = *vecRhs[1];

    dst(i, j, k) = src(i, j, k) * A0(i, j, k) + src(i + 1, j, k) * Aplusi(i, j, k) +
                   src(i, j + 1, k) * Aplusj(i, j, k) + src(i, j, k + 1) * Aplusk(i, j, k) +
                   src(i - 1, j, k) * Aminusi(i, j, k) + src(i, j - 1, k) * Aminusj(i, j, k) +
                   src(i, j, k - 1) * Aminusk(i, j, k);

    dst(i, j, k) += srcU(i + 1, j, k) * (*matrixA[7])(i, j, k) +
                    srcU(i + 1, j - 1, k) * (*matrixA[8])(i, j, k) +
                    srcU(i, j, k) * (*matrixA[9])(i, j, k) +
                    srcU(i, j - 1, k) * (*matrixA[10])(i, j, k) +
                    srcW(i, j, k + 1) * (*matrixA[11])(i, j, k) +
                    srcW(i, j - 1, k + 1) * (*matrixA[12])(i, j, k) +
                    srcW(i, j, k) * (*matrixA[13])(i, j, k) +
                    srcW(i, j - 1, k) * (*matrixA[14])(i, j, k);
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return src;
  }
  typedef Grid<Real> type2;
  inline const std::vector<Grid<Real> *> &getArg3()
  {
    return matrixA;
  }
  typedef std::vector<Grid<Real> *> type3;
  inline const std::vector<Grid<Real> *> &getArg4()
  {
    return vecRhs;
  }
  typedef std::vector<Grid<Real> *> type4;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMatrixViscosityV ", 3);
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
            op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  const Grid<Real> &src;
  const std::vector<Grid<Real> *> matrixA;
  const std::vector<Grid<Real> *> vecRhs;
};

struct ApplyMatrixViscosityW : public KernelBase {
  ApplyMatrixViscosityW(const FlagGrid &flags,
                        Grid<Real> &dst,
                        const Grid<Real> &src,
                        const std::vector<Grid<Real> *> matrixA,
                        const std::vector<Grid<Real> *> vecRhs)
      : KernelBase(&flags, 1), flags(flags), dst(dst), src(src), matrixA(matrixA), vecRhs(vecRhs)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 const Grid<Real> &src,
                 const std::vector<Grid<Real> *> matrixA,
                 const std::vector<Grid<Real> *> vecRhs) const
  {
    if (matrixA.size() != 15)
      errMsg("ConjugateGrad: Invalid A matrix in apply matrix step");
    Grid<Real> &A0 = *matrixA[0];
    Grid<Real> &Aplusi = *matrixA[1];
    Grid<Real> &Aplusj = *matrixA[2];
    Grid<Real> &Aplusk = *matrixA[3];
    Grid<Real> &Aminusi = *matrixA[4];
    Grid<Real> &Aminusj = *matrixA[5];
    Grid<Real> &Aminusk = *matrixA[6];

    if (vecRhs.size() != 2)
      errMsg("ConjugateGrad: Invalid rhs vector in apply matrix step");
    Grid<Real> &srcU = *vecRhs[0];
    Grid<Real> &srcV = *vecRhs[1];

    dst(i, j, k) = src(i, j, k) * A0(i, j, k) + src(i + 1, j, k) * Aplusi(i, j, k) +
                   src(i, j + 1, k) * Aplusj(i, j, k) + src(i, j, k + 1) * Aplusk(i, j, k) +
                   src(i - 1, j, k) * Aminusi(i, j, k) + src(i, j - 1, k) * Aminusj(i, j, k) +
                   src(i, j, k - 1) * Aminusk(i, j, k);

    dst(i, j, k) += srcU(i + 1, j, k) * (*matrixA[7])(i, j, k) +
                    srcU(i + 1, j, k - 1) * (*matrixA[8])(i, j, k) +
                    srcU(i, j, k) * (*matrixA[9])(i, j, k) +
                    srcU(i, j, k - 1) * (*matrixA[10])(i, j, k) +
                    srcV(i, j + 1, k) * (*matrixA[11])(i, j, k) +
                    srcV(i, j + 1, k - 1) * (*matrixA[12])(i, j, k) +
                    srcV(i, j, k) * (*matrixA[13])(i, j, k) +
                    srcV(i, j, k - 1) * (*matrixA[14])(i, j, k);
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return dst;
  }
  typedef Grid<Real> type1;
  inline const Grid<Real> &getArg2()
  {
    return src;
  }
  typedef Grid<Real> type2;
  inline const std::vector<Grid<Real> *> &getArg3()
  {
    return matrixA;
  }
  typedef std::vector<Grid<Real> *> type3;
  inline const std::vector<Grid<Real> *> &getArg4()
  {
    return vecRhs;
  }
  typedef std::vector<Grid<Real> *> type4;
  void runMessage()
  {
    debMsg("Executing kernel ApplyMatrixViscosityW ", 3);
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
            op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, dst, src, matrixA, vecRhs);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  const Grid<Real> &src;
  const std::vector<Grid<Real> *> matrixA;
  const std::vector<Grid<Real> *> vecRhs;
};

/* NOTE: Use this template for new matrix application kernels

//! Template for matrix application kernels
KERNEL()
void ApplyMatrixTemplate (const FlagGrid& flags, Grid<Real>& dst, const Grid<Real>& src,
  const std::vector<Grid<Real> *> matrixA, const std::vector<Grid<Real> *> vecRhs)
{
  // The kernel must define how to use the grids from the matrixA and vecRhs lists
}

*/

//! Kernel: Construct the matrix for the poisson equation

struct MakeLaplaceMatrix : public KernelBase {
  MakeLaplaceMatrix(const FlagGrid &flags,
                    Grid<Real> &A0,
                    Grid<Real> &Ai,
                    Grid<Real> &Aj,
                    Grid<Real> &Ak,
                    const MACGrid *fractions = 0)
      : KernelBase(&flags, 1), flags(flags), A0(A0), Ai(Ai), Aj(Aj), Ak(Ak), fractions(fractions)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &A0,
                 Grid<Real> &Ai,
                 Grid<Real> &Aj,
                 Grid<Real> &Ak,
                 const MACGrid *fractions = 0) const
  {
    if (!flags.isFluid(i, j, k))
      return;

    if (!fractions) {
      // diagonal, A0
      if (!flags.isObstacle(i - 1, j, k))
        A0(i, j, k) += 1.;
      if (!flags.isObstacle(i + 1, j, k))
        A0(i, j, k) += 1.;
      if (!flags.isObstacle(i, j - 1, k))
        A0(i, j, k) += 1.;
      if (!flags.isObstacle(i, j + 1, k))
        A0(i, j, k) += 1.;
      if (flags.is3D() && !flags.isObstacle(i, j, k - 1))
        A0(i, j, k) += 1.;
      if (flags.is3D() && !flags.isObstacle(i, j, k + 1))
        A0(i, j, k) += 1.;

      // off-diagonal entries
      if (flags.isFluid(i + 1, j, k))
        Ai(i, j, k) = -1.;
      if (flags.isFluid(i, j + 1, k))
        Aj(i, j, k) = -1.;
      if (flags.is3D() && flags.isFluid(i, j, k + 1))
        Ak(i, j, k) = -1.;
    }
    else {
      // diagonal
      A0(i, j, k) += fractions->get(i, j, k).x;
      A0(i, j, k) += fractions->get(i + 1, j, k).x;
      A0(i, j, k) += fractions->get(i, j, k).y;
      A0(i, j, k) += fractions->get(i, j + 1, k).y;
      if (flags.is3D())
        A0(i, j, k) += fractions->get(i, j, k).z;
      if (flags.is3D())
        A0(i, j, k) += fractions->get(i, j, k + 1).z;

      // off-diagonal entries
      if (flags.isFluid(i + 1, j, k))
        Ai(i, j, k) = -fractions->get(i + 1, j, k).x;
      if (flags.isFluid(i, j + 1, k))
        Aj(i, j, k) = -fractions->get(i, j + 1, k).y;
      if (flags.is3D() && flags.isFluid(i, j, k + 1))
        Ak(i, j, k) = -fractions->get(i, j, k + 1).z;
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return A0;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> &getArg2()
  {
    return Ai;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> &getArg3()
  {
    return Aj;
  }
  typedef Grid<Real> type3;
  inline Grid<Real> &getArg4()
  {
    return Ak;
  }
  typedef Grid<Real> type4;
  inline const MACGrid *getArg5()
  {
    return fractions;
  }
  typedef MACGrid type5;
  void runMessage()
  {
    debMsg("Executing kernel MakeLaplaceMatrix ", 3);
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
            op(i, j, k, flags, A0, Ai, Aj, Ak, fractions);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, A0, Ai, Aj, Ak, fractions);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &A0;
  Grid<Real> &Ai;
  Grid<Real> &Aj;
  Grid<Real> &Ak;
  const MACGrid *fractions;
};

}  // namespace Manta

#endif
