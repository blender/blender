

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

// ----------------------------------------------------------------------------
//
// MantaFlow fluid solver framework
// Copyright 2016-2017 Kiwon Um, Nils Thuerey
//
// This program is free software, distributed under the terms of the
// Apache License, Version 2.0
// http://www.apache.org/licenses/LICENSE-2.0
//
// Affine Particle-In-Cell
//
// ----------------------------------------------------------------------------

#include "particle.h"
#include "grid.h"

namespace Manta {

struct knApicMapLinearVec3ToMACGrid : public KernelBase {
  knApicMapLinearVec3ToMACGrid(const BasicParticleSystem &p,
                               MACGrid &mg,
                               MACGrid &vg,
                               const ParticleDataImpl<Vec3> &vp,
                               const ParticleDataImpl<Vec3> &cpx,
                               const ParticleDataImpl<Vec3> &cpy,
                               const ParticleDataImpl<Vec3> &cpz,
                               const ParticleDataImpl<int> *ptype,
                               const int exclude)
      : KernelBase(p.size()),
        p(p),
        mg(mg),
        vg(vg),
        vp(vp),
        cpx(cpx),
        cpy(cpy),
        cpz(cpz),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystem &p,
                 MACGrid &mg,
                 MACGrid &vg,
                 const ParticleDataImpl<Vec3> &vp,
                 const ParticleDataImpl<Vec3> &cpx,
                 const ParticleDataImpl<Vec3> &cpy,
                 const ParticleDataImpl<Vec3> &cpz,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude)
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    const IndexInt dX[2] = {0, vg.getStrideX()};
    const IndexInt dY[2] = {0, vg.getStrideY()};
    const IndexInt dZ[2] = {0, vg.getStrideZ()};

