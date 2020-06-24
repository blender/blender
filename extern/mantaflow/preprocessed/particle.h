

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
 * Base class for particle systems
 *
 ******************************************************************************/

#ifndef _PARTICLE_H
#define _PARTICLE_H

#include <vector>
#include "grid.h"
#include "vectorbase.h"
#include "integrator.h"
#include "randomstream.h"
namespace Manta {

// fwd decl
template<class T> class Grid;
class ParticleDataBase;
template<class T> class ParticleDataImpl;

//! Baseclass for particle systems. Does not implement any data
class ParticleBase : public PbClass {
 public:
  enum SystemType { BASE = 0, PARTICLE, VORTEX, FILAMENT, FLIP, TURBULENCE, INDEX };

  enum ParticleStatus {
    PNONE = 0,
    PNEW = (1 << 0),    // particles newly created in this step
    PSPRAY = (1 << 1),  // secondary particle types
    PBUBBLE = (1 << 2),
    PFOAM = (1 << 3),
    PTRACER = (1 << 4),
    PDELETE = (1 << 10),   // mark as deleted, will be deleted in next compress() step
    PINVALID = (1 << 30),  // unused
  };

  ParticleBase(FluidSolver *parent);
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ParticleBase::ParticleBase", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ParticleBase(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "ParticleBase::ParticleBase", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ParticleBase::ParticleBase", e.what());
      return -1;
    }
  }

  virtual ~ParticleBase();

  //! copy all the particle data thats registered with the other particle system to this one
  virtual void cloneParticleData(ParticleBase *nm);

  virtual SystemType getType() const
  {
    return BASE;
  }
  virtual std::string infoString() const;
  virtual ParticleBase *clone()
  {
    assertMsg(false, "Dont use, override...");
    return NULL;
  }

  //! slow virtual function to query size, do not use in kernels! use size() instead
  virtual IndexInt getSizeSlow() const
  {
    assertMsg(false, "Dont use, override...");
    return 0;
  }

  //! add a position as potential candidate for new particle (todo, make usable from parallel
  //! threads)
  inline void addBuffered(const Vec3 &pos, int flag = 0);

  //! particle data functions

  //! create a particle data object
  PbClass *create(PbType type, PbTypeVec T = PbTypeVec(), const std::string &name = "");
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleBase *pbo = dynamic_cast<ParticleBase *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleBase::create", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        PbType type = _args.get<PbType>("type", 0, &_lock);
        PbTypeVec T = _args.getOpt<PbTypeVec>("T", 1, PbTypeVec(), &_lock);
        const std::string &name = _args.getOpt<std::string>("name", 2, "", &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->create(type, T, name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleBase::create", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleBase::create", e.what());
      return 0;
    }
  }

  //! add a particle data field, set its parent particle-system pointer
  void registerPdata(ParticleDataBase *pdata);
  void registerPdataReal(ParticleDataImpl<Real> *pdata);
  void registerPdataVec3(ParticleDataImpl<Vec3> *pdata);
  void registerPdataInt(ParticleDataImpl<int> *pdata);
  //! remove a particle data entry
  void deregister(ParticleDataBase *pdata);
  //! add one zero entry to all data fields
  void addAllPdata();
  // note - deletion of pdata is handled in compress function

  //! how many are there?
  IndexInt getNumPdata() const
  {
    return mPartData.size();
  }
  //! access one of the fields
  ParticleDataBase *getPdata(int i)
  {
    return mPartData[i];
  }

 protected:
  //! new particle candidates
  std::vector<Vec3> mNewBufferPos;
  std::vector<int> mNewBufferFlag;

  //! allow automatic compression / resize? disallowed for, eg, flip particle systems
  bool mAllowCompress;

  //! store particle data , each pointer has its own storage vector of a certain type (int, real,
  //! vec3)
  std::vector<ParticleDataBase *> mPartData;
  //! lists of different types, for fast operations w/o virtual function calls (all calls necessary
  //! per particle)
  std::vector<ParticleDataImpl<Real> *> mPdataReal;
  std::vector<ParticleDataImpl<Vec3> *> mPdataVec3;
  std::vector<ParticleDataImpl<int> *>
      mPdataInt;  //! indicate that pdata of this particle system is copied, and needs to be freed
  bool mFreePdata;
 public:
  PbArgs _args;
}
#define _C_ParticleBase
;

