

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

#include "fluidsolver.h"
#include "grid.h"
#include <sstream>
#include <fstream>

using namespace std;
namespace Manta {

//******************************************************************************
// Gridstorage-related members

template<class T> void FluidSolver::GridStorage<T>::free()
{
  if (used != 0)
    errMsg("can't clean grid cache, some grids are still in use");
  for (size_t i = 0; i < grids.size(); i++)
    delete[] grids[i];
  grids.clear();
}
template<class T> T *FluidSolver::GridStorage<T>::get(Vec3i size)
{
  if ((int)grids.size() <= used) {
    debMsg("FluidSolver::GridStorage::get Allocating new " << size.x << "," << size.y << ","
                                                           << size.z << " ",
           3);
    grids.push_back(new T[(long long)(size.x) * size.y * size.z]);
  }
  if (used > 200)
    errMsg("too many temp grids used -- are they released properly ?");
  return grids[used++];
}
template<class T> void FluidSolver::GridStorage<T>::release(T *ptr)
{
  // rewrite pointer, as it may have changed due to swap operations
  used--;
  if (used < 0)
    errMsg("temp grid inconsistency");
  grids[used] = ptr;
}

template<> int *FluidSolver::getGridPointer<int>()
{
  return mGridsInt.get(mGridSize);
}
template<> Real *FluidSolver::getGridPointer<Real>()
{
  return mGridsReal.get(mGridSize);
}
template<> Vec3 *FluidSolver::getGridPointer<Vec3>()
{
  return mGridsVec.get(mGridSize);
}
template<> Vec4 *FluidSolver::getGridPointer<Vec4>()
{
  return mGridsVec4.get(mGridSize);
}
template<> void FluidSolver::freeGridPointer<int>(int *ptr)
{
  mGridsInt.release(ptr);
}
template<> void FluidSolver::freeGridPointer<Real>(Real *ptr)
{
  mGridsReal.release(ptr);
}
template<> void FluidSolver::freeGridPointer<Vec3>(Vec3 *ptr)
{
  mGridsVec.release(ptr);
}
template<> void FluidSolver::freeGridPointer<Vec4>(Vec4 *ptr)
{
  mGridsVec4.release(ptr);
}

// 4d data (work around for now, convert to 1d length)

template<> int *FluidSolver::getGrid4dPointer<int>()
{
  return mGrids4dInt.get(Vec3i(mGridSize[0] * mGridSize[1], mGridSize[2], mFourthDim));
}
template<> Real *FluidSolver::getGrid4dPointer<Real>()
{
  return mGrids4dReal.get(Vec3i(mGridSize[0] * mGridSize[1], mGridSize[2], mFourthDim));
}
template<> Vec3 *FluidSolver::getGrid4dPointer<Vec3>()
{
  return mGrids4dVec.get(Vec3i(mGridSize[0] * mGridSize[1], mGridSize[2], mFourthDim));
}
template<> Vec4 *FluidSolver::getGrid4dPointer<Vec4>()
{
  return mGrids4dVec4.get(Vec3i(mGridSize[0] * mGridSize[1], mGridSize[2], mFourthDim));
}
template<> void FluidSolver::freeGrid4dPointer<int>(int *ptr)
{
  mGrids4dInt.release(ptr);
}
template<> void FluidSolver::freeGrid4dPointer<Real>(Real *ptr)
{
  mGrids4dReal.release(ptr);
}
template<> void FluidSolver::freeGrid4dPointer<Vec3>(Vec3 *ptr)
{
  mGrids4dVec.release(ptr);
}
template<> void FluidSolver::freeGrid4dPointer<Vec4>(Vec4 *ptr)
{
  mGrids4dVec4.release(ptr);
}

//******************************************************************************
// FluidSolver members

FluidSolver::FluidSolver(Vec3i gridsize, int dim, int fourthDim)
    : PbClass(this),
      mDt(1.0),
      mTimeTotal(0.),
      mFrame(0),
      mCflCond(1000),
      mDtMin(1.),
      mDtMax(1.),
      mFrameLength(1.),
      mTimePerFrame(0.),
      mGridSize(gridsize),
      mDim(dim),
      mLockDt(false),
      mFourthDim(fourthDim)
{
  if (dim == 4 && mFourthDim > 0)
    errMsg("Don't create 4D solvers, use 3D with fourth-dim parameter >0 instead.");
  assertMsg(dim == 2 || dim == 3, "Only 2D and 3D solvers allowed.");
  assertMsg(dim != 2 || gridsize.z == 1, "Trying to create 2D solver with size.z != 1");
}

FluidSolver::~FluidSolver()
{
  mGridsInt.free();
  mGridsReal.free();
  mGridsVec.free();
  mGridsVec4.free();

  mGrids4dInt.free();
  mGrids4dReal.free();
  mGrids4dVec.free();
  mGrids4dVec4.free();
}

PbClass *FluidSolver::create(PbType t, PbTypeVec T, const string &name)
{
#if NOPYTHON != 1
  _args.add("nocheck", true);
  if (t.str() == "")
    errMsg(
        "Need to specify object type. Use e.g. Solver.create(FlagGrid, ...) or "
        "Solver.create(type=FlagGrid, ...)");

  PbClass *ret = PbClass::createPyObject(t.str() + T.str(), name, _args, this);
#else
  PbClass *ret = nullptr;
#endif
  return ret;
}

void FluidSolver::step()
{
  // update simulation time with adaptive time stepping
  // (use eps value to prevent roundoff errors)
  mTimePerFrame += mDt;
  mTimeTotal += mDt;

  if ((mTimePerFrame + VECTOR_EPSILON) > mFrameLength) {
    mFrame++;

    // re-calc total time, prevent drift...
    mTimeTotal = (double)mFrame * mFrameLength;
    mTimePerFrame = 0.;
    mLockDt = false;
  }

  updateQtGui(true, mFrame, mTimeTotal, "FluidSolver::step");
}

void FluidSolver::printMemInfo()
{
  std::ostringstream msg;
  msg << "Allocated grids: int " << mGridsInt.used << "/" << mGridsInt.grids.size() << ", ";
  msg << "                 real " << mGridsReal.used << "/" << mGridsReal.grids.size() << ", ";
  msg << "                 vec3 " << mGridsVec.used << "/" << mGridsVec.grids.size() << ". ";
  msg << "                 vec4 " << mGridsVec4.used << "/" << mGridsVec4.grids.size() << ". ";
  if (supports4D()) {
    msg << "Allocated 4d grids: int " << mGrids4dInt.used << "/" << mGrids4dInt.grids.size()
        << ", ";
    msg << "                    real " << mGrids4dReal.used << "/" << mGrids4dReal.grids.size()
        << ", ";
    msg << "                    vec3 " << mGrids4dVec.used << "/" << mGrids4dVec.grids.size()
        << ". ";
    msg << "                    vec4 " << mGrids4dVec4.used << "/" << mGrids4dVec4.grids.size()
        << ". ";
  }
  printf("%s\n", msg.str().c_str());
}

//! warning, uses 10^-4 epsilon values, thus only use around "regular" FPS time scales, e.g. 30
//! frames per time unit pass max magnitude of current velocity as maxvel, not yet scaled by dt!
void FluidSolver::adaptTimestep(Real maxVel)
{
  const Real mvt = maxVel * mDt;
  if (!mLockDt) {
    // calculate current timestep from maxvel, clamp range
    mDt = std::max(std::min(mDt * (Real)(mCflCond / (mvt + 1e-05)), mDtMax), mDtMin);
    if ((mTimePerFrame + mDt * 1.05) > mFrameLength) {
      // within 5% of full step? add epsilon to prevent roundoff errors...
      mDt = (mFrameLength - mTimePerFrame) + 1e-04;
    }
    else if ((mTimePerFrame + mDt + mDtMin) > mFrameLength ||
             (mTimePerFrame + (mDt * 1.25)) > mFrameLength) {
      // avoid tiny timesteps and strongly varying ones, do 2 medium size ones if necessary...
      mDt = (mFrameLength - mTimePerFrame + 1e-04) * 0.5;
      mLockDt = true;
    }
  }
  debMsg("Frame " << mFrame << ", max vel per step: " << mvt << " , dt: " << mDt << ", frame time "
                  << mTimePerFrame << "/" << mFrameLength << "; lock:" << mLockDt,
         2);

  // sanity check
  assertMsg((mDt > (mDtMin / 2.)), "Invalid dt encountered! Shouldnt happen...");
}

//******************************************************************************
// Generic helpers (no PYTHON funcs in general.cpp, thus they're here...)

//! helper to unify printing from python scripts and printing internal messages (optionally pass
//! debug level to control amount of output)
void mantaMsg(const std::string &out, int level = 1)
{
  debMsg(out, level);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "mantaMsg", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const std::string &out = _args.get<std::string>("out", 0, &_lock);
      int level = _args.getOpt<int>("level", 1, 1, &_lock);
      _retval = getPyNone();
      mantaMsg(out, level);
      _args.check();
    }
    pbFinalizePlugin(parent, "mantaMsg", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("mantaMsg", e.what());
    return 0;
  }
}
static const Pb::Register _RP_mantaMsg("", "mantaMsg", _W_0);
extern "C" {
void PbRegister_mantaMsg()
{
  KEEP_UNUSED(_RP_mantaMsg);
}
}

