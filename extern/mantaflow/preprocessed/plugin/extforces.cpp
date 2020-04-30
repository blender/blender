

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL)
 * http://www.gnu.org/licenses
 *
 * Set boundary conditions, gravity
 *
 ******************************************************************************/

#include "vectorbase.h"
#include "grid.h"
#include "commonkernels.h"
#include "particle.h"

using namespace std;

namespace Manta {

//! add constant force between fl/fl and fl/em cells
struct KnApplyForceField : public KernelBase {
  KnApplyForceField(const FlagGrid &flags,
                    MACGrid &vel,
                    const Grid<Vec3> &force,
                    const Grid<Real> *include,
                    bool additive,
                    bool isMAC)
      : KernelBase(&flags, 1),
        flags(flags),
        vel(vel),
        force(force),
        include(include),
        additive(additive),
        isMAC(isMAC)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 MACGrid &vel,
                 const Grid<Vec3> &force,
                 const Grid<Real> *include,
                 bool additive,
                 bool isMAC) const
  {
    bool curFluid = flags.isFluid(i, j, k);
    bool curEmpty = flags.isEmpty(i, j, k);
    if (!curFluid && !curEmpty)
      return;
    if (include && ((*include)(i, j, k) > 0.))
      return;

    Real forceX = (isMAC) ? force(i, j, k).x : 0.5 * (force(i - 1, j, k).x + force(i, j, k).x);
    Real forceY = (isMAC) ? force(i, j, k).y : 0.5 * (force(i, j - 1, k).y + force(i, j, k).y);

    Real forceZ = 0.;
    if (vel.is3D())
      forceZ = (isMAC) ? force(i, j, k).z : 0.5 * (force(i, j, k - 1).z + force(i, j, k).z);

    if (flags.isFluid(i - 1, j, k) || (curFluid && flags.isEmpty(i - 1, j, k)))
      vel(i, j, k).x = (additive) ? vel(i, j, k).x + forceX : forceX;
    if (flags.isFluid(i, j - 1, k) || (curFluid && flags.isEmpty(i, j - 1, k)))
      vel(i, j, k).y = (additive) ? vel(i, j, k).y + forceY : forceY;
    if (vel.is3D() && (flags.isFluid(i, j, k - 1) || (curFluid && flags.isEmpty(i, j, k - 1))))
      vel(i, j, k).z = (additive) ? vel(i, j, k).z + forceZ : forceZ;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const Grid<Vec3> &getArg2()
  {
    return force;
  }
  typedef Grid<Vec3> type2;
  inline const Grid<Real> *getArg3()
  {
    return include;
  }
  typedef Grid<Real> type3;
  inline bool &getArg4()
  {
    return additive;
  }
  typedef bool type4;
  inline bool &getArg5()
  {
    return isMAC;
  }
  typedef bool type5;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyForceField ", 3);
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
            op(i, j, k, flags, vel, force, include, additive, isMAC);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, vel, force, include, additive, isMAC);
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
  MACGrid &vel;
  const Grid<Vec3> &force;
  const Grid<Real> *include;
  bool additive;
  bool isMAC;
};

//! add constant force between fl/fl and fl/em cells
struct KnApplyForce : public KernelBase {
  KnApplyForce(
      const FlagGrid &flags, MACGrid &vel, Vec3 force, const Grid<Real> *exclude, bool additive)
      : KernelBase(&flags, 1),
        flags(flags),
        vel(vel),
        force(force),
        exclude(exclude),
        additive(additive)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 MACGrid &vel,
                 Vec3 force,
                 const Grid<Real> *exclude,
                 bool additive) const
  {
    bool curFluid = flags.isFluid(i, j, k);
    bool curEmpty = flags.isEmpty(i, j, k);
    if (!curFluid && !curEmpty)
      return;
    if (exclude && ((*exclude)(i, j, k) < 0.))
      return;

    if (flags.isFluid(i - 1, j, k) || (curFluid && flags.isEmpty(i - 1, j, k)))
      vel(i, j, k).x = (additive) ? vel(i, j, k).x + force.x : force.x;
    if (flags.isFluid(i, j - 1, k) || (curFluid && flags.isEmpty(i, j - 1, k)))
      vel(i, j, k).y = (additive) ? vel(i, j, k).y + force.y : force.y;
    if (vel.is3D() && (flags.isFluid(i, j, k - 1) || (curFluid && flags.isEmpty(i, j, k - 1))))
      vel(i, j, k).z = (additive) ? vel(i, j, k).z + force.z : force.z;
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline Vec3 &getArg2()
  {
    return force;
  }
  typedef Vec3 type2;
  inline const Grid<Real> *getArg3()
  {
    return exclude;
  }
  typedef Grid<Real> type3;
  inline bool &getArg4()
  {
    return additive;
  }
  typedef bool type4;
  void runMessage()
  {
    debMsg("Executing kernel KnApplyForce ", 3);
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
            op(i, j, k, flags, vel, force, exclude, additive);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, vel, force, exclude, additive);
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
  MACGrid &vel;
  Vec3 force;
  const Grid<Real> *exclude;
  bool additive;
};