//! Main class for particle systems
/*! Basetype S must at least contain flag, pos fields */
template<class S> class ParticleSystem : public ParticleBase {
 public:
  ParticleSystem(FluidSolver *parent) : ParticleBase(parent), mDeletes(0), mDeleteChunk(0)
  {
  }
  static int _W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ParticleSystem::ParticleSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ParticleSystem(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "ParticleSystem::ParticleSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::ParticleSystem", e.what());
      return -1;
    }
  }

  virtual ~ParticleSystem(){};

  virtual SystemType getType() const
  {
    return S::getType();
  };

  //! accessors
  inline S &operator[](IndexInt idx)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }
  inline const S &operator[](IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }
  //! return size of container
  //! note , python binding disabled for now! cannot yet deal with long-long types
  inline IndexInt size() const
  {
    return mData.size();
  }
  //! slow virtual function of base class, also returns size
  virtual IndexInt getSizeSlow() const
  {
    return size();
  }
  //! note , special call for python, note - doesnt support more than 2b parts!
  int pySize() const
  {
    return (int)mData.size();
  }
  static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::pySize", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->pySize());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::pySize", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::pySize", e.what());
      return 0;
    }
  }

  //! query status
  inline int getStatus(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx].flag;
  }
  inline bool isActive(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return (mData[idx].flag & PDELETE) == 0;
  }
  inline bool isSpray(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return (mData[idx].flag & PSPRAY);
  }
  inline bool isBubble(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return (mData[idx].flag & PBUBBLE);
  }
  inline bool isFoam(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return (mData[idx].flag & PFOAM);
  }
  inline bool isTracer(IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return (mData[idx].flag & PTRACER);
  }

  //! update status
  inline void setStatus(IndexInt idx, const int status)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    mData[idx].flag = status;
  }

  //! safe accessor for python
  void setPos(const IndexInt idx, const Vec3 &pos)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    mData[idx].pos = pos;
  }
  static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::setPos", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const IndexInt idx = _args.get<IndexInt>("idx", 0, &_lock);
        const Vec3 &pos = _args.get<Vec3>("pos", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setPos(idx, pos);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::setPos", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::setPos", e.what());
      return 0;
    }
  }

  const Vec3 &getPos(const IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx].pos;
  }
  static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::getPos", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const IndexInt idx = _args.get<IndexInt>("idx", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getPos(idx));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::getPos", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::getPos", e.what());
      return 0;
    }
  }

  //! copy all positions into pdata vec3 field
  void getPosPdata(ParticleDataImpl<Vec3> &target) const;
  static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::getPosPdata", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        ParticleDataImpl<Vec3> &target = *_args.getPtr<ParticleDataImpl<Vec3>>(
            "target", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->getPosPdata(target);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::getPosPdata", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::getPosPdata", e.what());
      return 0;
    }
  }

  void setPosPdata(const ParticleDataImpl<Vec3> &source);
  static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::setPosPdata", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<Vec3> &source = *_args.getPtr<ParticleDataImpl<Vec3>>(
            "source", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setPosPdata(source);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::setPosPdata", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::setPosPdata", e.what());
      return 0;
    }
  }

  //! transform coordinate system from one grid size to another (usually upon load)
  void transformPositions(Vec3i dimOld, Vec3i dimNew);

  //! explicitly trigger compression from outside
  void doCompress()
  {
    if (mDeletes > mDeleteChunk)
      compress();
  }
  //! insert buffered positions as new particles, update additional particle data
  void insertBufferedParticles();
  //! resize data vector, and all pdata fields
  void resizeAll(IndexInt newsize);

  //! adding and deleting
  inline void kill(IndexInt idx);
  IndexInt add(const S &data);
  //! remove all particles, init 0 length arrays (also pdata)
  void clear();
  static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::clear", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clear();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::clear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::clear", e.what());
      return 0;
    }
  }

  //! Advect particle in grid velocity field
  void advectInGrid(const FlagGrid &flags,
                    const MACGrid &vel,
                    const int integrationMode,
                    const bool deleteInObstacle = true,
                    const bool stopInObstacle = true,
                    const bool skipNew = false,
                    const ParticleDataImpl<int> *ptype = NULL,
                    const int exclude = 0);
  static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::advectInGrid", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
        const int integrationMode = _args.get<int>("integrationMode", 2, &_lock);
        const bool deleteInObstacle = _args.getOpt<bool>("deleteInObstacle", 3, true, &_lock);
        const bool stopInObstacle = _args.getOpt<bool>("stopInObstacle", 4, true, &_lock);
        const bool skipNew = _args.getOpt<bool>("skipNew", 5, false, &_lock);
        const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
            "ptype", 6, NULL, &_lock);
        const int exclude = _args.getOpt<int>("exclude", 7, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->advectInGrid(flags,
                          vel,
                          integrationMode,
                          deleteInObstacle,
                          stopInObstacle,
                          skipNew,
                          ptype,
                          exclude);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::advectInGrid", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::advectInGrid", e.what());
      return 0;
    }
  }

  //! Project particles outside obstacles
  void projectOutside(Grid<Vec3> &gradient);
  static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::projectOutside", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        Grid<Vec3> &gradient = *_args.getPtr<Grid<Vec3>>("gradient", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->projectOutside(gradient);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::projectOutside", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::projectOutside", e.what());
      return 0;
    }
  }

  void projectOutOfBnd(const FlagGrid &flags,
                       const Real bnd,
                       const std::string &plane = "xXyYzZ",
                       const ParticleDataImpl<int> *ptype = NULL,
                       const int exclude = 0);
  static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleSystem *pbo = dynamic_cast<ParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleSystem::projectOutOfBnd", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
        const Real bnd = _args.get<Real>("bnd", 1, &_lock);
        const std::string &plane = _args.getOpt<std::string>("plane", 2, "xXyYzZ", &_lock);
        const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
            "ptype", 3, NULL, &_lock);
        const int exclude = _args.getOpt<int>("exclude", 4, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->projectOutOfBnd(flags, bnd, plane, ptype, exclude);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleSystem::projectOutOfBnd", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleSystem::projectOutOfBnd", e.what());
      return 0;
    }
  }

  virtual ParticleBase *clone();
  virtual std::string infoString() const;

  //! debugging
  inline void checkPartIndex(IndexInt idx) const;

 protected:
  //! deletion count , and interval for re-compressing
  IndexInt mDeletes, mDeleteChunk;
  //! the particle data
  std::vector<S> mData;
  //! reduce storage , called by doCompress
  virtual void compress();
 public:
  PbArgs _args;
}
#define _C_ParticleSystem
;

//******************************************************************************

//! Simplest data class for particle systems
//! contains a position and an int flag; note that these are deprectated, and will at
//! some point be replaced by the more flexible pdata fields. For now manually copy with
//! getPosPdata / setPosPdata.
struct BasicParticleData {
 public:
  BasicParticleData() : pos(0.), flag(0)
  {
  }
  BasicParticleData(const Vec3 &p) : pos(p), flag(0)
  {
  }
  static ParticleBase::SystemType getType()
  {
    return ParticleBase::PARTICLE;
  }

  //! data (note, this size is currently hard coded for uni i/o)
  Vec3 pos;
  int flag;
};

