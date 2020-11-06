

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

// ----------------------------------------------------------------------------
//
// MantaFlow fluid solver framework
// Copyright 2018 Kiwon Um, Nils Thuerey
//
// This program is free software, distributed under the terms of the
// GNU General Public License (GPL)
// http://www.gnu.org/licenses
//
// Particle system helper
//
// ----------------------------------------------------------------------------

#include "particle.h"

namespace Manta {

struct KnAddForcePvel : public KernelBase {
  KnAddForcePvel(ParticleDataImpl<Vec3> &v,
                 const Vec3 &da,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude)
      : KernelBase(v.size()), v(v), da(da), ptype(ptype), exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 ParticleDataImpl<Vec3> &v,
                 const Vec3 &da,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (ptype && ((*ptype)[idx] & exclude))
      return;
    v[idx] += da;
  }
  inline ParticleDataImpl<Vec3> &getArg0()
  {
    return v;
  }
  typedef ParticleDataImpl<Vec3> type0;
  inline const Vec3 &getArg1()
  {
    return da;
  }
  typedef Vec3 type1;
  inline const ParticleDataImpl<int> *getArg2()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type2;
  inline const int &getArg3()
  {
    return exclude;
  }
  typedef int type3;
  void runMessage()
  {
    debMsg("Executing kernel KnAddForcePvel ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, v, da, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  ParticleDataImpl<Vec3> &v;
  const Vec3 &da;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};
//! add force to vec3 particle data; a: acceleration

void addForcePvel(ParticleDataImpl<Vec3> &vel,
                  const Vec3 &a,
                  const Real dt,
                  const ParticleDataImpl<int> *ptype,
                  const int exclude)
{
  KnAddForcePvel(vel, a * dt, ptype, exclude);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "addForcePvel", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      ParticleDataImpl<Vec3> &vel = *_args.getPtr<ParticleDataImpl<Vec3>>("vel", 0, &_lock);
      const Vec3 &a = _args.get<Vec3>("a", 1, &_lock);
      const Real dt = _args.get<Real>("dt", 2, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtr<ParticleDataImpl<int>>("ptype", 3, &_lock);
      const int exclude = _args.get<int>("exclude", 4, &_lock);
      _retval = getPyNone();
      addForcePvel(vel, a, dt, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "addForcePvel", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("addForcePvel", e.what());
    return 0;
  }
}
static const Pb::Register _RP_addForcePvel("", "addForcePvel", _W_0);
extern "C" {
void PbRegister_addForcePvel()
{
  KEEP_UNUSED(_RP_addForcePvel);
}
}

struct KnUpdateVelocityFromDeltaPos : public KernelBase {
  KnUpdateVelocityFromDeltaPos(const BasicParticleSystem &p,
                               ParticleDataImpl<Vec3> &v,
                               const ParticleDataImpl<Vec3> &x_prev,
                               const Real over_dt,
                               const ParticleDataImpl<int> *ptype,
                               const int exclude)
      : KernelBase(p.size()),
        p(p),
        v(v),
        x_prev(x_prev),
        over_dt(over_dt),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 ParticleDataImpl<Vec3> &v,
                 const ParticleDataImpl<Vec3> &x_prev,
                 const Real over_dt,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (ptype && ((*ptype)[idx] & exclude))
      return;
    v[idx] = (p[idx].pos - x_prev[idx]) * over_dt;
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline ParticleDataImpl<Vec3> &getArg1()
  {
    return v;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline const ParticleDataImpl<Vec3> &getArg2()
  {
    return x_prev;
  }
  typedef ParticleDataImpl<Vec3> type2;
  inline const Real &getArg3()
  {
    return over_dt;
  }
  typedef Real type3;
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
    debMsg("Executing kernel KnUpdateVelocityFromDeltaPos ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, v, x_prev, over_dt, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystem &p;
  ParticleDataImpl<Vec3> &v;
  const ParticleDataImpl<Vec3> &x_prev;
  const Real over_dt;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};
//! retrieve velocity from position change

void updateVelocityFromDeltaPos(const BasicParticleSystem &parts,
                                ParticleDataImpl<Vec3> &vel,
                                const ParticleDataImpl<Vec3> &x_prev,
                                const Real dt,
                                const ParticleDataImpl<int> *ptype,
                                const int exclude)
{
  KnUpdateVelocityFromDeltaPos(parts, vel, x_prev, 1.0 / dt, ptype, exclude);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "updateVelocityFromDeltaPos", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleDataImpl<Vec3> &vel = *_args.getPtr<ParticleDataImpl<Vec3>>("vel", 1, &_lock);
      const ParticleDataImpl<Vec3> &x_prev = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "x_prev", 2, &_lock);
      const Real dt = _args.get<Real>("dt", 3, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtr<ParticleDataImpl<int>>("ptype", 4, &_lock);
      const int exclude = _args.get<int>("exclude", 5, &_lock);
      _retval = getPyNone();
      updateVelocityFromDeltaPos(parts, vel, x_prev, dt, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "updateVelocityFromDeltaPos", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("updateVelocityFromDeltaPos", e.what());
    return 0;
  }
}
static const Pb::Register _RP_updateVelocityFromDeltaPos("", "updateVelocityFromDeltaPos", _W_1);
extern "C" {
void PbRegister_updateVelocityFromDeltaPos()
{
  KEEP_UNUSED(_RP_updateVelocityFromDeltaPos);
}
}

struct KnStepEuler : public KernelBase {
  KnStepEuler(BasicParticleSystem &p,
              const ParticleDataImpl<Vec3> &v,
              const Real dt,
              const ParticleDataImpl<int> *ptype,
              const int exclude)
      : KernelBase(p.size()), p(p), v(v), dt(dt), ptype(ptype), exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystem &p,
                 const ParticleDataImpl<Vec3> &v,
                 const Real dt,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (ptype && ((*ptype)[idx] & exclude))
      return;
    p[idx].pos += v[idx] * dt;
  }
  inline BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return v;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline const Real &getArg2()
  {
    return dt;
  }
  typedef Real type2;
  inline const ParticleDataImpl<int> *getArg3()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type3;
  inline const int &getArg4()
  {
    return exclude;
  }
  typedef int type4;
  void runMessage()
  {
    debMsg("Executing kernel KnStepEuler ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, v, dt, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystem &p;
  const ParticleDataImpl<Vec3> &v;
  const Real dt;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};
//! simple foward Euler integration for particle system

void eulerStep(BasicParticleSystem &parts,
               const ParticleDataImpl<Vec3> &vel,
               const ParticleDataImpl<int> *ptype,
               const int exclude)
{
  KnStepEuler(parts, vel, parts.getParent()->getDt(), ptype, exclude);
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "eulerStep", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const ParticleDataImpl<Vec3> &vel = *_args.getPtr<ParticleDataImpl<Vec3>>("vel", 1, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtr<ParticleDataImpl<int>>("ptype", 2, &_lock);
      const int exclude = _args.get<int>("exclude", 3, &_lock);
      _retval = getPyNone();
      eulerStep(parts, vel, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "eulerStep", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("eulerStep", e.what());
    return 0;
  }
}
static const Pb::Register _RP_eulerStep("", "eulerStep", _W_2);
extern "C" {
void PbRegister_eulerStep()
{
  KEEP_UNUSED(_RP_eulerStep);
}
}

struct KnSetPartType : public KernelBase {
  KnSetPartType(ParticleDataImpl<int> &ptype,
                const BasicParticleSystem &part,
                const int mark,
                const int stype,
                const FlagGrid &flags,
                const int cflag)
      : KernelBase(ptype.size()),
        ptype(ptype),
        part(part),
        mark(mark),
        stype(stype),
        flags(flags),
        cflag(cflag)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 ParticleDataImpl<int> &ptype,
                 const BasicParticleSystem &part,
                 const int mark,
                 const int stype,
                 const FlagGrid &flags,
                 const int cflag) const
  {
    if (flags.isInBounds(part.getPos(idx), 0) && (flags.getAt(part.getPos(idx)) & cflag) &&
        (ptype[idx] & stype))
      ptype[idx] = mark;
  }
  inline ParticleDataImpl<int> &getArg0()
  {
    return ptype;
  }
  typedef ParticleDataImpl<int> type0;
  inline const BasicParticleSystem &getArg1()
  {
    return part;
  }
  typedef BasicParticleSystem type1;
  inline const int &getArg2()
  {
    return mark;
  }
  typedef int type2;
  inline const int &getArg3()
  {
    return stype;
  }
  typedef int type3;
  inline const FlagGrid &getArg4()
  {
    return flags;
  }
  typedef FlagGrid type4;
  inline const int &getArg5()
  {
    return cflag;
  }
  typedef int type5;
  void runMessage()
  {
    debMsg("Executing kernel KnSetPartType ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, ptype, part, mark, stype, flags, cflag);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  ParticleDataImpl<int> &ptype;
  const BasicParticleSystem &part;
  const int mark;
  const int stype;
  const FlagGrid &flags;
  const int cflag;
};
//! if particle is stype and in cflag cell, set ptype as mark

void setPartType(const BasicParticleSystem &parts,
                 ParticleDataImpl<int> &ptype,
                 const int mark,
                 const int stype,
                 const FlagGrid &flags,
                 const int cflag)
{
  KnSetPartType(ptype, parts, mark, stype, flags, cflag);
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setPartType", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      ParticleDataImpl<int> &ptype = *_args.getPtr<ParticleDataImpl<int>>("ptype", 1, &_lock);
      const int mark = _args.get<int>("mark", 2, &_lock);
      const int stype = _args.get<int>("stype", 3, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 4, &_lock);
      const int cflag = _args.get<int>("cflag", 5, &_lock);
      _retval = getPyNone();
      setPartType(parts, ptype, mark, stype, flags, cflag);
      _args.check();
    }
    pbFinalizePlugin(parent, "setPartType", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setPartType", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setPartType("", "setPartType", _W_3);
extern "C" {
void PbRegister_setPartType()
{
  KEEP_UNUSED(_RP_setPartType);
}
}

}  // namespace Manta