    const Vec3 &pos = p[idx].pos, &vel = vp[idx];
    const IndexInt fi = static_cast<IndexInt>(pos.x), fj = static_cast<IndexInt>(pos.y),
                   fk = static_cast<IndexInt>(pos.z);
    const IndexInt ci = static_cast<IndexInt>(pos.x - 0.5),
                   cj = static_cast<IndexInt>(pos.y - 0.5),
                   ck = static_cast<IndexInt>(pos.z - 0.5);
    const Real wfi = clamp(pos.x - fi, Real(0), Real(1)),
               wfj = clamp(pos.y - fj, Real(0), Real(1)),
               wfk = clamp(pos.z - fk, Real(0), Real(1));
    const Real wci = clamp(Real(pos.x - ci - 0.5), Real(0), Real(1)),
               wcj = clamp(Real(pos.y - cj - 0.5), Real(0), Real(1)),
               wck = clamp(Real(pos.z - ck - 0.5), Real(0), Real(1));
    // TODO: check index for safety
    {  // u-face
      const IndexInt gidx = fi * dX[1] + cj * dY[1] + ck * dZ[1];
      const Vec3 gpos(fi, cj + 0.5, ck + 0.5);
      const Real wi[2] = {Real(1) - wfi, wfi};
      const Real wj[2] = {Real(1) - wcj, wcj};
      const Real wk[2] = {Real(1) - wck, wck};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const Real w = wi[i] * wj[j] * wk[k];
            mg[gidx + dX[i] + dY[j] + dZ[k]].x += w;
            vg[gidx + dX[i] + dY[j] + dZ[k]].x += w * vel.x;
            vg[gidx + dX[i] + dY[j] + dZ[k]].x += w * dot(cpx[idx], gpos + Vec3(i, j, k) - pos);
          }
    }
    {  // v-face
      const IndexInt gidx = ci * dX[1] + fj * dY[1] + ck * dZ[1];
      const Vec3 gpos(ci + 0.5, fj, ck + 0.5);
      const Real wi[2] = {Real(1) - wci, wci};
      const Real wj[2] = {Real(1) - wfj, wfj};
      const Real wk[2] = {Real(1) - wck, wck};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const Real w = wi[i] * wj[j] * wk[k];
            mg[gidx + dX[i] + dY[j] + dZ[k]].y += w;
            vg[gidx + dX[i] + dY[j] + dZ[k]].y += w * vel.y;
            vg[gidx + dX[i] + dY[j] + dZ[k]].y += w * dot(cpy[idx], gpos + Vec3(i, j, k) - pos);
          }
    }
    if (!vg.is3D())
      return;
    {  // w-face
      const IndexInt gidx = ci * dX[1] + cj * dY[1] + fk * dZ[1];
      const Vec3 gpos(ci + 0.5, cj + 0.5, fk);
      const Real wi[2] = {Real(1) - wci, wci};
      const Real wj[2] = {Real(1) - wcj, wcj};
      const Real wk[2] = {Real(1) - wfk, wfk};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const Real w = wi[i] * wj[j] * wk[k];
            mg[gidx + dX[i] + dY[j] + dZ[k]].z += w;
            vg[gidx + dX[i] + dY[j] + dZ[k]].z += w * vel.z;
            vg[gidx + dX[i] + dY[j] + dZ[k]].z += w * dot(cpz[idx], gpos + Vec3(i, j, k) - pos);
          }
    }
  }
  inline const BasicParticleSystem &getArg0()
  {
    return p;
  }
  typedef BasicParticleSystem type0;
  inline MACGrid &getArg1()
  {
    return mg;
  }
  typedef MACGrid type1;
  inline MACGrid &getArg2()
  {
    return vg;
  }
  typedef MACGrid type2;
  inline const ParticleDataImpl<Vec3> &getArg3()
  {
    return vp;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline const ParticleDataImpl<Vec3> &getArg4()
  {
    return cpx;
  }
  typedef ParticleDataImpl<Vec3> type4;
  inline const ParticleDataImpl<Vec3> &getArg5()
  {
    return cpy;
  }
  typedef ParticleDataImpl<Vec3> type5;
  inline const ParticleDataImpl<Vec3> &getArg6()
  {
    return cpz;
  }
  typedef ParticleDataImpl<Vec3> type6;
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
    debMsg("Executing kernel knApicMapLinearVec3ToMACGrid ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void run()
  {
    const IndexInt _sz = size;
    for (IndexInt i = 0; i < _sz; i++)
      op(i, p, mg, vg, vp, cpx, cpy, cpz, ptype, exclude);
  }
  const BasicParticleSystem &p;
  MACGrid &mg;
  MACGrid &vg;
  const ParticleDataImpl<Vec3> &vp;
  const ParticleDataImpl<Vec3> &cpx;
  const ParticleDataImpl<Vec3> &cpy;
  const ParticleDataImpl<Vec3> &cpz;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

void apicMapPartsToMAC(const FlagGrid &flags,
                       MACGrid &vel,
                       const BasicParticleSystem &parts,
                       const ParticleDataImpl<Vec3> &partVel,
                       const ParticleDataImpl<Vec3> &cpx,
                       const ParticleDataImpl<Vec3> &cpy,
                       const ParticleDataImpl<Vec3> &cpz,
                       MACGrid *mass = NULL,
                       const ParticleDataImpl<int> *ptype = NULL,
                       const int exclude = 0)
{
  // affine map
  // let's assume that the particle mass is constant, 1.0
  const bool freeMass = !mass;
  if (!mass)
    mass = new MACGrid(flags.getParent());
  else
    mass->clear();

  vel.clear();
  knApicMapLinearVec3ToMACGrid(parts, *mass, vel, partVel, cpx, cpy, cpz, ptype, exclude);
  mass->stomp(VECTOR_EPSILON);
  vel.safeDivide(*mass);

  if (freeMass)
    delete mass;
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "apicMapPartsToMAC", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      MACGrid &vel = *_args.getPtr<MACGrid>("vel", 1, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 2, &_lock);
      const ParticleDataImpl<Vec3> &partVel = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "partVel", 3, &_lock);
      const ParticleDataImpl<Vec3> &cpx = *_args.getPtr<ParticleDataImpl<Vec3>>("cpx", 4, &_lock);
      const ParticleDataImpl<Vec3> &cpy = *_args.getPtr<ParticleDataImpl<Vec3>>("cpy", 5, &_lock);
      const ParticleDataImpl<Vec3> &cpz = *_args.getPtr<ParticleDataImpl<Vec3>>("cpz", 6, &_lock);
      MACGrid *mass = _args.getPtrOpt<MACGrid>("mass", 7, NULL, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 8, NULL, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 9, 0, &_lock);
      _retval = getPyNone();
      apicMapPartsToMAC(flags, vel, parts, partVel, cpx, cpy, cpz, mass, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "apicMapPartsToMAC", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("apicMapPartsToMAC", e.what());
    return 0;
  }
}
static const Pb::Register _RP_apicMapPartsToMAC("", "apicMapPartsToMAC", _W_0);
extern "C" {
void PbRegister_apicMapPartsToMAC()
{
  KEEP_UNUSED(_RP_apicMapPartsToMAC);
}
}

struct knApicMapLinearMACGridToVec3 : public KernelBase {
  knApicMapLinearMACGridToVec3(ParticleDataImpl<Vec3> &vp,
                               ParticleDataImpl<Vec3> &cpx,
                               ParticleDataImpl<Vec3> &cpy,
                               ParticleDataImpl<Vec3> &cpz,
                               const BasicParticleSystem &p,
                               const MACGrid &vg,
                               const FlagGrid &flags,
                               const ParticleDataImpl<int> *ptype,
                               const int exclude)
      : KernelBase(vp.size()),
        vp(vp),
        cpx(cpx),
        cpy(cpy),
        cpz(cpz),
        p(p),
        vg(vg),
        flags(flags),
        ptype(ptype),
        exclude(exclude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 ParticleDataImpl<Vec3> &vp,
                 ParticleDataImpl<Vec3> &cpx,
                 ParticleDataImpl<Vec3> &cpy,
                 ParticleDataImpl<Vec3> &cpz,
                 const BasicParticleSystem &p,
                 const MACGrid &vg,
                 const FlagGrid &flags,
                 const ParticleDataImpl<int> *ptype,
                 const int exclude) const
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;

    vp[idx] = cpx[idx] = cpy[idx] = cpz[idx] = Vec3(Real(0));
    const IndexInt dX[2] = {0, vg.getStrideX()}, dY[2] = {0, vg.getStrideY()},
                   dZ[2] = {0, vg.getStrideZ()};
    const Real gw[2] = {-Real(1), Real(1)};

    const Vec3 &pos = p[idx].pos;
    const IndexInt fi = static_cast<IndexInt>(pos.x), fj = static_cast<IndexInt>(pos.y),
                   fk = static_cast<IndexInt>(pos.z);
    const IndexInt ci = static_cast<IndexInt>(pos.x - 0.5),
                   cj = static_cast<IndexInt>(pos.y - 0.5),
                   ck = static_cast<IndexInt>(pos.z - 0.5);
    const Real wfi = clamp(pos.x - fi, Real(0), Real(1)),
               wfj = clamp(pos.y - fj, Real(0), Real(1)),
               wfk = clamp(pos.z - fk, Real(0), Real(1));
    const Real wci = clamp(Real(pos.x - ci - 0.5), Real(0), Real(1)),
               wcj = clamp(Real(pos.y - cj - 0.5), Real(0), Real(1)),
               wck = clamp(Real(pos.z - ck - 0.5), Real(0), Real(1));
    // TODO: check index for safety
    {  // u
      const IndexInt gidx = fi * dX[1] + cj * dY[1] + ck * dZ[1];
      const Real wx[2] = {Real(1) - wfi, wfi};
      const Real wy[2] = {Real(1) - wcj, wcj};
      const Real wz[2] = {Real(1) - wck, wck};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const IndexInt vidx = gidx + dX[i] + dY[j] + dZ[k];
            Real vgx = vg[vidx].x;
            vp[idx].x += wx[i] * wy[j] * wz[k] * vgx;
            cpx[idx].x += gw[i] * wy[j] * wz[k] * vgx;
            cpx[idx].y += wx[i] * gw[j] * wz[k] * vgx;
            cpx[idx].z += wx[i] * wy[j] * gw[k] * vgx;
          }
    }
    {  // v
      const IndexInt gidx = ci * dX[1] + fj * dY[1] + ck * dZ[1];
      const Real wx[2] = {Real(1) - wci, wci};
      const Real wy[2] = {Real(1) - wfj, wfj};
      const Real wz[2] = {Real(1) - wck, wck};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const IndexInt vidx = gidx + dX[i] + dY[j] + dZ[k];
            Real vgy = vg[vidx].y;
            vp[idx].y += wx[i] * wy[j] * wz[k] * vgy;
            cpy[idx].x += gw[i] * wy[j] * wz[k] * vgy;
            cpy[idx].y += wx[i] * gw[j] * wz[k] * vgy;
            cpy[idx].z += wx[i] * wy[j] * gw[k] * vgy;
          }
    }
    if (!vg.is3D())
      return;
    {  // w
      const IndexInt gidx = ci * dX[1] + cj * dY[1] + fk * dZ[1];
      const Real wx[2] = {Real(1) - wci, wci};
      const Real wy[2] = {Real(1) - wcj, wcj};
      const Real wz[2] = {Real(1) - wfk, wfk};
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          for (int k = 0; k < 2; ++k) {
            const IndexInt vidx = gidx + dX[i] + dY[j] + dZ[k];
            Real vgz = vg[vidx].z;
            vp[idx].z += wx[i] * wy[j] * wz[k] * vgz;
            cpz[idx].x += gw[i] * wy[j] * wz[k] * vgz;
            cpz[idx].y += wx[i] * gw[j] * wz[k] * vgz;
            cpz[idx].z += wx[i] * wy[j] * gw[k] * vgz;
          }
    }
  }
  inline ParticleDataImpl<Vec3> &getArg0()
  {
    return vp;
  }
  typedef ParticleDataImpl<Vec3> type0;
  inline ParticleDataImpl<Vec3> &getArg1()
  {
    return cpx;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline ParticleDataImpl<Vec3> &getArg2()
  {
    return cpy;
  }
  typedef ParticleDataImpl<Vec3> type2;
  inline ParticleDataImpl<Vec3> &getArg3()
  {
    return cpz;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline const BasicParticleSystem &getArg4()
  {
    return p;
  }
  typedef BasicParticleSystem type4;
  inline const MACGrid &getArg5()
  {
    return vg;
  }
  typedef MACGrid type5;
  inline const FlagGrid &getArg6()
  {
    return flags;
  }
  typedef FlagGrid type6;
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
    debMsg("Executing kernel knApicMapLinearMACGridToVec3 ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, vp, cpx, cpy, cpz, p, vg, flags, ptype, exclude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  ParticleDataImpl<Vec3> &vp;
  ParticleDataImpl<Vec3> &cpx;
  ParticleDataImpl<Vec3> &cpy;
  ParticleDataImpl<Vec3> &cpz;
  const BasicParticleSystem &p;
  const MACGrid &vg;
  const FlagGrid &flags;
  const ParticleDataImpl<int> *ptype;
  const int exclude;
};

void apicMapMACGridToParts(ParticleDataImpl<Vec3> &partVel,
                           ParticleDataImpl<Vec3> &cpx,
                           ParticleDataImpl<Vec3> &cpy,
                           ParticleDataImpl<Vec3> &cpz,
                           const BasicParticleSystem &parts,
                           const MACGrid &vel,
                           const FlagGrid &flags,
                           const ParticleDataImpl<int> *ptype = NULL,
                           const int exclude = 0)
{
  knApicMapLinearMACGridToVec3(partVel, cpx, cpy, cpz, parts, vel, flags, ptype, exclude);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "apicMapMACGridToParts", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      ParticleDataImpl<Vec3> &partVel = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "partVel", 0, &_lock);
      ParticleDataImpl<Vec3> &cpx = *_args.getPtr<ParticleDataImpl<Vec3>>("cpx", 1, &_lock);
      ParticleDataImpl<Vec3> &cpy = *_args.getPtr<ParticleDataImpl<Vec3>>("cpy", 2, &_lock);
      ParticleDataImpl<Vec3> &cpz = *_args.getPtr<ParticleDataImpl<Vec3>>("cpz", 3, &_lock);
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 4, &_lock);
      const MACGrid &vel = *_args.getPtr<MACGrid>("vel", 5, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 6, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 7, NULL, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 8, 0, &_lock);
      _retval = getPyNone();
      apicMapMACGridToParts(partVel, cpx, cpy, cpz, parts, vel, flags, ptype, exclude);
      _args.check();
    }
    pbFinalizePlugin(parent, "apicMapMACGridToParts", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("apicMapMACGridToParts", e.what());
    return 0;
  }
}
static const Pb::Register _RP_apicMapMACGridToParts("", "apicMapMACGridToParts", _W_1);
extern "C" {
void PbRegister_apicMapMACGridToParts()
{
  KEEP_UNUSED(_RP_apicMapMACGridToParts);
}
}

}  // namespace Manta