class BasicParticleSystem : public ParticleSystem<BasicParticleData> {
 public:
  BasicParticleSystem(FluidSolver *parent);
  static int _W_12(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "BasicParticleSystem::BasicParticleSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new BasicParticleSystem(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "BasicParticleSystem::BasicParticleSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::BasicParticleSystem", e.what());
      return -1;
    }
  }

  //! file io
  int save(const std::string name);
  static PyObject *_W_13(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::save", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->save(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::save", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::save", e.what());
      return 0;
    }
  }

  int load(const std::string name);
  static PyObject *_W_14(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::load", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->load(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::load", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::load", e.what());
      return 0;
    }
  }

  //! save to text file
  void writeParticlesText(const std::string name) const;
  //! other output formats
  void writeParticlesRawPositionsGz(const std::string name) const;
  void writeParticlesRawVelocityGz(const std::string name) const;

  //! read from other particle system (with resize)
  void readParticles(BasicParticleSystem *from);
  static PyObject *_W_15(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::readParticles", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        BasicParticleSystem *from = _args.getPtr<BasicParticleSystem>("from", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->readParticles(from);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::readParticles", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::readParticles", e.what());
      return 0;
    }
  }

  //! add particles in python
  void addParticle(Vec3 pos)
  {
    add(BasicParticleData(pos));
  }
  static PyObject *_W_16(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::addParticle", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        Vec3 pos = _args.get<Vec3>("pos", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addParticle(pos);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::addParticle", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::addParticle", e.what());
      return 0;
    }
  }

  //! dangerous, get low level access - avoid usage, only used in vortex filament advection for now
  std::vector<BasicParticleData> &getData()
  {
    return mData;
  }

  void printParts(IndexInt start = -1, IndexInt stop = -1, bool printIndex = false);
  static PyObject *_W_17(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::printParts", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        IndexInt start = _args.getOpt<IndexInt>("start", 0, -1, &_lock);
        IndexInt stop = _args.getOpt<IndexInt>("stop", 1, -1, &_lock);
        bool printIndex = _args.getOpt<bool>("printIndex", 2, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->printParts(start, stop, printIndex);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::printParts", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::printParts", e.what());
      return 0;
    }
  }

  //! get data pointer of particle data
  std::string getDataPointer();
  static PyObject *_W_18(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      BasicParticleSystem *pbo = dynamic_cast<BasicParticleSystem *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "BasicParticleSystem::getDataPointer", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getDataPointer());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "BasicParticleSystem::getDataPointer", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("BasicParticleSystem::getDataPointer", e.what());
      return 0;
    }
  }

 public:
  PbArgs _args;
}
#define _C_BasicParticleSystem
;

//******************************************************************************

//! Index into other particle system
//  used for grid based neighborhood searches on generic particle systems (stores
//  only active particles, and reduces copied data)
//  note - pos & flag are disabled here, do not use!
struct ParticleIndexData {
 public:
  ParticleIndexData() : sourceIndex(0)
  {
  }
  static ParticleBase::SystemType getType()
  {
    return ParticleBase::INDEX;
  }

  IndexInt sourceIndex;  // index of this particle in the original particle system
  //! note - the following two are needed for template instantiation, but not used
  //! for the particle index system (use values from original one!)
  static Vec3 pos;  // do not use...
  static int flag;  // not needed usally
                    // Vec3 pos; // enable for debugging
};

class ParticleIndexSystem : public ParticleSystem<ParticleIndexData> {
 public:
  ParticleIndexSystem(FluidSolver *parent) : ParticleSystem<ParticleIndexData>(parent)
  {
  }
  static int _W_19(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ParticleIndexSystem::ParticleIndexSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ParticleIndexSystem(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "ParticleIndexSystem::ParticleIndexSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ParticleIndexSystem::ParticleIndexSystem", e.what());
      return -1;
    }
  };
  //! we only need a resize function...
  void resize(IndexInt size)
  {
    mData.resize(size);
  }
 public:
  PbArgs _args;
}
#define _C_ParticleIndexSystem
;

//******************************************************************************

//! Particle set with connectivity

template<class DATA, class CON> class ConnectedParticleSystem : public ParticleSystem<DATA> {
 public:
  ConnectedParticleSystem(FluidSolver *parent) : ParticleSystem<DATA>(parent)
  {
  }
  static int _W_20(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ConnectedParticleSystem::ConnectedParticleSystem", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ConnectedParticleSystem(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(
          obj->getParent(), "ConnectedParticleSystem::ConnectedParticleSystem", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ConnectedParticleSystem::ConnectedParticleSystem", e.what());
      return -1;
    }
  }

  //! accessors
  inline bool isSegActive(int i)
  {
    return (mSegments[i].flag & ParticleBase::PDELETE) == 0;
  }
  inline int segSize() const
  {
    return mSegments.size();
  }
  inline CON &seg(int i)
  {
    return mSegments[i];
  }
  inline const CON &seg(int i) const
  {
    return mSegments[i];
  }

  virtual ParticleBase *clone();

 protected:
  std::vector<CON> mSegments;
  virtual void compress();
 public:
  PbArgs _args;
}
#define _C_ConnectedParticleSystem
;

//******************************************************************************

//! abstract interface for particle data
class ParticleDataBase : public PbClass {
 public:
  ParticleDataBase(FluidSolver *parent);
  static int _W_21(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ParticleDataBase::ParticleDataBase", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ParticleDataBase(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "ParticleDataBase::ParticleDataBase", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataBase::ParticleDataBase", e.what());
      return -1;
    }
  }

  virtual ~ParticleDataBase();

  //! data type IDs, in line with those for grids
  enum PdataType { TypeNone = 0, TypeReal = 1, TypeInt = 2, TypeVec3 = 4 };

  //! interface functions, using assert instead of pure virtual for python compatibility
  virtual IndexInt getSizeSlow() const
  {
    assertMsg(false, "Dont use, override...");
    return 0;
  }
  virtual void addEntry()
  {
    assertMsg(false, "Dont use, override...");
    return;
  }
  virtual ParticleDataBase *clone()
  {
    assertMsg(false, "Dont use, override...");
    return NULL;
  }
  virtual PdataType getType() const
  {
    assertMsg(false, "Dont use, override...");
    return TypeNone;
  }
  virtual void resize(IndexInt size)
  {
    assertMsg(false, "Dont use, override...");
    return;
  }
  virtual void copyValueSlow(IndexInt from, IndexInt to)
  {
    assertMsg(false, "Dont use, override...");
    return;
  }

  //! set / get base pointer to parent particle system
  void setParticleSys(ParticleBase *set)
  {
    mpParticleSys = set;
  }
  ParticleBase *getParticleSys()
  {
    return mpParticleSys;
  }

  //! debugging
  inline void checkPartIndex(IndexInt idx) const;

 protected:
  ParticleBase *mpParticleSys;
 public:
  PbArgs _args;
}
#define _C_ParticleDataBase
;

//! abstract interface for particle data

