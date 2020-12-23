

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
 * Conjugate gradient solver, for pressure and viscosity
 *
 ******************************************************************************/

#include "conjugategrad.h"
#include "commonkernels.h"

using namespace std;
namespace Manta {

const int CG_DEBUGLEVEL = 3;

//*****************************************************************************
//  Precondition helpers

//! Preconditioning a la Wavelet Turbulence (needs 4 add. grids)
void InitPreconditionIncompCholesky(const FlagGrid &flags,
                                    Grid<Real> &A0,
                                    Grid<Real> &Ai,
                                    Grid<Real> &Aj,
                                    Grid<Real> &Ak,
                                    Grid<Real> &orgA0,
                                    Grid<Real> &orgAi,
                                    Grid<Real> &orgAj,
                                    Grid<Real> &orgAk)
{
  // compute IC according to Golub and Van Loan
  A0.copyFrom(orgA0);
  Ai.copyFrom(orgAi);
  Aj.copyFrom(orgAj);
  Ak.copyFrom(orgAk);

  FOR_IJK(A0)
  {
    if (flags.isFluid(i, j, k)) {
      const IndexInt idx = A0.index(i, j, k);
      A0[idx] = sqrt(A0[idx]);

      // correct left and top stencil in other entries
      // for i = k+1:n
      //  if (A(i,k) != 0)
      //    A(i,k) = A(i,k) / A(k,k)
      Real invDiagonal = 1.0f / A0[idx];
      Ai[idx] *= invDiagonal;
      Aj[idx] *= invDiagonal;
      Ak[idx] *= invDiagonal;

      // correct the right and bottom stencil in other entries
      // for j = k+1:n
      //   for i = j:n
      //      if (A(i,j) != 0)
      //        A(i,j) = A(i,j) - A(i,k) * A(j,k)
      A0(i + 1, j, k) -= square(Ai[idx]);
      A0(i, j + 1, k) -= square(Aj[idx]);
      A0(i, j, k + 1) -= square(Ak[idx]);
    }
  }

  // invert A0 for faster computation later
  InvertCheckFluid(flags, A0);
};

//! Preconditioning using modified IC ala Bridson (needs 1 add. grid)
void InitPreconditionModifiedIncompCholesky2(const FlagGrid &flags,
                                             Grid<Real> &Aprecond,
                                             Grid<Real> &A0,
                                             Grid<Real> &Ai,
                                             Grid<Real> &Aj,
                                             Grid<Real> &Ak)
{
  // compute IC according to Golub and Van Loan
  Aprecond.clear();

  FOR_IJK(flags)
  {
    if (!flags.isFluid(i, j, k))
      continue;

    const Real tau = 0.97;
    const Real sigma = 0.25;

    // compute modified incomplete cholesky
    Real e = 0.;
    e = A0(i, j, k) - square(Ai(i - 1, j, k) * Aprecond(i - 1, j, k)) -
        square(Aj(i, j - 1, k) * Aprecond(i, j - 1, k)) -
        square(Ak(i, j, k - 1) * Aprecond(i, j, k - 1));
    e -= tau *
         (Ai(i - 1, j, k) * (Aj(i - 1, j, k) + Ak(i - 1, j, k)) * square(Aprecond(i - 1, j, k)) +
          Aj(i, j - 1, k) * (Ai(i, j - 1, k) + Ak(i, j - 1, k)) * square(Aprecond(i, j - 1, k)) +
          Ak(i, j, k - 1) * (Ai(i, j, k - 1) + Aj(i, j, k - 1)) * square(Aprecond(i, j, k - 1)) +
          0.);

    // stability cutoff
    if (e < sigma * A0(i, j, k))
      e = A0(i, j, k);

    Aprecond(i, j, k) = 1. / sqrt(e);
  }
};

//! Preconditioning using multigrid ala Dick et al.
void InitPreconditionMultigrid(
    GridMg *MG, Grid<Real> &A0, Grid<Real> &Ai, Grid<Real> &Aj, Grid<Real> &Ak, Real mAccuracy)
{
  // build multigrid hierarchy if necessary
  if (!MG->isASet())
    MG->setA(&A0, &Ai, &Aj, &Ak);
  MG->setCoarsestLevelAccuracy(mAccuracy * 1E-4);
  MG->setSmoothing(1, 1);
};

//! Apply WT-style ICP
void ApplyPreconditionIncompCholesky(Grid<Real> &dst,
                                     Grid<Real> &Var1,
                                     const FlagGrid &flags,
                                     Grid<Real> &A0,
                                     Grid<Real> &Ai,
                                     Grid<Real> &Aj,
                                     Grid<Real> &Ak,
                                     Grid<Real> &orgA0,
                                     Grid<Real> &orgAi,
                                     Grid<Real> &orgAj,
                                     Grid<Real> &orgAk)
{

  // forward substitution
  FOR_IJK(dst)
  {
    if (!flags.isFluid(i, j, k))
      continue;
    dst(i, j, k) = A0(i, j, k) *
                   (Var1(i, j, k) - dst(i - 1, j, k) * Ai(i - 1, j, k) -
                    dst(i, j - 1, k) * Aj(i, j - 1, k) - dst(i, j, k - 1) * Ak(i, j, k - 1));
  }

  // backward substitution
  FOR_IJK_REVERSE(dst)
  {
    const IndexInt idx = A0.index(i, j, k);
    if (!flags.isFluid(idx))
      continue;
    dst[idx] = A0[idx] * (dst[idx] - dst(i + 1, j, k) * Ai[idx] - dst(i, j + 1, k) * Aj[idx] -
                          dst(i, j, k + 1) * Ak[idx]);
  }
}

//! Apply Bridson-style mICP
void ApplyPreconditionModifiedIncompCholesky2(Grid<Real> &dst,
                                              Grid<Real> &Var1,
                                              const FlagGrid &flags,
                                              Grid<Real> &Aprecond,
                                              Grid<Real> &A0,
                                              Grid<Real> &Ai,
                                              Grid<Real> &Aj,
                                              Grid<Real> &Ak)
{
  // forward substitution
  FOR_IJK(dst)
  {
    if (!flags.isFluid(i, j, k))
      continue;
    const Real p = Aprecond(i, j, k);
    dst(i, j, k) = p *
                   (Var1(i, j, k) - dst(i - 1, j, k) * Ai(i - 1, j, k) * Aprecond(i - 1, j, k) -
                    dst(i, j - 1, k) * Aj(i, j - 1, k) * Aprecond(i, j - 1, k) -
                    dst(i, j, k - 1) * Ak(i, j, k - 1) * Aprecond(i, j, k - 1));
  }

  // backward substitution
  FOR_IJK_REVERSE(dst)
  {
    const IndexInt idx = A0.index(i, j, k);
    if (!flags.isFluid(idx))
      continue;
    const Real p = Aprecond[idx];
    dst[idx] = p * (dst[idx] - dst(i + 1, j, k) * Ai[idx] * p - dst(i, j + 1, k) * Aj[idx] * p -
                    dst(i, j, k + 1) * Ak[idx] * p);
  }
}

//! Perform one Multigrid VCycle
void ApplyPreconditionMultigrid(GridMg *pMG, Grid<Real> &dst, Grid<Real> &Var1)
{
  // one VCycle on "A*dst = Var1" with initial guess dst=0
  pMG->setRhs(Var1);
  pMG->doVCycle(dst);
}

//*****************************************************************************
// Kernels

//! Kernel: Compute the dot product between two Real grids
/*! Uses double precision internally */

struct GridDotProduct : public KernelBase {
  GridDotProduct(const Grid<Real> &a, const Grid<Real> &b)
      : KernelBase(&a, 0), a(a), b(b), result(0.0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const Grid<Real> &a, const Grid<Real> &b, double &result)
  {
    result += (a[idx] * b[idx]);
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
  inline const Grid<Real> &getArg1()
  {
    return b;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel GridDotProduct ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, a, b, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  GridDotProduct(GridDotProduct &o, tbb::split) : KernelBase(o), a(o.a), b(o.b), result(0.0)
  {
  }
  void join(const GridDotProduct &o)
  {
    result += o.result;
  }
  const Grid<Real> &a;
  const Grid<Real> &b;
  double result;
};
;

//! Kernel: compute residual (init) and add to sigma

struct InitSigma : public KernelBase {
  InitSigma(const FlagGrid &flags, Grid<Real> &dst, Grid<Real> &rhs, Grid<Real> &temp)
      : KernelBase(&flags, 0), flags(flags), dst(dst), rhs(rhs), temp(temp), sigma(0)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const FlagGrid &flags,
                 Grid<Real> &dst,
                 Grid<Real> &rhs,
                 Grid<Real> &temp,
                 double &sigma)
  {
    const double res = rhs[idx] - temp[idx];
    dst[idx] = (Real)res;

    // only compute residual in fluid region
    if (flags.isFluid(idx))
      sigma += res * res;
  }
  inline operator double()
  {
    return sigma;
  }
  inline double &getRet()
  {
    return sigma;
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
  inline Grid<Real> &getArg2()
  {
    return rhs;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> &getArg3()
  {
    return temp;
  }
  typedef Grid<Real> type3;
  void runMessage()
  {
    debMsg("Executing kernel InitSigma ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, dst, rhs, temp, sigma);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  InitSigma(InitSigma &o, tbb::split)
      : KernelBase(o), flags(o.flags), dst(o.dst), rhs(o.rhs), temp(o.temp), sigma(0)
  {
  }
  void join(const InitSigma &o)
  {
    sigma += o.sigma;
  }
  const FlagGrid &flags;
  Grid<Real> &dst;
  Grid<Real> &rhs;
  Grid<Real> &temp;
  double sigma;
};
;

//! Kernel: update search vector

struct UpdateSearchVec : public KernelBase {
  UpdateSearchVec(Grid<Real> &dst, Grid<Real> &src, Real factor)
      : KernelBase(&dst, 0), dst(dst), src(src), factor(factor)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Real> &dst, Grid<Real> &src, Real factor) const
  {
    dst[idx] = src[idx] + factor * dst[idx];
  }
  inline Grid<Real> &getArg0()
  {
    return dst;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return src;
  }
  typedef Grid<Real> type1;
  inline Real &getArg2()
  {
    return factor;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel UpdateSearchVec ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, dst, src, factor);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &dst;
  Grid<Real> &src;
  Real factor;
};

//*****************************************************************************
// CG class

template<class APPLYMAT>
GridCg<APPLYMAT>::GridCg(Grid<Real> &dst,
                         Grid<Real> &rhs,
                         Grid<Real> &residual,
                         Grid<Real> &search,
                         const FlagGrid &flags,
                         Grid<Real> &tmp,
                         std::vector<Grid<Real> *> matrixAVec,
                         std::vector<Grid<Real> *> rhsVec)
    : GridCgInterface(),
      mInited(false),
      mIterations(0),
      mDst(dst),
      mRhs(rhs),
      mResidual(residual),
      mSearch(search),
      mFlags(flags),
      mTmp(tmp),
      mMatrixA(matrixAVec),
      mVecRhs(rhsVec),
      mPcMethod(PC_None),
      mpPCA0(nullptr),
      mpPCAi(nullptr),
      mpPCAj(nullptr),
      mpPCAk(nullptr),
      mMG(nullptr),
      mSigma(0.),
      mAccuracy(VECTOR_EPSILON),
      mResNorm(1e20)
{
}

template<class APPLYMAT> void GridCg<APPLYMAT>::doInit()
{
  mInited = true;
  mIterations = 0;

  mDst.clear();
  mResidual.copyFrom(mRhs);  // p=0, residual = b

  if (mPcMethod == PC_ICP) {
    assertMsg(mDst.is3D(), "ICP only supports 3D grids so far");
    InitPreconditionIncompCholesky(mFlags,
                                   *mpPCA0,
                                   *mpPCAi,
                                   *mpPCAj,
                                   *mpPCAk,
                                   *mMatrixA[0],
                                   *mMatrixA[1],
                                   *mMatrixA[2],
                                   *mMatrixA[3]);
    ApplyPreconditionIncompCholesky(mTmp,
                                    mResidual,
                                    mFlags,
                                    *mpPCA0,
                                    *mpPCAi,
                                    *mpPCAj,
                                    *mpPCAk,
                                    *mMatrixA[0],
                                    *mMatrixA[1],
                                    *mMatrixA[2],
                                    *mMatrixA[3]);
  }
  else if (mPcMethod == PC_mICP) {
    assertMsg(mDst.is3D(), "mICP only supports 3D grids so far");
    InitPreconditionModifiedIncompCholesky2(
        mFlags, *mpPCA0, *mMatrixA[0], *mMatrixA[1], *mMatrixA[2], *mMatrixA[3]);
    ApplyPreconditionModifiedIncompCholesky2(
        mTmp, mResidual, mFlags, *mpPCA0, *mMatrixA[0], *mMatrixA[1], *mMatrixA[2], *mMatrixA[3]);
  }
  else if (mPcMethod == PC_MGP) {
    InitPreconditionMultigrid(
        mMG, *mMatrixA[0], *mMatrixA[1], *mMatrixA[2], *mMatrixA[3], mAccuracy);
    ApplyPreconditionMultigrid(mMG, mTmp, mResidual);
  }
  else {
    mTmp.copyFrom(mResidual);
  }

  mSearch.copyFrom(mTmp);
  mSigma = GridDotProduct(mTmp, mResidual);
}

template<class APPLYMAT> bool GridCg<APPLYMAT>::iterate()
{
  if (!mInited)
    doInit();

  mIterations++;

  // create matrix application operator passed as template argument,
  // this could reinterpret the mpA pointers (not so clean right now)
  // tmp = applyMat(search)

  APPLYMAT(mFlags, mTmp, mSearch, mMatrixA, mVecRhs);

  // alpha = sigma/dot(tmp, search)
  Real dp = GridDotProduct(mTmp, mSearch);
  Real alpha = 0.;
  if (fabs(dp) > 0.)
    alpha = mSigma / (Real)dp;

  gridScaledAdd<Real, Real>(mDst, mSearch, alpha);     // dst += search * alpha
  gridScaledAdd<Real, Real>(mResidual, mTmp, -alpha);  // residual += tmp * -alpha

  if (mPcMethod == PC_ICP)
    ApplyPreconditionIncompCholesky(mTmp,
                                    mResidual,
                                    mFlags,
                                    *mpPCA0,
                                    *mpPCAi,
                                    *mpPCAj,
                                    *mpPCAk,
                                    *mMatrixA[0],
                                    *mMatrixA[1],
                                    *mMatrixA[2],
                                    *mMatrixA[3]);
  else if (mPcMethod == PC_mICP)
    ApplyPreconditionModifiedIncompCholesky2(
        mTmp, mResidual, mFlags, *mpPCA0, *mMatrixA[0], *mMatrixA[1], *mMatrixA[2], *mMatrixA[3]);
  else if (mPcMethod == PC_MGP)
    ApplyPreconditionMultigrid(mMG, mTmp, mResidual);
  else
    mTmp.copyFrom(mResidual);

  // use the l2 norm of the residual for convergence check? (usually max norm is recommended
  // instead)
  if (this->mUseL2Norm) {
    mResNorm = GridSumSqr(mResidual).sum;
  }
  else {
    mResNorm = mResidual.getMaxAbs();
  }

  // abort here to safe some work...
  if (mResNorm < mAccuracy) {
    mSigma = mResNorm;  // this will be returned later on to the caller...
    return false;
  }

  Real sigmaNew = GridDotProduct(mTmp, mResidual);
  Real beta = sigmaNew / mSigma;

  // search =  tmp + beta * search
  UpdateSearchVec(mSearch, mTmp, beta);

  debMsg("GridCg::iterate i=" << mIterations << " sigmaNew=" << sigmaNew << " sigmaLast=" << mSigma
                              << " alpha=" << alpha << " beta=" << beta << " ",
         CG_DEBUGLEVEL);
  mSigma = sigmaNew;

  if (!(mResNorm < 1e35)) {
    if (mPcMethod == PC_MGP) {
      // diverging solves can be caused by the static multigrid mode, we cannot detect this here,
      // though only the pressure solve call "knows" whether the MG is static or dynamics...
      debMsg(
          "GridCg::iterate: Warning - this diverging solve can be caused by the 'static' mode of "
          "the MG preconditioner. If the static mode is active, try switching to dynamic.",
          1);
    }
    errMsg("GridCg::iterate: The CG solver diverged, residual norm > 1e30, stopping.");
  }

  // debMsg("PB-CG-Norms::p"<<sqrt( GridOpNormNosqrt(mpDst, mpFlags).getValue() ) <<"
  // search"<<sqrt( GridOpNormNosqrt(mpSearch, mpFlags).getValue(), CG_DEBUGLEVEL ) <<" res"<<sqrt(
  // GridOpNormNosqrt(mpResidual, mpFlags).getValue() ) <<" tmp"<<sqrt( GridOpNormNosqrt(mpTmp,
  // mpFlags).getValue() ), CG_DEBUGLEVEL ); // debug
  return true;
}

template<class APPLYMAT> void GridCg<APPLYMAT>::solve(int maxIter)
{
  for (int iter = 0; iter < maxIter; iter++) {
    if (!iterate())
      iter = maxIter;
  }
  return;
}

static bool gPrint2dWarning = true;
template<class APPLYMAT>
void GridCg<APPLYMAT>::setICPreconditioner(
    PreconditionType method, Grid<Real> *A0, Grid<Real> *Ai, Grid<Real> *Aj, Grid<Real> *Ak)
{
  assertMsg(method == PC_ICP || method == PC_mICP,
            "GridCg<APPLYMAT>::setICPreconditioner: Invalid method specified.");

  mPcMethod = method;
  if ((!A0->is3D())) {
    if (gPrint2dWarning) {
      debMsg("ICP/mICP pre-conditioning only supported in 3D for now, disabling it.", 1);
      gPrint2dWarning = false;
    }
    mPcMethod = PC_None;
  }
  mpPCA0 = A0;
  mpPCAi = Ai;
  mpPCAj = Aj;
  mpPCAk = Ak;
}

template<class APPLYMAT>
void GridCg<APPLYMAT>::setMGPreconditioner(PreconditionType method, GridMg *MG)
{
  assertMsg(method == PC_MGP, "GridCg<APPLYMAT>::setMGPreconditioner: Invalid method specified.");

  mPcMethod = method;
  mMG = MG;
}

// explicit instantiation
template class GridCg<ApplyMatrix>;
template class GridCg<ApplyMatrix2D>;
template class GridCg<ApplyMatrixViscosityU>;
template class GridCg<ApplyMatrixViscosityV>;
template class GridCg<ApplyMatrixViscosityW>;

//*****************************************************************************
// diffusion for real and vec grids, e.g. for viscosity

//! do a CG solve for diffusion; note: diffusion coefficient alpha given in grid space,
//  rescale in python file for discretization independence (or physical correspondence)
//  see lidDrivenCavity.py for an example

void cgSolveDiffusion(const FlagGrid &flags,
                      GridBase &grid,
                      Real alpha = 0.25,
                      Real cgMaxIterFac = 1.0,
                      Real cgAccuracy = 1e-4)
{
  // reserve temp grids
  FluidSolver *parent = flags.getParent();
  Grid<Real> rhs(parent);
  Grid<Real> residual(parent), search(parent), tmp(parent);
  Grid<Real> A0(parent), Ai(parent), Aj(parent), Ak(parent);

  // setup matrix and boundaries
  FlagGrid flagsDummy(parent);
  flagsDummy.setConst(FlagGrid::TypeFluid);
  MakeLaplaceMatrix(flagsDummy, A0, Ai, Aj, Ak);

  FOR_IJK(flags)
  {
    if (flags.isObstacle(i, j, k)) {
      Ai(i, j, k) = Aj(i, j, k) = Ak(i, j, k) = 0.0;
      A0(i, j, k) = 1.0;
    }
    else {
      Ai(i, j, k) *= alpha;
      Aj(i, j, k) *= alpha;
      Ak(i, j, k) *= alpha;
      A0(i, j, k) *= alpha;
      A0(i, j, k) += 1.;
    }
  }

  GridCgInterface *gcg;
  // note , no preconditioning for now...
  const int maxIter = (int)(cgMaxIterFac * flags.getSize().max()) * (flags.is3D() ? 1 : 4);

  if (grid.getType() & GridBase::TypeReal) {
    Grid<Real> &u = ((Grid<Real> &)grid);
    rhs.copyFrom(u);
    vector<Grid<Real> *> matA{&A0, &Ai, &Aj};

    if (flags.is3D()) {
      matA.push_back(&Ak);
      gcg = new GridCg<ApplyMatrix>(u, rhs, residual, search, flags, tmp, matA);
    }
    else {
      gcg = new GridCg<ApplyMatrix2D>(u, rhs, residual, search, flags, tmp, matA);
    }

    gcg->setAccuracy(cgAccuracy);
    gcg->solve(maxIter);

    debMsg("FluidSolver::solveDiffusion iterations:" << gcg->getIterations()
                                                     << ", res:" << gcg->getSigma(),
           CG_DEBUGLEVEL);
  }
  else if ((grid.getType() & GridBase::TypeVec3) || (grid.getType() & GridBase::TypeMAC)) {
    Grid<Vec3> &vec = ((Grid<Vec3> &)grid);
    Grid<Real> u(parent);
    vector<Grid<Real> *> matA{&A0, &Ai, &Aj};

    // core solve is same as for a regular real grid
    if (flags.is3D()) {
      matA.push_back(&Ak);
      gcg = new GridCg<ApplyMatrix>(u, rhs, residual, search, flags, tmp, matA);
    }
    else {
      gcg = new GridCg<ApplyMatrix2D>(u, rhs, residual, search, flags, tmp, matA);
    }

    gcg->setAccuracy(cgAccuracy);

    // diffuse every component separately
    for (int component = 0; component < (grid.is3D() ? 3 : 2); ++component) {
      getComponent(vec, u, component);
      gcg->forceReinit();

      rhs.copyFrom(u);
      gcg->solve(maxIter);
      debMsg("FluidSolver::solveDiffusion vec3, iterations:" << gcg->getIterations()
                                                             << ", res:" << gcg->getSigma(),
             CG_DEBUGLEVEL);

      setComponent(u, vec, component);
    }
  }
  else {
    errMsg("cgSolveDiffusion: Grid Type is not supported (only Real, Vec3, MAC, or Levelset)");
  }

  delete gcg;
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "cgSolveDiffusion", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      GridBase &grid = *_args.getPtr<GridBase>("grid", 1, &_lock);
      Real alpha = _args.getOpt<Real>("alpha", 2, 0.25, &_lock);
      Real cgMaxIterFac = _args.getOpt<Real>("cgMaxIterFac", 3, 1.0, &_lock);
      Real cgAccuracy = _args.getOpt<Real>("cgAccuracy", 4, 1e-4, &_lock);
      _retval = getPyNone();
      cgSolveDiffusion(flags, grid, alpha, cgMaxIterFac, cgAccuracy);
      _args.check();
    }
    pbFinalizePlugin(parent, "cgSolveDiffusion", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("cgSolveDiffusion", e.what());
    return 0;
  }
}
static const Pb::Register _RP_cgSolveDiffusion("", "cgSolveDiffusion", _W_0);
extern "C" {
void PbRegister_cgSolveDiffusion()
{
  KEEP_UNUSED(_RP_cgSolveDiffusion);
}
}

};  // namespace Manta