std::string printBuildInfo()
{
  string infoString = buildInfoString();
  debMsg("Build info: " << infoString.c_str() << " ", 1);
  return infoString;
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "printBuildInfo", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      _retval = toPy(printBuildInfo());
      _args.check();
    }
    pbFinalizePlugin(parent, "printBuildInfo", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("printBuildInfo", e.what());
    return 0;
  }
}
static const Pb::Register _RP_printBuildInfo("", "printBuildInfo", _W_1);
extern "C" {
void PbRegister_printBuildInfo()
{
  KEEP_UNUSED(_RP_printBuildInfo);
}
}

//! set debug level for messages (0 off, 1 regular, higher = more, up to 10)
void setDebugLevel(int level = 1)
{
  gDebugLevel = level;
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setDebugLevel", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      int level = _args.getOpt<int>("level", 0, 1, &_lock);
      _retval = getPyNone();
      setDebugLevel(level);
      _args.check();
    }
    pbFinalizePlugin(parent, "setDebugLevel", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setDebugLevel", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setDebugLevel("", "setDebugLevel", _W_2);
extern "C" {
void PbRegister_setDebugLevel()
{
  KEEP_UNUSED(_RP_setDebugLevel);
}
}

//! helper function to check for numpy compilation
void assertNumpy()
{
#if NUMPY == 1
  // all good, nothing to do...
#else
  errMsg("This scene requires numpy support. Enable compilation in cmake with \"-DNUMPY=1\" ");
#endif
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "assertNumpy", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      _retval = getPyNone();
      assertNumpy();
      _args.check();
    }
    pbFinalizePlugin(parent, "assertNumpy", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("assertNumpy", e.what());
    return 0;
  }
}
static const Pb::Register _RP_assertNumpy("", "assertNumpy", _W_3);
extern "C" {
void PbRegister_assertNumpy()
{
  KEEP_UNUSED(_RP_assertNumpy);
}
}

}  // namespace Manta