template<class T> class ParticleDataImpl : public ParticleDataBase {
 public:
  ParticleDataImpl(FluidSolver *parent);
  static int _W_22(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "ParticleDataImpl::ParticleDataImpl", !noTiming);
      {
        ArgLocker _lock;
        FluidSolver *parent = _args.getPtr<FluidSolver>("parent", 0, &_lock);
        obj = new ParticleDataImpl(parent);
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "ParticleDataImpl::ParticleDataImpl", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::ParticleDataImpl", e.what());
      return -1;
    }
  }

  ParticleDataImpl(FluidSolver *parent, ParticleDataImpl<T> *other);
  virtual ~ParticleDataImpl();

  //! access data
  inline T &get(const IndexInt idx)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }
  inline const T &get(const IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }
  inline T &operator[](const IndexInt idx)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }
  inline const T &operator[](const IndexInt idx) const
  {
    DEBUG_ONLY(checkPartIndex(idx));
    return mData[idx];
  }

  //! set data
  inline void set(const IndexInt idx, T &val)
  {
    DEBUG_ONLY(checkPartIndex(idx));
    mData[idx] = val;
  }

  //! set all values to 0, note - different from particleSystem::clear! doesnt modify size of array
  //! (has to stay in sync with parent system)
  void clear();
  static PyObject *_W_23(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::clear", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clear();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::clear", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::clear", e.what());
      return 0;
    }
  }

  //! set grid from which to get data...
  void setSource(Grid<T> *grid, bool isMAC = false);
  static PyObject *_W_24(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::setSource", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        Grid<T> *grid = _args.getPtr<Grid<T>>("grid", 0, &_lock);
        bool isMAC = _args.getOpt<bool>("isMAC", 1, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setSource(grid, isMAC);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::setSource", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::setSource", e.what());
      return 0;
    }
  }

  //! particle data base interface
  virtual IndexInt getSizeSlow() const;
  virtual void addEntry();
  virtual ParticleDataBase *clone();
  virtual PdataType getType() const;
  virtual void resize(IndexInt s);
  virtual void copyValueSlow(IndexInt from, IndexInt to);

  IndexInt size() const
  {
    return mData.size();
  }

  //! fast inlined functions for per particle operations
  inline void copyValue(IndexInt from, IndexInt to)
  {
    get(to) = get(from);
  }
  void initNewValue(IndexInt idx, Vec3 pos);

  //! python interface (similar to grid data)
  ParticleDataImpl<T> &copyFrom(const ParticleDataImpl<T> &a);
  static PyObject *_W_25(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::copyFrom", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->copyFrom(a));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::copyFrom", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::copyFrom", e.what());
      return 0;
    }
  }

  void setConst(const T &s);
  static PyObject *_W_26(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::setConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const T &s = *_args.getPtr<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::setConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::setConst", e.what());
      return 0;
    }
  }

  void setConstRange(const T &s, const int begin, const int end);
  static PyObject *_W_27(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::setConstRange", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const T &s = *_args.getPtr<T>("s", 0, &_lock);
        const int begin = _args.get<int>("begin", 1, &_lock);
        const int end = _args.get<int>("end", 2, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConstRange(s, begin, end);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::setConstRange", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::setConstRange", e.what());
      return 0;
    }
  }

  void add(const ParticleDataImpl<T> &a);
  static PyObject *_W_28(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::add", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->add(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::add", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::add", e.what());
      return 0;
    }
  }

  void sub(const ParticleDataImpl<T> &a);
  static PyObject *_W_29(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::sub", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->sub(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::sub", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::sub", e.what());
      return 0;
    }
  }

  void addConst(const T &s);
  static PyObject *_W_30(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::addConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const T &s = *_args.getPtr<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::addConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::addConst", e.what());
      return 0;
    }
  }

  void addScaled(const ParticleDataImpl<T> &a, const T &factor);
  static PyObject *_W_31(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::addScaled", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        const T &factor = *_args.getPtr<T>("factor", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->addScaled(a, factor);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::addScaled", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::addScaled", e.what());
      return 0;
    }
  }

  void mult(const ParticleDataImpl<T> &a);
  static PyObject *_W_32(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::mult", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->mult(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::mult", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::mult", e.what());
      return 0;
    }
  }

  void multConst(const T &s);
  static PyObject *_W_33(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::multConst", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const T &s = *_args.getPtr<T>("s", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->multConst(s);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::multConst", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::multConst", e.what());
      return 0;
    }
  }

  void safeDiv(const ParticleDataImpl<T> &a);
  static PyObject *_W_34(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::safeDiv", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<T> &a = *_args.getPtr<ParticleDataImpl<T>>("a", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->safeDiv(a);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::safeDiv", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::safeDiv", e.what());
      return 0;
    }
  }

  void clamp(const Real vmin, const Real vmax);
  static PyObject *_W_35(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::clamp", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Real vmin = _args.get<Real>("vmin", 0, &_lock);
        const Real vmax = _args.get<Real>("vmax", 1, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clamp(vmin, vmax);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::clamp", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::clamp", e.what());
      return 0;
    }
  }

  void clampMin(const Real vmin);
  static PyObject *_W_36(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::clampMin", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Real vmin = _args.get<Real>("vmin", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clampMin(vmin);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::clampMin", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::clampMin", e.what());
      return 0;
    }
  }

  void clampMax(const Real vmax);
  static PyObject *_W_37(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::clampMax", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const Real vmax = _args.get<Real>("vmax", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->clampMax(vmax);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::clampMax", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::clampMax", e.what());
      return 0;
    }
  }

  Real getMaxAbs() const;
  static PyObject *_W_38(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::getMaxAbs", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMaxAbs());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::getMaxAbs", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::getMaxAbs", e.what());
      return 0;
    }
  }

  Real getMax() const;
  static PyObject *_W_39(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::getMax", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMax());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::getMax", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::getMax", e.what());
      return 0;
    }
  }

  Real getMin() const;
  static PyObject *_W_40(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::getMin", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getMin());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::getMin", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::getMin", e.what());
      return 0;
    }
  }

  T sum(const ParticleDataImpl<int> *t = NULL, const int itype = 0) const;
  static PyObject *_W_41(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::sum", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const ParticleDataImpl<int> *t = _args.getPtrOpt<ParticleDataImpl<int>>(
            "t", 0, NULL, &_lock);
        const int itype = _args.getOpt<int>("itype", 1, 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sum(t, itype));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::sum", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::sum", e.what());
      return 0;
    }
  }

  Real sumSquare() const;
  static PyObject *_W_42(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::sumSquare", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sumSquare());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::sumSquare", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::sumSquare", e.what());
      return 0;
    }
  }

  Real sumMagnitude() const;
  static PyObject *_W_43(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::sumMagnitude", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->sumMagnitude());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::sumMagnitude", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::sumMagnitude", e.what());
      return 0;
    }
  }

  //! special, set if int flag in t has "flag"
  void setConstIntFlag(const T &s, const ParticleDataImpl<int> &t, const int flag);
  static PyObject *_W_44(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::setConstIntFlag", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const T &s = *_args.getPtr<T>("s", 0, &_lock);
        const ParticleDataImpl<int> &t = *_args.getPtr<ParticleDataImpl<int>>("t", 1, &_lock);
        const int flag = _args.get<int>("flag", 2, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->setConstIntFlag(s, t, flag);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::setConstIntFlag", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::setConstIntFlag", e.what());
      return 0;
    }
  }

  void printPdata(IndexInt start = -1, IndexInt stop = -1, bool printIndex = false);
  static PyObject *_W_45(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::printPdata", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        IndexInt start = _args.getOpt<IndexInt>("start", 0, -1, &_lock);
        IndexInt stop = _args.getOpt<IndexInt>("stop", 1, -1, &_lock);
        bool printIndex = _args.getOpt<bool>("printIndex", 2, false, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->printPdata(start, stop, printIndex);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::printPdata", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::printPdata", e.what());
      return 0;
    }
  }

  //! file io
  int save(const std::string name);
  static PyObject *_W_46(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::save", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->save(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::save", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::save", e.what());
      return 0;
    }
  }

  int load(const std::string name);
  static PyObject *_W_47(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::load", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        const std::string name = _args.get<std::string>("name", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = toPy(pbo->load(name));
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::load", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::load", e.what());
      return 0;
    }
  }

  //! get data pointer of particle data
  std::string getDataPointer();
  static PyObject *_W_48(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      ParticleDataImpl *pbo = dynamic_cast<ParticleDataImpl *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "ParticleDataImpl::getDataPointer", !noTiming);
      PyObject *_retval = 0;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = toPy(pbo->getDataPointer());
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "ParticleDataImpl::getDataPointer", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("ParticleDataImpl::getDataPointer", e.what());
      return 0;
    }
  }

 protected:
  //! data storage
  std::vector<T> mData;

  //! optionally , we might have an associated grid from which to grab new data
  Grid<T> *mpGridSource;  //! unfortunately , we need to distinguish mac vs regular vec3
  bool mGridSourceMAC;
 public:
  PbArgs _args;
}
#define _C_ParticleDataImpl
;