//! add gravity forces to all fluid cells, optionally  adapts to different grid sizes automatically
void addGravity(const FlagGrid &flags,
                MACGrid &vel,
                Vec3 gravity,
                const Grid<Real> *exclude = NULL,
                bool scale = true)
{
  float gridScale = (scale) ? flags.getDx() : 1;
  Vec3 f = gravity * flags.getParent()->getDt() / gridScale;
  KnApplyForce(flags, vel, f, exclude, true);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addGravity", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      Vec3 gravity = _args.get<Vec3>("gravity", 2, &_lock);
      const Grid<Real> *exclude = _args.getPtrOpt<Grid<Real>>("exclude", 3, NULL, &_lock);
      bool scale = _args.getOpt<bool>("scale", 4, true, &_lock);
      _retval = getPyNone();
      addGravity(flags, vel, gravity, exclude, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "addGravity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addGravity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addGravity("", "addGravity", _W_0);
extern "C" {
void PbRegister_addGravity()
{
  KEEP_UNUSED(_RP_addGravity);
}
}

//! Deprecated: use addGravity(scale=false) instead
void addGravityNoScale(const FlagGrid &flags,
                       MACGrid &vel,
                       const Vec3 &gravity,
                       const Grid<Real> *exclude = NULL)
{
  addGravity(flags, vel, gravity, exclude, false);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addGravityNoScale", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Vec3 &gravity = _args.get<Vec3>("gravity", 2, &_lock);
      const Grid<Real> *exclude = _args.getPtrOpt<Grid<Real>>("exclude", 3, NULL, &_lock);
      _retval = getPyNone();
      addGravityNoScale(flags, vel, gravity, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "addGravityNoScale", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addGravityNoScale", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addGravityNoScale("", "addGravityNoScale", _W_1);
extern "C" {
void PbRegister_addGravityNoScale()
{
  KEEP_UNUSED(_RP_addGravityNoScale);
}
}

//! kernel to add Buoyancy force
struct KnAddBuoyancy : public KernelBase {
  KnAddBuoyancy(const FlagGrid &flags, const Grid<Real> &factor, MACGrid &vel, Vec3 strength)
      : KernelBase(&flags, 1), flags(flags), factor(factor), vel(vel), strength(strength)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 const Grid<Real> &factor,
                 MACGrid &vel,
                 Vec3 strength) const
  {
    if (!flags.isFluid(i, j, k))
      return;
    if (flags.isFluid(i - 1, j, k))
      vel(i, j, k).x += (0.5 * strength.x) * (factor(i, j, k) + factor(i - 1, j, k));
    if (flags.isFluid(i, j - 1, k))
      vel(i, j, k).y += (0.5 * strength.y) * (factor(i, j, k) + factor(i, j - 1, k));
    if (vel.is3D() && flags.isFluid(i, j, k - 1))
      vel(i, j, k).z += (0.5 * strength.z) * (factor(i, j, k) + factor(i, j, k - 1));
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const Grid<Real> &getArg1()
  {
    return factor;
  }
  typedef Grid<Real> type1;
  inline MACGrid &getArg2()
  {
    return vel;
  }
  typedef MACGrid type2;
  inline Vec3 &getArg3()
  {
    return strength;
  }
  typedef Vec3 type3;
  void runMessage()
  {
    debMsg("Executing kernel KnAddBuoyancy ", 3);
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
            op(i, j, k, flags, factor, vel, strength);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, factor, vel, strength);
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
  const Grid<Real> &factor;
  MACGrid &vel;
  Vec3 strength;
};

//! add Buoyancy force based on factor (e.g. smoke density), optionally adapts to different grid
//! sizes automatically
void addBuoyancy(const FlagGrid &flags,
                 const Grid<Real> &density,
                 MACGrid &vel,
                 Vec3 gravity,
                 Real coefficient = 1.,
                 bool scale = true)
{
  float gridScale = (scale) ? flags.getDx() : 1;
  Vec3 f = -gravity * flags.getParent()->getDt() / gridScale * coefficient;
  KnAddBuoyancy(flags, density, vel, f);
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addBuoyancy", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 2, &_lock);
      Vec3 gravity = _args.get<Vec3>("gravity", 3, &_lock);
      Real coefficient = _args.getOpt<Real>("coefficient", 4, 1., &_lock);
      bool scale = _args.getOpt<bool>("scale", 5, true, &_lock);
      _retval = getPyNone();
      addBuoyancy(flags, density, vel, gravity, coefficient, scale);
      _args.check();
    }
    pbFinalizePlugin(parent, "addBuoyancy", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addBuoyancy", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addBuoyancy("", "addBuoyancy", _W_2);
extern "C" {
void PbRegister_addBuoyancy()
{
  KEEP_UNUSED(_RP_addBuoyancy);
}
}

// inflow / outflow boundaries

//! helper to parse openbounds string [xXyYzZ] , convert to vec3
inline void convertDescToVec(const string &desc, Vector3D<bool> &lo, Vector3D<bool> &up)
{
  for (size_t i = 0; i < desc.size(); i++) {
    if (desc[i] == 'x')
      lo.x = true;
    else if (desc[i] == 'y')
      lo.y = true;
    else if (desc[i] == 'z')
      lo.z = true;
    else if (desc[i] == 'X')
      up.x = true;
    else if (desc[i] == 'Y')
      up.y = true;
    else if (desc[i] == 'Z')
      up.z = true;
    else
      errMsg("invalid character in boundary description string. Only [xyzXYZ] allowed.");
  }
}

//! add empty and outflow flag to cells of open boundaries
void setOpenBound(FlagGrid &flags,
                  int bWidth,
                  string openBound = "",
                  int type = FlagGrid::TypeOutflow | FlagGrid::TypeEmpty)
{
  if (openBound == "")
    return;
  Vector3D<bool> lo, up;
  convertDescToVec(openBound, lo, up);

  FOR_IJK(flags)
  {
    bool loX = lo.x && i <= bWidth;  // a cell which belongs to the lower x open bound
    bool loY = lo.y && j <= bWidth;
    bool upX = up.x && i >= flags.getSizeX() - bWidth -
                                1;  // a cell which belongs to the upper x open bound
    bool upY = up.y && j >= flags.getSizeY() - bWidth - 1;
    bool innerI = i > bWidth &&
                  i < flags.getSizeX() - bWidth -
                          1;  // a cell which does not belong to the lower or upper x bound
    bool innerJ = j > bWidth && j < flags.getSizeY() - bWidth - 1;

    // when setting boundaries to open: don't set shared part of wall to empty if neighboring wall
    // is not open
    if ((!flags.is3D()) && (loX || upX || loY || upY)) {
      if ((loX || upX || innerI) && (loY || upY || innerJ) && flags.isObstacle(i, j, k))
        flags(i, j, k) = type;
    }
    else {
      bool loZ = lo.z && k <= bWidth;  // a cell which belongs to the lower z open bound
      bool upZ = up.z && k >= flags.getSizeZ() - bWidth -
                                  1;  // a cell which belongs to the upper z open bound
      bool innerK = k > bWidth &&
                    k < flags.getSizeZ() - bWidth -
                            1;  // a cell which does not belong to the lower or upper z bound
      if (loX || upX || loY || upY || loZ || upZ) {
        if ((loX || upX || innerI) && (loY || upY || innerJ) && (loZ || upZ || innerK) &&
            flags.isObstacle(i, j, k))
          flags(i, j, k) = type;
      }
    }
  }
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setOpenBound", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      int bWidth = _args.get<int>("bWidth", 1, &_lock);
      string openBound = _args.getOpt<string>("openBound", 2, "", &_lock);
      int type = _args.getOpt<int>("type", 3, FlagGrid::TypeOutflow | FlagGrid::TypeEmpty, &_lock);
      _retval = getPyNone();
      setOpenBound(flags, bWidth, openBound, type);
      _args.check();
    }
    pbFinalizePlugin(parent, "setOpenBound", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setOpenBound", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setOpenBound("", "setOpenBound", _W_3);
extern "C" {
void PbRegister_setOpenBound()
{
  KEEP_UNUSED(_RP_setOpenBound);
}
}

//! delete fluid and ensure empty flag in outflow cells, delete particles and density and set phi
//! to 0.5
void resetOutflow(FlagGrid &flags,
                  Grid<Real> *phi = 0,
                  BasicParticleSystem *parts = 0,
                  Grid<Real> *real = 0,
                  Grid<int> *index = 0,
                  ParticleIndexSystem *indexSys = 0)
{
  // check if phi and parts -> pindex and gpi already created -> access particles from cell index,
  // avoid extra looping over particles
  if (parts && (!index || !indexSys)) {
    if (phi)
      debMsg(
          "resetOpenBound for phi and particles, but missing index and indexSys for enhanced "
          "particle access!",
          1);
    for (int idx = 0; idx < (int)parts->size(); idx++)
      if (parts->isActive(idx) && flags.isInBounds(parts->getPos(idx)) &&
          flags.isOutflow(parts->getPos(idx)))
        parts->kill(idx);
  }
  FOR_IJK(flags)
  {
    if (flags.isOutflow(i, j, k)) {
      flags(i, j, k) = (flags(i, j, k) | FlagGrid::TypeEmpty) &
                       ~FlagGrid::TypeFluid;  // make sure there is not fluid flag set and to reset
                                              // the empty flag
      // the particles in a cell i,j,k are particles[index(i,j,k)] to particles[index(i+1,j,k)-1]
      if (parts && index && indexSys) {
        int isysIdxS = index->index(i, j, k);
        int pStart = (*index)(isysIdxS), pEnd = 0;
        if (flags.isInBounds(isysIdxS + 1))
          pEnd = (*index)(isysIdxS + 1);
        else
          pEnd = indexSys->size();
        // now loop over particles in cell
        for (int p = pStart; p < pEnd; ++p) {
          int psrc = (*indexSys)[p].sourceIndex;
          if (parts->isActive(psrc) && flags.isInBounds(parts->getPos(psrc)))
            parts->kill(psrc);
        }
      }
      if (phi)
        (*phi)(i, j, k) = 0.5;
      if (real)
        (*real)(i, j, k) = 0;
    }
  }
  if (parts)
    parts->doCompress();
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "resetOutflow", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> *phi = _args.getPtrOpt<Grid<Real>>("phi", 1, 0, &_lock);
      BasicParticleSystem *parts = _args.getPtrOpt<BasicParticleSystem>("parts", 2, 0, &_lock);
      Grid<Real> *real = _args.getPtrOpt<Grid<Real>>("real", 3, 0, &_lock);
      Grid<int> *index = _args.getPtrOpt<Grid<int>>("index", 4, 0, &_lock);
      ParticleIndexSystem *indexSys = _args.getPtrOpt<ParticleIndexSystem>(
          "indexSys", 5, 0, &_lock);
      _retval = getPyNone();
      resetOutflow(flags, phi, parts, real, index, indexSys);
      _args.check();
    }
    pbFinalizePlugin(parent, "resetOutflow", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("resetOutflow", e.what());
    return 0;
  }
}
static const Pb::Register _RP_resetOutflow("", "resetOutflow", _W_4);
extern "C" {
void PbRegister_resetOutflow()
{
  KEEP_UNUSED(_RP_resetOutflow);
}
}

//! enforce a constant inflow/outflow at the grid boundaries
struct KnSetInflow : public KernelBase {
  KnSetInflow(MACGrid &vel, int dim, int p0, const Vec3 &val)
      : KernelBase(&vel, 0), vel(vel), dim(dim), p0(p0), val(val)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &vel, int dim, int p0, const Vec3 &val) const
  {
    Vec3i p(i, j, k);
    if (p[dim] == p0 || p[dim] == p0 + 1)
      vel(i, j, k) = val;
  }
  inline MACGrid &getArg0()
  {
    return vel;
  }
  typedef MACGrid type0;
  inline int &getArg1()
  {
    return dim;
  }
  typedef int type1;
  inline int &getArg2()
  {
    return p0;
  }
  typedef int type2;
  inline const Vec3 &getArg3()
  {
    return val;
  }
  typedef Vec3 type3;
  void runMessage()
  {
    debMsg("Executing kernel KnSetInflow ", 3);
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
            op(i, j, k, vel, dim, p0, val);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, vel, dim, p0, val);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &vel;
  int dim;
  int p0;
  const Vec3 &val;
};

//! enforce a constant inflow/outflow at the grid boundaries
void setInflowBcs(MACGrid &vel, string dir, Vec3 value)
{
  for (size_t i = 0; i < dir.size(); i++) {
    if (dir[i] >= 'x' && dir[i] <= 'z') {
      int dim = dir[i] - 'x';
      KnSetInflow(vel, dim, 0, value);
    }
    else if (dir[i] >= 'X' && dir[i] <= 'Z') {
      int dim = dir[i] - 'X';
      KnSetInflow(vel, dim, vel.getSize()[dim] - 1, value);
    }
    else
      errMsg("invalid character in direction string. Only [xyzXYZ] allowed.");
  }
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setInflowBcs", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      string dir = _args.get<string>("dir", 1, &_lock);
      Vec3 value = _args.get<Vec3>("value", 2, &_lock);
      _retval = getPyNone();
      setInflowBcs(vel, dir, value);
      _args.check();
    }
    pbFinalizePlugin(parent, "setInflowBcs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setInflowBcs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setInflowBcs("", "setInflowBcs", _W_5);
extern "C" {
void PbRegister_setInflowBcs()
{
  KEEP_UNUSED(_RP_setInflowBcs);
}
}

// set obstacle boundary conditions

//! set no-stick wall boundary condition between ob/fl and ob/ob cells
struct KnSetWallBcs : public KernelBase {
  KnSetWallBcs(const FlagGrid &flags, MACGrid &vel, const MACGrid *obvel)
      : KernelBase(&flags, 0), flags(flags), vel(vel), obvel(obvel)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const FlagGrid &flags, MACGrid &vel, const MACGrid *obvel) const
  {

    bool curFluid = flags.isFluid(i, j, k);
    bool curObs = flags.isObstacle(i, j, k);
    Vec3 bcsVel(0., 0., 0.);
    if (!curFluid && !curObs)
      return;

    if (obvel) {
      bcsVel.x = (*obvel)(i, j, k).x;
      bcsVel.y = (*obvel)(i, j, k).y;
      if ((*obvel).is3D())
        bcsVel.z = (*obvel)(i, j, k).z;
    }

    // we use i>0 instead of bnd=1 to check outer wall
    if (i > 0 && flags.isObstacle(i - 1, j, k))
      vel(i, j, k).x = bcsVel.x;
    if (i > 0 && curObs && flags.isFluid(i - 1, j, k))
      vel(i, j, k).x = bcsVel.x;
    if (j > 0 && flags.isObstacle(i, j - 1, k))
      vel(i, j, k).y = bcsVel.y;
    if (j > 0 && curObs && flags.isFluid(i, j - 1, k))
      vel(i, j, k).y = bcsVel.y;

    if (!vel.is3D()) {
      vel(i, j, k).z = 0;
    }
    else {
      if (k > 0 && flags.isObstacle(i, j, k - 1))
        vel(i, j, k).z = bcsVel.z;
      if (k > 0 && curObs && flags.isFluid(i, j, k - 1))
        vel(i, j, k).z = bcsVel.z;
    }

    if (curFluid) {
      if ((i > 0 && flags.isStick(i - 1, j, k)) ||
          (i < flags.getSizeX() - 1 && flags.isStick(i + 1, j, k)))
        vel(i, j, k).y = vel(i, j, k).z = 0;
      if ((j > 0 && flags.isStick(i, j - 1, k)) ||
          (j < flags.getSizeY() - 1 && flags.isStick(i, j + 1, k)))
        vel(i, j, k).x = vel(i, j, k).z = 0;
      if (vel.is3D() && ((k > 0 && flags.isStick(i, j, k - 1)) ||
                         (k < flags.getSizeZ() - 1 && flags.isStick(i, j, k + 1))))
        vel(i, j, k).x = vel(i, j, k).y = 0;
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const MACGrid *getArg2()
  {
    return obvel;
  }
  typedef MACGrid type2;
  void runMessage()
  {
    debMsg("Executing kernel KnSetWallBcs ", 3);
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
            op(i, j, k, flags, vel, obvel);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, vel, obvel);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const FlagGrid &flags;
  MACGrid &vel;
  const MACGrid *obvel;
};

//! set wall BCs for fill fraction mode, note - only needs obstacle SDF

struct KnSetWallBcsFrac : public KernelBase {
  KnSetWallBcsFrac(const FlagGrid &flags,
                   const MACGrid &vel,
                   MACGrid &velTarget,
                   const MACGrid *obvel,
                   const Grid<Real> *phiObs,
                   const int &boundaryWidth = 0)
      : KernelBase(&flags, 0),
        flags(flags),
        vel(vel),
        velTarget(velTarget),
        obvel(obvel),
        phiObs(phiObs),
        boundaryWidth(boundaryWidth)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 const MACGrid &vel,
                 MACGrid &velTarget,
                 const MACGrid *obvel,
                 const Grid<Real> *phiObs,
                 const int &boundaryWidth = 0) const
  {
    bool curFluid = flags.isFluid(i, j, k);
    bool curObs = flags.isObstacle(i, j, k);
    velTarget(i, j, k) = vel(i, j, k);
    if (!curFluid && !curObs)
      return;

    // zero normal component in all obstacle regions
    if (flags.isInBounds(Vec3i(i, j, k), 1)) {

      if (curObs | flags.isObstacle(i - 1, j, k)) {
        Vec3 dphi(0., 0., 0.);
        const Real tmp1 = (phiObs->get(i, j, k) + phiObs->get(i - 1, j, k)) * .5;
        Real tmp2 = (phiObs->get(i, j + 1, k) + phiObs->get(i - 1, j + 1, k)) * .5;
        Real phi1 = (tmp1 + tmp2) * .5;
        tmp2 = (phiObs->get(i, j - 1, k) + phiObs->get(i - 1, j - 1, k)) * .5;
        Real phi2 = (tmp1 + tmp2) * .5;

        dphi.x = phiObs->get(i, j, k) - phiObs->get(i - 1, j, k);
        dphi.y = phi1 - phi2;

        if (phiObs->is3D()) {
          tmp2 = (phiObs->get(i, j, k + 1) + phiObs->get(i - 1, j, k + 1)) * .5;
          phi1 = (tmp1 + tmp2) * .5;
          tmp2 = (phiObs->get(i, j, k - 1) + phiObs->get(i - 1, j, k - 1)) * .5;
          phi2 = (tmp1 + tmp2) * .5;
          dphi.z = phi1 - phi2;
        }

        normalize(dphi);
        Vec3 velMAC = vel.getAtMACX(i, j, k);
        velTarget(i, j, k).x = velMAC.x - dot(dphi, velMAC) * dphi.x;
      }

      if (curObs | flags.isObstacle(i, j - 1, k)) {
        Vec3 dphi(0., 0., 0.);
        const Real tmp1 = (phiObs->get(i, j, k) + phiObs->get(i, j - 1, k)) * .5;
        Real tmp2 = (phiObs->get(i + 1, j, k) + phiObs->get(i + 1, j - 1, k)) * .5;
        Real phi1 = (tmp1 + tmp2) * .5;
        tmp2 = (phiObs->get(i - 1, j, k) + phiObs->get(i - 1, j - 1, k)) * .5;
        Real phi2 = (tmp1 + tmp2) * .5;

        dphi.x = phi1 - phi2;
        dphi.y = phiObs->get(i, j, k) - phiObs->get(i, j - 1, k);
        if (phiObs->is3D()) {
          tmp2 = (phiObs->get(i, j, k + 1) + phiObs->get(i, j - 1, k + 1)) * .5;
          phi1 = (tmp1 + tmp2) * .5;
          tmp2 = (phiObs->get(i, j, k - 1) + phiObs->get(i, j - 1, k - 1)) * .5;
          phi2 = (tmp1 + tmp2) * .5;
          dphi.z = phi1 - phi2;
        }

        normalize(dphi);
        Vec3 velMAC = vel.getAtMACY(i, j, k);
        velTarget(i, j, k).y = velMAC.y - dot(dphi, velMAC) * dphi.y;
      }

      if (phiObs->is3D() && (curObs | flags.isObstacle(i, j, k - 1))) {
        Vec3 dphi(0., 0., 0.);
        const Real tmp1 = (phiObs->get(i, j, k) + phiObs->get(i, j, k - 1)) * .5;

        Real tmp2;
        tmp2 = (phiObs->get(i + 1, j, k) + phiObs->get(i + 1, j, k - 1)) * .5;
        Real phi1 = (tmp1 + tmp2) * .5;
        tmp2 = (phiObs->get(i - 1, j, k) + phiObs->get(i - 1, j, k - 1)) * .5;
        Real phi2 = (tmp1 + tmp2) * .5;
        dphi.x = phi1 - phi2;

        tmp2 = (phiObs->get(i, j + 1, k) + phiObs->get(i, j + 1, k - 1)) * .5;
        phi1 = (tmp1 + tmp2) * .5;
        tmp2 = (phiObs->get(i, j - 1, k) + phiObs->get(i, j - 1, k - 1)) * .5;
        phi2 = (tmp1 + tmp2) * .5;
        dphi.y = phi1 - phi2;

        dphi.z = phiObs->get(i, j, k) - phiObs->get(i, j, k - 1);

        normalize(dphi);
        Vec3 velMAC = vel.getAtMACZ(i, j, k);
        velTarget(i, j, k).z = velMAC.z - dot(dphi, velMAC) * dphi.z;
      }
    }  // not at boundary
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline MACGrid &getArg2()
  {
    return velTarget;
  }
  typedef MACGrid type2;
  inline const MACGrid *getArg3()
  {
    return obvel;
  }
  typedef MACGrid type3;
  inline const Grid<Real> *getArg4()
  {
    return phiObs;
  }
  typedef Grid<Real> type4;
  inline const int &getArg5()
  {
    return boundaryWidth;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnSetWallBcsFrac ", 3);
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
            op(i, j, k, flags, vel, velTarget, obvel, phiObs, boundaryWidth);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, vel, velTarget, obvel, phiObs, boundaryWidth);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const FlagGrid &flags;
  const MACGrid &vel;
  MACGrid &velTarget;
  const MACGrid *obvel;
  const Grid<Real> *phiObs;
  const int &boundaryWidth;
};

//! set zero normal velocity boundary condition on walls
// (optionally with second order accuracy using the obstacle SDF , fractions grid currentlyl not
// needed)
void setWallBcs(const FlagGrid &flags,
                MACGrid &vel,
                const MACGrid *obvel = 0,
                const MACGrid *fractions = 0,
                const Grid<Real> *phiObs = 0,
                int boundaryWidth = 0)
{
  if (!phiObs || !fractions) {
    KnSetWallBcs(flags, vel, obvel);
  }
  else {
    MACGrid tmpvel(vel.getParent());
    KnSetWallBcsFrac(flags, vel, tmpvel, obvel, phiObs, boundaryWidth);
    vel.swap(tmpvel);
  }
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setWallBcs", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const MACGrid *obvel = _args.getPtrOpt<MACGrid>("obvel", 2, 0, &_lock);
      const MACGrid *fractions = _args.getPtrOpt<MACGrid>("fractions", 3, 0, &_lock);
      const Grid<Real> *phiObs = _args.getPtrOpt<Grid<Real>>("phiObs", 4, 0, &_lock);
      int boundaryWidth = _args.getOpt<int>("boundaryWidth", 5, 0, &_lock);
      _retval = getPyNone();
      setWallBcs(flags, vel, obvel, fractions, phiObs, boundaryWidth);
      _args.check();
    }
    pbFinalizePlugin(parent, "setWallBcs", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setWallBcs", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setWallBcs("", "setWallBcs", _W_6);
extern "C" {
void PbRegister_setWallBcs()
{
  KEEP_UNUSED(_RP_setWallBcs);
}
}

//! add Forces between fl/fl and fl/em cells (interpolate cell centered forces to MAC grid)
struct KnAddForceIfLower : public KernelBase {
  KnAddForceIfLower(const FlagGrid &flags, MACGrid &vel, const Grid<Vec3> &force)
      : KernelBase(&flags, 1), flags(flags), vel(vel), force(force)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, const FlagGrid &flags, MACGrid &vel, const Grid<Vec3> &force) const
  {
    bool curFluid = flags.isFluid(i, j, k);
    bool curEmpty = flags.isEmpty(i, j, k);
    if (!curFluid && !curEmpty)
      return;

    if (flags.isFluid(i - 1, j, k) || (curFluid && flags.isEmpty(i - 1, j, k))) {
      Real forceMACX = 0.5 * (force(i - 1, j, k).x + force(i, j, k).x);
      Real min = std::min(vel(i, j, k).x, forceMACX);
      Real max = std::max(vel(i, j, k).x, forceMACX);
      Real sum = vel(i, j, k).x + forceMACX;
      vel(i, j, k).x = (forceMACX > 0) ? std::min(sum, max) : std::max(sum, min);
    }
    if (flags.isFluid(i, j - 1, k) || (curFluid && flags.isEmpty(i, j - 1, k))) {
      Real forceMACY = 0.5 * (force(i, j - 1, k).y + force(i, j, k).y);
      Real min = std::min(vel(i, j, k).y, forceMACY);
      Real max = std::max(vel(i, j, k).y, forceMACY);
      Real sum = vel(i, j, k).y + forceMACY;
      vel(i, j, k).y = (forceMACY > 0) ? std::min(sum, max) : std::max(sum, min);
    }
    if (vel.is3D() && (flags.isFluid(i, j, k - 1) || (curFluid && flags.isEmpty(i, j, k - 1)))) {
      Real forceMACZ = 0.5 * (force(i, j, k - 1).z + force(i, j, k).z);
      Real min = std::min(vel(i, j, k).z, forceMACZ);
      Real max = std::max(vel(i, j, k).z, forceMACZ);
      Real sum = vel(i, j, k).z + forceMACZ;
      vel(i, j, k).z = (forceMACZ > 0) ? std::min(sum, max) : std::max(sum, min);
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const Grid<Vec3> &getArg2()
  {
    return force;
  }
  typedef Grid<Vec3> type2;
  void runMessage()
  {
    debMsg("Executing kernel KnAddForceIfLower ", 3);
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
            op(i, j, k, flags, vel, force);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, vel, force);
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
  MACGrid &vel;
  const Grid<Vec3> &force;
};

// Initial velocity for smoke
void setInitialVelocity(const FlagGrid &flags, MACGrid &vel, const Grid<Vec3> &invel)
{
  KnAddForceIfLower(flags, vel, invel);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setInitialVelocity", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Grid<Vec3> &invel = *_args.getPtr<Grid<Vec3>>("invel", 2, &_lock);
      _retval = getPyNone();
      setInitialVelocity(flags, vel, invel);
      _args.check();
    }
    pbFinalizePlugin(parent, "setInitialVelocity", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setInitialVelocity", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setInitialVelocity("", "setInitialVelocity", _W_7);
extern "C" {
void PbRegister_setInitialVelocity()
{
  KEEP_UNUSED(_RP_setInitialVelocity);
}
}

//! Kernel: gradient norm operator
struct KnConfForce : public KernelBase {
  KnConfForce(Grid<Vec3> &force,
              const Grid<Real> &grid,
              const Grid<Vec3> &curl,
              Real str,
              const Grid<Real> *strGrid)
      : KernelBase(&force, 1), force(force), grid(grid), curl(curl), str(str), strGrid(strGrid)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Vec3> &force,
                 const Grid<Real> &grid,
                 const Grid<Vec3> &curl,
                 Real str,
                 const Grid<Real> *strGrid) const
  {
    Vec3 grad = 0.5 * Vec3(grid(i + 1, j, k) - grid(i - 1, j, k),
                           grid(i, j + 1, k) - grid(i, j - 1, k),
                           0.);
    if (grid.is3D())
      grad[2] = 0.5 * (grid(i, j, k + 1) - grid(i, j, k - 1));
    normalize(grad);
    if (strGrid)
      str += (*strGrid)(i, j, k);
    force(i, j, k) = str * cross(grad, curl(i, j, k));
  }
  inline Grid<Vec3> &getArg0()
  {
    return force;
  }
  typedef Grid<Vec3> type0;
  inline const Grid<Real> &getArg1()
  {
    return grid;
  }
  typedef Grid<Real> type1;
  inline const Grid<Vec3> &getArg2()
  {
    return curl;
  }
  typedef Grid<Vec3> type2;
  inline Real &getArg3()
  {
    return str;
  }
  typedef Real type3;
  inline const Grid<Real> *getArg4()
  {
    return strGrid;
  }
  typedef Grid<Real> type4;
  void runMessage()
  {
    debMsg("Executing kernel KnConfForce ", 3);
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
            op(i, j, k, force, grid, curl, str, strGrid);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, force, grid, curl, str, strGrid);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Vec3> &force;
  const Grid<Real> &grid;
  const Grid<Vec3> &curl;
  Real str;
  const Grid<Real> *strGrid;
};

void vorticityConfinement(MACGrid &vel,
                          const FlagGrid &flags,
                          Real strength = 0,
                          const Grid<Real> *strengthCell = NULL)
{
  Grid<Vec3> velCenter(flags.getParent()), curl(flags.getParent()), force(flags.getParent());
  Grid<Real> norm(flags.getParent());

  GetCentered(velCenter, vel);
  CurlOp(velCenter, curl);
  GridNorm(norm, curl);
  KnConfForce(force, norm, curl, strength, strengthCell);
  KnApplyForceField(flags, vel, force, NULL, true, false);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "vorticityConfinement", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      Real strength = _args.getOpt<Real>("strength", 2, 0, &_lock);
      const Grid<Real> *strengthCell = _args.getPtrOpt<Grid<Real>>(
          "strengthCell", 3, NULL, &_lock);
      _retval = getPyNone();
      vorticityConfinement(vel, flags, strength, strengthCell);
      _args.check();
    }
    pbFinalizePlugin(parent, "vorticityConfinement", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("vorticityConfinement", e.what());
    return 0;
  }
}
static const Pb::Register _RP_vorticityConfinement("", "vorticityConfinement", _W_8);
extern "C" {
void PbRegister_vorticityConfinement()
{
  KEEP_UNUSED(_RP_vorticityConfinement);
}
}

void addForceField(const FlagGrid &flags,
                   MACGrid &vel,
                   const Grid<Vec3> &force,
                   const Grid<Real> *region = NULL,
                   bool isMAC = false)
{
  KnApplyForceField(flags, vel, force, region, true, isMAC);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addForceField", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Grid<Vec3> &force = *_args.getPtr<Grid<Vec3>>("force", 2, &_lock);
      const Grid<Real> *region = _args.getPtrOpt<Grid<Real>>("region", 3, NULL, &_lock);
      bool isMAC = _args.getOpt<bool>("isMAC", 4, false, &_lock);
      _retval = getPyNone();
      addForceField(flags, vel, force, region, isMAC);
      _args.check();
    }
    pbFinalizePlugin(parent, "addForceField", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addForceField", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addForceField("", "addForceField", _W_9);
extern "C" {
void PbRegister_addForceField()
{
  KEEP_UNUSED(_RP_addForceField);
}
}

void setForceField(const FlagGrid &flags,
                   MACGrid &vel,
                   const Grid<Vec3> &force,
                   const Grid<Real> *region = NULL,
                   bool isMAC = false)
{
  KnApplyForceField(flags, vel, force, region, false, isMAC);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setForceField", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const Grid<Vec3> &force = *_args.getPtr<Grid<Vec3>>("force", 2, &_lock);
      const Grid<Real> *region = _args.getPtrOpt<Grid<Real>>("region", 3, NULL, &_lock);
      bool isMAC = _args.getOpt<bool>("isMAC", 4, false, &_lock);
      _retval = getPyNone();
      setForceField(flags, vel, force, region, isMAC);
      _args.check();
    }
    pbFinalizePlugin(parent, "setForceField", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setForceField", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setForceField("", "setForceField", _W_10);
extern "C" {
void PbRegister_setForceField()
{
  KEEP_UNUSED(_RP_setForceField);
}
}

struct KnDissolveSmoke : public KernelBase {
  KnDissolveSmoke(const FlagGrid &flags,
                  Grid<Real> &density,
                  Grid<Real> *heat,
                  Grid<Real> *red,
                  Grid<Real> *green,
                  Grid<Real> *blue,
                  int speed,
                  bool logFalloff,
                  float dydx,
                  float fac)
      : KernelBase(&flags, 0),
        flags(flags),
        density(density),
        heat(heat),
        red(red),
        green(green),
        blue(blue),
        speed(speed),
        logFalloff(logFalloff),
        dydx(dydx),
        fac(fac)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &density,
                 Grid<Real> *heat,
                 Grid<Real> *red,
                 Grid<Real> *green,
                 Grid<Real> *blue,
                 int speed,
                 bool logFalloff,
                 float dydx,
                 float fac) const
  {

    bool curFluid = flags.isFluid(i, j, k);
    if (!curFluid)
      return;

    if (logFalloff) {
      density(i, j, k) *= fac;
      if (heat) {
        (*heat)(i, j, k) *= fac;
      }
      if (red) {
        (*red)(i, j, k) *= fac;
        (*green)(i, j, k) *= fac;
        (*blue)(i, j, k) *= fac;
      }
    }
    else {  // linear falloff
      float d = density(i, j, k);
      density(i, j, k) -= dydx;
      if (density(i, j, k) < 0.0f)
        density(i, j, k) = 0.0f;
      if (heat) {
        if (fabs((*heat)(i, j, k)) < dydx)
          (*heat)(i, j, k) = 0.0f;
        else if ((*heat)(i, j, k) > 0.0f)
          (*heat)(i, j, k) -= dydx;
        else if ((*heat)(i, j, k) < 0.0f)
          (*heat)(i, j, k) += dydx;
      }
      if (red && notZero(d)) {
        (*red)(i, j, k) *= (density(i, j, k) / d);
        (*green)(i, j, k) *= (density(i, j, k) / d);
        (*blue)(i, j, k) *= (density(i, j, k) / d);
      }
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return density;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> *getArg2()
  {
    return heat;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> *getArg3()
  {
    return red;
  }
  typedef Grid<Real> type3;
  inline Grid<Real> *getArg4()
  {
    return green;
  }
  typedef Grid<Real> type4;
  inline Grid<Real> *getArg5()
  {
    return blue;
  }
  typedef Grid<Real> type5;
  inline int &getArg6()
  {
    return speed;
  }
  typedef int type6;
  inline bool &getArg7()
  {
    return logFalloff;
  }
  typedef bool type7;
  inline float &getArg8()
  {
    return dydx;
  }
  typedef float type8;
  inline float &getArg9()
  {
    return fac;
  }
  typedef float type9;
  void runMessage()
  {
    debMsg("Executing kernel KnDissolveSmoke ", 3);
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
            op(i, j, k, flags, density, heat, red, green, blue, speed, logFalloff, dydx, fac);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, flags, density, heat, red, green, blue, speed, logFalloff, dydx, fac);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const FlagGrid &flags;
  Grid<Real> &density;
  Grid<Real> *heat;
  Grid<Real> *red;
  Grid<Real> *green;
  Grid<Real> *blue;
  int speed;
  bool logFalloff;
  float dydx;
  float fac;
};

void dissolveSmoke(const FlagGrid &flags,
                   Grid<Real> &density,
                   Grid<Real> *heat = NULL,
                   Grid<Real> *red = NULL,
                   Grid<Real> *green = NULL,
                   Grid<Real> *blue = NULL,
                   int speed = 5,
                   bool logFalloff = true)
{
  float dydx = 1.0f / (float)speed;  // max density/speed = dydx
  float fac = 1.0f - dydx;
  KnDissolveSmoke(flags, density, heat, red, green, blue, speed, logFalloff, dydx, fac);
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "dissolveSmoke", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &density = *_args.getPtr<Grid<Real>>("density", 1, &_lock);
      Grid<Real> *heat = _args.getPtrOpt<Grid<Real>>("heat", 2, NULL, &_lock);
      Grid<Real> *red = _args.getPtrOpt<Grid<Real>>("red", 3, NULL, &_lock);
      Grid<Real> *green = _args.getPtrOpt<Grid<Real>>("green", 4, NULL, &_lock);
      Grid<Real> *blue = _args.getPtrOpt<Grid<Real>>("blue", 5, NULL, &_lock);
      int speed = _args.getOpt<int>("speed", 6, 5, &_lock);
      bool logFalloff = _args.getOpt<bool>("logFalloff", 7, true, &_lock);
      _retval = getPyNone();
      dissolveSmoke(flags, density, heat, red, green, blue, speed, logFalloff);
      _args.check();
    }
    pbFinalizePlugin(parent, "dissolveSmoke", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("dissolveSmoke", e.what());
    return 0;
  }
}
static const Pb::Register _RP_dissolveSmoke("", "dissolveSmoke", _W_11);
extern "C" {
void PbRegister_dissolveSmoke()
{
  KEEP_UNUSED(_RP_dissolveSmoke);
}
}

}  // namespace Manta