//******************************************************************************
// Implementation
//******************************************************************************

const int DELETE_PART = 20;  // chunk size for compression

void ParticleBase::addBuffered(const Vec3 &pos, int flag)
{
  mNewBufferPos.push_back(pos);
  mNewBufferFlag.push_back(flag);
}

template<class S> void ParticleSystem<S>::clear()
{
  mDeleteChunk = mDeletes = 0;
  this->resizeAll(0);  // instead of mData.clear
}

template<class S> IndexInt ParticleSystem<S>::add(const S &data)
{
  mData.push_back(data);
  mDeleteChunk = mData.size() / DELETE_PART;
  this->addAllPdata();
  return mData.size() - 1;
}

template<class S> inline void ParticleSystem<S>::kill(IndexInt idx)
{
  assertMsg(idx >= 0 && idx < size(), "Index out of bounds");
  mData[idx].flag |= PDELETE;
  if ((++mDeletes > mDeleteChunk) && (mAllowCompress))
    compress();
}

template<class S> void ParticleSystem<S>::getPosPdata(ParticleDataImpl<Vec3> &target) const
{
  for (IndexInt i = 0; i < (IndexInt)this->size(); ++i) {
    target[i] = this->getPos(i);
  }
}
template<class S> void ParticleSystem<S>::setPosPdata(const ParticleDataImpl<Vec3> &source)
{
  for (IndexInt i = 0; i < (IndexInt)this->size(); ++i) {
    this->setPos(i, source[i]);
  }
}

template<class S> void ParticleSystem<S>::transformPositions(Vec3i dimOld, Vec3i dimNew)
{
  const Vec3 factor = calcGridSizeFactor(dimNew, dimOld);
  for (IndexInt i = 0; i < (IndexInt)this->size(); ++i) {
    this->setPos(i, this->getPos(i) * factor);
  }
}

// check for deletion/invalid position, otherwise return velocity

template<class S> struct _GridAdvectKernel : public KernelBase {
  _GridAdvectKernel(const KernelBase &base,
                    std::vector<S> &p,
                    const MACGrid &vel,
                    const FlagGrid &flags,
                    const Real dt,
                    const bool deleteInObstacle,
                    const bool stopInObstacle,
                    const bool skipNew,
                    const ParticleDataImpl<int> *ptype,
                    const int exclude,
                    std::vector<Vec3> &u)
      : KernelBase(base),
        p(p),
        vel(vel),
        flags(flags),
        dt(dt),
        deleteInObstacle(deleteInObstacle),
        stopInObstacle(stopInObstacle),
        skipNew(skipNew),
        ptype(ptype),
        exclude(exclude),
        u(u)
  {
  }
  inline void op(IndexInt idx,
                 std::vector<S> &p,
                 const MACGrid &vel,
                 const FlagGrid &flags,
                 const Real dt,
                 const bool deleteInObstacle,
                 const bool stopInObstacle,
                 const bool skipNew,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude,
                 std::vector<Vec3> &u) const
  {
    if ((p[idx].flag & ParticleBase::PDELETE) || (ptype && ((*ptype)[idx] & exclude)) ||
        (skipNew && (p[idx].flag & ParticleBase::PNEW))) {
      u[idx] = 0.;
      return;
    }
    // special handling
    if (deleteInObstacle || stopInObstacle) {
      if (!flags.isInBounds(p[idx].pos, 1) || flags.isObstacle(p[idx].pos)) {
        if (stopInObstacle)
          u[idx] = 0.;
        // for simple tracer particles, its convenient to delete particles right away
        // for other sim types, eg flip, we can try to fix positions later on
        if (deleteInObstacle)
          p[idx].flag |= ParticleBase::PDELETE;
        return;
      }
    }
    u[idx] = vel.getInterpolated(p[idx].pos) * dt;
  }
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, vel, flags, dt, deleteInObstacle, stopInObstacle, skipNew, ptype, exclude, u);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<S> &p;
  const MACGrid &vel;
  const FlagGrid &flags;
  const Real dt;
  const bool deleteInObstacle;
  const bool stopInObstacle;
  const bool skipNew;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
  std::vector<Vec3> &u;
};
template<class S> struct GridAdvectKernel : public KernelBase {
  GridAdvectKernel(std::vector<S> &p,
                   const MACGrid &vel,
                   const FlagGrid &flags,
                   const Real dt,
                   const bool deleteInObstacle,
                   const bool stopInObstacle,
                   const bool skipNew,
                   const ParticleDataImpl<int> *ptype,
                   const int exclude)
      : KernelBase(p.size()),
        _inner(KernelBase(p.size()),
               p,
               vel,
               flags,
               dt,
               deleteInObstacle,
               stopInObstacle,
               skipNew,
               ptype,
               exclude,
               u),
        p(p),
        vel(vel),
        flags(flags),
        dt(dt),
        deleteInObstacle(deleteInObstacle),
        stopInObstacle(stopInObstacle),
        skipNew(skipNew),
        ptype(ptype),
        exclude(exclude),
        u((size))
  {
    runMessage();
    run();
  }
  void run()
  {
    _inner.run();
  }
  inline operator std::vector<Vec3>()
  {
    return u;
  }
  inline std::vector<Vec3> &getRet()
  {
    return u;
  }
  inline std::vector<S> &getArg0()
  {
    return p;
  }
  typedef std::vector<S> type0;
  inline const MACGrid &getArg1()
  {
    return vel;
  }
  typedef MACGrid type1;
  inline const FlagGrid &getArg2()
  {
    return flags;
  }
  typedef FlagGrid type2;
  inline const Real &getArg3()
  {
    return dt;
  }
  typedef Real type3;
  inline const bool &getArg4()
  {
    return deleteInObstacle;
  }
  typedef bool type4;
  inline const bool &getArg5()
  {
    return stopInObstacle;
  }
  typedef bool type5;
  inline const bool &getArg6()
  {
    return skipNew;
  }
  typedef bool type6;
  inline const ParticleDataImpl<int> *getArg7()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type7;
  inline const int &getArg8()
  {
    return exclude;
  }
  typedef int type8;
  void runMessage()
  {
    debMsg("Executing kernel GridAdvectKernel ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  _GridAdvectKernel<S> _inner;
  std::vector<S> &p;
  const MACGrid &vel;
  const FlagGrid &flags;
  const Real dt;
  const bool deleteInObstacle;
  const bool stopInObstacle;
  const bool skipNew;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
  std::vector<Vec3> u;
};
;

// final check after advection to make sure particles haven't escaped
// (similar to particle advection kernel)

template<class S> struct KnDeleteInObstacle : public KernelBase {
  KnDeleteInObstacle(std::vector<S> &p, const FlagGrid &flags)
      : KernelBase(p.size()), p(p), flags(flags)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<S> &p, const FlagGrid &flags) const
  {
    if (p[idx].flag & ParticleBase::PDELETE)
      return;
    if (!flags.isInBounds(p[idx].pos, 1) || flags.isObstacle(p[idx].pos)) {
      p[idx].flag |= ParticleBase::PDELETE;
    }
  }
  inline std::vector<S> &getArg0()
  {
    return p;
  }
  typedef std::vector<S> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel KnDeleteInObstacle ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, flags);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<S> &p;
  const FlagGrid &flags;
};

// try to get closer to actual obstacle boundary
static inline Vec3 bisectBacktracePos(const FlagGrid &flags, const Vec3 &oldp, const Vec3 &newp)
{
  Real s = 0.;
  for (int i = 1; i < 5; ++i) {
    Real ds = 1. / (Real)(1 << i);
    if (!flags.isObstacle(oldp * (1. - (s + ds)) + newp * (s + ds))) {
      s += ds;
    }
  }
  return (oldp * (1. - (s)) + newp * (s));
}

// at least make sure all particles are inside domain

template<class S> struct KnClampPositions : public KernelBase {
  KnClampPositions(std::vector<S> &p,
                   const FlagGrid &flags,
                   ParticleDataImpl<Vec3> *posOld = NULL,
                   bool stopInObstacle = true,
                   const ParticleDataImpl<int> *ptype = NULL,
                   const int exclude = 0)
      : KernelBase(p.size()),
        p(p),
        flags(flags),
        posOld(posOld),
        stopInObstacle(stopInObstacle),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<S> &p,
                 const FlagGrid &flags,
                 ParticleDataImpl<Vec3> *posOld = NULL,
                 bool stopInObstacle = true,
                 const ParticleDataImpl<int> *ptype = NULL,
                 const int exclude = 0) const
  {
    if (p[idx].flag & ParticleBase::PDELETE)
      return;
    if (ptype && ((*ptype)[idx] & exclude)) {
      if (posOld)
        p[idx].pos = (*posOld)[idx];
      return;
    }
    if (!flags.isInBounds(p[idx].pos, 0)) {
      p[idx].pos = clamp(p[idx].pos, Vec3(0.), toVec3(flags.getSize()) - Vec3(1.));
    }
    if (stopInObstacle && (flags.isObstacle(p[idx].pos))) {
      p[idx].pos = bisectBacktracePos(flags, (*posOld)[idx], p[idx].pos);
    }
  }
  inline std::vector<S> &getArg0()
  {
    return p;
  }
  typedef std::vector<S> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline ParticleDataImpl<Vec3> *getArg2()
  {
    return posOld;
  }
  typedef ParticleDataImpl<Vec3> type2;
  inline bool &getArg3()
  {
    return stopInObstacle;
  }
  typedef bool type3;
  inline const ParticleDataImpl<int> *getArg4()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type4;
  inline const int &getArg5()
  {
    return exclude;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnClampPositions ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, flags, posOld, stopInObstacle, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<S> &p;
  const FlagGrid &flags;
  ParticleDataImpl<Vec3> *posOld;
  bool stopInObstacle;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

// advection plugin
template<class S>
void ParticleSystem<S>::advectInGrid(const FlagGrid &flags,
                                     const MACGrid &vel,
                                     const int integrationMode,
                                     const bool deleteInObstacle,
                                     const bool stopInObstacle,
                                     const bool skipNew,
                                     const ParticleDataImpl<int> *ptype,
                                     const int exclude)
{
  // position clamp requires old positions, backup
  ParticleDataImpl<Vec3> *posOld = NULL;
  if (!deleteInObstacle) {
    posOld = new ParticleDataImpl<Vec3>(this->getParent());
    posOld->resize(mData.size());
    for (IndexInt i = 0; i < (IndexInt)mData.size(); ++i)
      (*posOld)[i] = mData[i].pos;
  }

  // update positions
  GridAdvectKernel<S> kernel(mData,
                             vel,
                             flags,
                             getParent()->getDt(),
                             deleteInObstacle,
                             stopInObstacle,
                             skipNew,
                             ptype,
                             exclude);
  integratePointSet(kernel, integrationMode);

  if (!deleteInObstacle) {
    KnClampPositions<S>(mData, flags, posOld, stopInObstacle, ptype, exclude);
    delete posOld;
  }
  else {
    KnDeleteInObstacle<S>(mData, flags);
  }
}

template<class S> struct KnProjectParticles : public KernelBase {
  KnProjectParticles(ParticleSystem<S> &part, Grid<Vec3> &gradient)
      : KernelBase(part.size()), part(part), gradient(gradient)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, ParticleSystem<S> &part, Grid<Vec3> &gradient)
  {
    static RandomStream rand(3123984);
    const double jlen = 0.1;

    if (part.isActive(idx)) {
      // project along levelset gradient
      Vec3 p = part[idx].pos;
      if (gradient.isInBounds(p)) {
        Vec3 n = gradient.getInterpolated(p);
        Real dist = normalize(n);
        Vec3 dx = n * (-dist + jlen * (1 + rand.getReal()));
        p += dx;
      }
      // clamp to outer boundaries (+jitter)
      const double jlen = 0.1;
      Vec3 jitter = jlen * rand.getVec3();
      part[idx].pos = clamp(p, Vec3(1, 1, 1) + jitter, toVec3(gradient.getSize() - 1) - jitter);
    }
  }
  inline ParticleSystem<S> &getArg0()
  {
    return part;
  }
  typedef ParticleSystem<S> type0;
  inline Grid<Vec3> &getArg1()
  {
    return gradient;
  }
  typedef Grid<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel KnProjectParticles ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void run()
  {
    const IndexInt _sz = size;
    for (IndexInt i = 0; i < _sz; i++)
      op(i, part, gradient);
  }
  ParticleSystem<S> &part;
  Grid<Vec3> &gradient;
};

template<class S> void ParticleSystem<S>::projectOutside(Grid<Vec3> &gradient)
{
  KnProjectParticles<S>(*this, gradient);
}

template<class S> struct KnProjectOutOfBnd : public KernelBase {
  KnProjectOutOfBnd(ParticleSystem<S> &part,
                    const FlagGrid &flags,
                    const Real bnd,
                    const bool *axis,
                    const ParticleDataImpl<int> *ptype,
                    const int exclude)
      : KernelBase(part.size()),
        part(part),
        flags(flags),
        bnd(bnd),
        axis(axis),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 ParticleSystem<S> &part,
                 const FlagGrid &flags,
                 const Real bnd,
                 const bool *axis,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (!part.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    if (axis[0])
      part[idx].pos.x = std::max(part[idx].pos.x, bnd);
    if (axis[1])
      part[idx].pos.x = std::min(part[idx].pos.x, static_cast<Real>(flags.getSizeX()) - bnd);
    if (axis[2])
      part[idx].pos.y = std::max(part[idx].pos.y, bnd);
    if (axis[3])
      part[idx].pos.y = std::min(part[idx].pos.y, static_cast<Real>(flags.getSizeY()) - bnd);
    if (flags.is3D()) {
      if (axis[4])
        part[idx].pos.z = std::max(part[idx].pos.z, bnd);
      if (axis[5])
        part[idx].pos.z = std::min(part[idx].pos.z, static_cast<Real>(flags.getSizeZ()) - bnd);
    }
  }
  inline ParticleSystem<S> &getArg0()
  {
    return part;
  }
  typedef ParticleSystem<S> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const Real &getArg2()
  {
    return bnd;
  }
  typedef Real type2;
  inline const bool *getArg3()
  {
    return axis;
  }
  typedef bool type3;
  inline const ParticleDataImpl<int> *getArg4()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type4;
  inline const int &getArg5()
  {
    return exclude;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnProjectOutOfBnd ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, part, flags, bnd, axis, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  ParticleSystem<S> &part;
  const FlagGrid &flags;
  const Real bnd;
  const bool *axis;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

template<class S>
void ParticleSystem<S>::projectOutOfBnd(const FlagGrid &flags,
                                        const Real bnd,
                                        const std::string &plane,
                                        const ParticleDataImpl<int> *ptype,
                                        const int exclude)
{
  bool axis[6] = {false};
  for (std::string::const_iterator it = plane.begin(); it != plane.end(); ++it) {
    if (*it == 'x')
      axis[0] = true;
    if (*it == 'X')
      axis[1] = true;
    if (*it == 'y')
      axis[2] = true;
    if (*it == 'Y')
      axis[3] = true;
    if (*it == 'z')
      axis[4] = true;
    if (*it == 'Z')
      axis[5] = true;
  }
  KnProjectOutOfBnd<S>(*this, flags, bnd, axis, ptype, exclude);
}

template<class S> void ParticleSystem<S>::resizeAll(IndexInt size)
{
  // resize all buffers to target size in 1 go
  mData.resize(size);
  for (IndexInt i = 0; i < (IndexInt)mPartData.size(); ++i)
    mPartData[i]->resize(size);
}

template<class S> void ParticleSystem<S>::compress()
{
  IndexInt nextRead = mData.size();
  for (IndexInt i = 0; i < (IndexInt)mData.size(); i++) {
    while ((mData[i].flag & PDELETE) != 0) {
      nextRead--;
      mData[i] = mData[nextRead];
      // ugly, but prevent virtual function calls here:
      for (IndexInt pd = 0; pd < (IndexInt)mPdataReal.size(); ++pd)
        mPdataReal[pd]->copyValue(nextRead, i);
      for (IndexInt pd = 0; pd < (IndexInt)mPdataVec3.size(); ++pd)
        mPdataVec3[pd]->copyValue(nextRead, i);
      for (IndexInt pd = 0; pd < (IndexInt)mPdataInt.size(); ++pd)
        mPdataInt[pd]->copyValue(nextRead, i);
      mData[nextRead].flag = PINVALID;
    }
  }
  if (nextRead < (IndexInt)mData.size())
    debMsg("Deleted " << ((IndexInt)mData.size() - nextRead) << " particles", 1);  // debug info

  resizeAll(nextRead);
  mDeletes = 0;
  mDeleteChunk = mData.size() / DELETE_PART;
}

//! insert buffered positions as new particles, update additional particle data
template<class S> void ParticleSystem<S>::insertBufferedParticles()
{
  // clear new flag everywhere
  for (IndexInt i = 0; i < (IndexInt)mData.size(); ++i)
    mData[i].flag &= ~PNEW;

  if (mNewBufferPos.size() == 0)
    return;
  IndexInt newCnt = mData.size();
  resizeAll(newCnt + mNewBufferPos.size());

  for (IndexInt i = 0; i < (IndexInt)mNewBufferPos.size(); ++i) {
    int flag = (mNewBufferFlag.size() > 0) ? mNewBufferFlag[i] : 0;
    // note, other fields are not initialized here...
    mData[newCnt].pos = mNewBufferPos[i];
    mData[newCnt].flag = PNEW | flag;
    // now init pdata fields from associated grids...
    for (IndexInt pd = 0; pd < (IndexInt)mPdataReal.size(); ++pd)
      mPdataReal[pd]->initNewValue(newCnt, mNewBufferPos[i]);
    for (IndexInt pd = 0; pd < (IndexInt)mPdataVec3.size(); ++pd)
      mPdataVec3[pd]->initNewValue(newCnt, mNewBufferPos[i]);
    for (IndexInt pd = 0; pd < (IndexInt)mPdataInt.size(); ++pd)
      mPdataInt[pd]->initNewValue(newCnt, mNewBufferPos[i]);
    newCnt++;
  }
  if (mNewBufferPos.size() > 0)
    debMsg("Added & initialized " << (IndexInt)mNewBufferPos.size() << " particles",
           2);  // debug info
  mNewBufferPos.clear();
  mNewBufferFlag.clear();
}

template<class DATA, class CON> void ConnectedParticleSystem<DATA, CON>::compress()
{
  const IndexInt sz = ParticleSystem<DATA>::size();
  IndexInt *renumber_back = new IndexInt[sz];
  IndexInt *renumber = new IndexInt[sz];
  for (IndexInt i = 0; i < sz; i++)
    renumber[i] = renumber_back[i] = -1;

  // reorder elements
  std::vector<DATA> &data = ParticleSystem<DATA>::mData;
  IndexInt nextRead = sz;
  for (IndexInt i = 0; i < nextRead; i++) {
    if ((data[i].flag & ParticleBase::PDELETE) != 0) {
      nextRead--;
      data[i] = data[nextRead];
      data[nextRead].flag = 0;
      renumber_back[i] = nextRead;
    }
    else
      renumber_back[i] = i;
  }

  // acceleration structure
  for (IndexInt i = 0; i < nextRead; i++)
    renumber[renumber_back[i]] = i;

  // rename indices in filaments
  for (IndexInt i = 0; i < (IndexInt)mSegments.size(); i++)
    mSegments[i].renumber(renumber);

  ParticleSystem<DATA>::mData.resize(nextRead);
  ParticleSystem<DATA>::mDeletes = 0;
  ParticleSystem<DATA>::mDeleteChunk = ParticleSystem<DATA>::size() / DELETE_PART;

  delete[] renumber;
  delete[] renumber_back;
}

template<class S> ParticleBase *ParticleSystem<S>::clone()
{
  ParticleSystem<S> *nm = new ParticleSystem<S>(getParent());
  if (this->mAllowCompress)
    compress();

  nm->mData = mData;
  nm->setName(getName());
  this->cloneParticleData(nm);
  return nm;
}

template<class DATA, class CON> ParticleBase *ConnectedParticleSystem<DATA, CON>::clone()
{
  ConnectedParticleSystem<DATA, CON> *nm = new ConnectedParticleSystem<DATA, CON>(
      this->getParent());
  if (this->mAllowCompress)
    compress();

  nm->mData = this->mData;
  nm->mSegments = mSegments;
  nm->setName(this->getName());
  this->cloneParticleData(nm);
  return nm;
}

template<class S> std::string ParticleSystem<S>::infoString() const
{
  std::stringstream s;
  s << "ParticleSys '" << getName() << "'\n-> ";
  if (this->getNumPdata() > 0)
    s << "pdata: " << this->getNumPdata();
  s << "parts: " << size();
  // for(IndexInt i=0; i<(IndexInt)mPartData.size(); ++i) { sstr << i<<":" << mPartData[i]->size()
  // <<" "; }
  return s.str();
}

template<class S> inline void ParticleSystem<S>::checkPartIndex(IndexInt idx) const
{
  IndexInt mySize = this->size();
  if (idx < 0 || idx > mySize) {
    errMsg("ParticleBase "
           << " size " << mySize << " : index " << idx << " out of bound ");
  }
}

inline void ParticleDataBase::checkPartIndex(IndexInt idx) const
{
  IndexInt mySize = this->getSizeSlow();
  if (idx < 0 || idx > mySize) {
    errMsg("ParticleData "
           << " size " << mySize << " : index " << idx << " out of bound ");
  }
  if (mpParticleSys && mpParticleSys->getSizeSlow() != mySize) {
    errMsg("ParticleData "
           << " size " << mySize << " does not match parent! (" << mpParticleSys->getSizeSlow()
           << ") ");
  }
}

// set contents to zero, as for a grid
template<class T> void ParticleDataImpl<T>::clear()
{
  for (IndexInt i = 0; i < (IndexInt)mData.size(); ++i)
    mData[i] = 0.;
}

//! count by type flag
int countParticles(const ParticleDataImpl<int> &t, const int flag);

}  // namespace Manta

#endif
