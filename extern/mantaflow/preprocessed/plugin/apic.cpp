

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

// ----------------------------------------------------------------------------
//
// MantaFlow fluid solver framework
// Copyright 2016-2020 Kiwon Um, Nils Thuerey
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

#define FOR_INT_IJK(num) \
  for (int i = 0; i < num; ++i) \
    for (int j = 0; j < num; ++j) \
      for (int k = 0; k < num; ++k)

static inline IndexInt indexUFace(const Vec3 &pos, const MACGrid &ref)
{
  const Vec3i f = toVec3i(pos), c = toVec3i(pos - 0.5);
  const IndexInt index = f.x * ref.getStrideX() + c.y * ref.getStrideY() + c.z * ref.getStrideZ();
  assertDeb(ref.isInBounds(index),
            "U face index out of bounds for particle position [" << pos.x << ", " << pos.y << ", "
                                                                 << pos.z << "]");
  return (ref.isInBounds(index)) ? index : -1;
}

static inline IndexInt indexVFace(const Vec3 &pos, const MACGrid &ref)
{
  const Vec3i f = toVec3i(pos), c = toVec3i(pos - 0.5);
  const IndexInt index = c.x * ref.getStrideX() + f.y * ref.getStrideY() + c.z * ref.getStrideZ();
  assertDeb(ref.isInBounds(index),
            "V face index out of bounds for particle position [" << pos.x << ", " << pos.y << ", "
                                                                 << pos.z << "]");
  return (ref.isInBounds(index)) ? index : -1;
}

static inline IndexInt indexWFace(const Vec3 &pos, const MACGrid &ref)
{
  const Vec3i f = toVec3i(pos), c = toVec3i(pos - 0.5);
  const IndexInt index = c.x * ref.getStrideX() + c.y * ref.getStrideY() + f.z * ref.getStrideZ();
  assertDeb(ref.isInBounds(index),
            "W face index out of bounds for particle position [" << pos.x << ", " << pos.y << ", "
                                                                 << pos.z << "]");
  return (ref.isInBounds(index)) ? index : -1;
}

static inline IndexInt indexOffset(
    const IndexInt gidx, const int i, const int j, const int k, const MACGrid &ref)
{
  const IndexInt dX[2] = {0, ref.getStrideX()};
  const IndexInt dY[2] = {0, ref.getStrideY()};
  const IndexInt dZ[2] = {0, ref.getStrideZ()};
  const IndexInt index = gidx + dX[i] + dY[j] + dZ[k];
  assertDeb(ref.isInBounds(index), "Offset index " << index << " is out of bounds");
  return (ref.isInBounds(index)) ? index : -1;
}

struct knApicMapLinearVec3ToMACGrid : public KernelBase {
  knApicMapLinearVec3ToMACGrid(const BasicParticleSystem &p,
                               MACGrid &mg,
                               MACGrid &vg,
                               const ParticleDataImpl<Vec3> &vp,
                               const ParticleDataImpl<Vec3> &cpx,
                               const ParticleDataImpl<Vec3> &cpy,
                               const ParticleDataImpl<Vec3> &cpz,
                               const ParticleDataImpl<int> *ptype,
                               const int exclude,
                               const int boundaryWidth)
      : KernelBase(p.size()),
        p(p),
        mg(mg),
        vg(vg),
        vp(vp),
        cpx(cpx),
        cpy(cpy),
        cpz(cpz),
        ptype(ptype),
        exclude(exclude),
        boundaryWidth(boundaryWidth)
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
                 const int exclude,
                 const int boundaryWidth)
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    if (!vg.isInBounds(p.getPos(idx), boundaryWidth)) {
      debMsg("Skipping particle at index " << idx
                                           << ". Is out of bounds and cannot be applied to grid.",
             1);
      return;
    }

    const Vec3 &pos = p.getPos(idx), &vel = vp[idx];
    const Vec3i f = toVec3i(pos);
    const Vec3i c = toVec3i(pos - 0.5);
    const Vec3 wf = clamp(pos - toVec3(f), Vec3(0.), Vec3(1.));
    const Vec3 wc = clamp(pos - toVec3(c) - 0.5, Vec3(0.), Vec3(1.));

    {  // u-face
      const IndexInt gidx = indexUFace(pos, vg);
      if (gidx < 0)
        return;  // debug will fail before

      const Vec3 gpos(f.x, c.y + 0.5, c.z + 0.5);
      const Real wi[2] = {Real(1) - wf.x, wf.x};
      const Real wj[2] = {Real(1) - wc.y, wc.y};
      const Real wk[2] = {Real(1) - wc.z, wc.z};

      FOR_INT_IJK(2)
      {
        const Real w = wi[i] * wj[j] * wk[k];
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;  // debug will fail before

        mg[vidx].x += w;
        vg[vidx].x += w * vel.x;
        vg[vidx].x += w * dot(cpx[idx], gpos + Vec3(i, j, k) - pos);
      }
    }
    {  // v-face
      const IndexInt gidx = indexVFace(pos, vg);
      if (gidx < 0)
        return;

      const Vec3 gpos(c.x + 0.5, f.y, c.z + 0.5);
      const Real wi[2] = {Real(1) - wc.x, wc.x};
      const Real wj[2] = {Real(1) - wf.y, wf.y};
      const Real wk[2] = {Real(1) - wc.z, wc.z};

      FOR_INT_IJK(2)
      {
        const Real w = wi[i] * wj[j] * wk[k];
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;

        mg[vidx].y += w;
        vg[vidx].y += w * vel.y;
        vg[vidx].y += w * dot(cpy[idx], gpos + Vec3(i, j, k) - pos);
      }
    }
    if (!vg.is3D())
      return;
    {  // w-face
      const IndexInt gidx = indexWFace(pos, vg);
      if (gidx < 0)
        return;

      const Vec3 gpos(c.x + 0.5, c.y + 0.5, f.z);
      const Real wi[2] = {Real(1) - wc.x, wc.x};
      const Real wj[2] = {Real(1) - wc.y, wc.y};
      const Real wk[2] = {Real(1) - wf.z, wf.z};

      FOR_INT_IJK(2)
      {
        const Real w = wi[i] * wj[j] * wk[k];
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;

        mg[vidx].z += w;
        vg[vidx].z += w * vel.z;
        vg[vidx].z += w * dot(cpz[idx], gpos + Vec3(i, j, k) - pos);
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
  inline const int &getArg9()
  {
    return boundaryWidth;
  }
  typedef int type9;
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
      op(i, p, mg, vg, vp, cpx, cpy, cpz, ptype, exclude, boundaryWidth);
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
  const int boundaryWidth;
};

void apicMapPartsToMAC(const FlagGrid &flags,
                       MACGrid &vel,
                       const BasicParticleSystem &parts,
                       const ParticleDataImpl<Vec3> &partVel,
                       const ParticleDataImpl<Vec3> &cpx,
                       const ParticleDataImpl<Vec3> &cpy,
                       const ParticleDataImpl<Vec3> &cpz,
                       MACGrid *mass = nullptr,
                       const ParticleDataImpl<int> *ptype = nullptr,
                       const int exclude = 0,
                       const int boundaryWidth = 0)
{
  // affine map: let's assume that the particle mass is constant, 1.0
  MACGrid tmpmass(vel.getParent());

  tmpmass.clear();
  vel.clear();

  knApicMapLinearVec3ToMACGrid(
      parts, tmpmass, vel, partVel, cpx, cpy, cpz, ptype, exclude, boundaryWidth);
  tmpmass.stomp(VECTOR_EPSILON);
  vel.safeDivide(tmpmass);

  if (mass)
    (*mass).swap(tmpmass);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "apicMapPartsToMAC", !noTiming);
    PyObject *_retval = nullptr;
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
      MACGrid *mass = _args.getPtrOpt<MACGrid>("mass", 7, nullptr, &_lock);
      const ParticleDataImpl<int> *ptype = _args.getPtrOpt<ParticleDataImpl<int>>(
          "ptype", 8, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 9, 0, &_lock);
      const int boundaryWidth = _args.getOpt<int>("boundaryWidth", 10, 0, &_lock);
      _retval = getPyNone();
      apicMapPartsToMAC(
          flags, vel, parts, partVel, cpx, cpy, cpz, mass, ptype, exclude, boundaryWidth);
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
                               const int exclude,
                               const int boundaryWidth)
      : KernelBase(vp.size()),
        vp(vp),
        cpx(cpx),
        cpy(cpy),
        cpz(cpz),
        p(p),
        vg(vg),
        flags(flags),
        ptype(ptype),
        exclude(exclude),
        boundaryWidth(boundaryWidth)
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
                 const int exclude,
                 const int boundaryWidth) const
  {
    if (!p.isActive(idx) || (ptype && ((*ptype)[idx] & exclude)))
      return;
    if (!vg.isInBounds(p.getPos(idx), boundaryWidth)) {
      debMsg("Skipping particle at index " << idx
                                           << ". Is out of bounds and cannot get value from grid.",
             1);
      return;
    }

    vp[idx] = cpx[idx] = cpy[idx] = cpz[idx] = Vec3(Real(0));
    const Real gw[2] = {-Real(1), Real(1)};

    const Vec3 &pos = p.getPos(idx);
    const Vec3i f = toVec3i(pos);
    const Vec3i c = toVec3i(pos - 0.5);
    const Vec3 wf = clamp(pos - toVec3(f), Vec3(0.), Vec3(1.));
    const Vec3 wc = clamp(pos - toVec3(c) - 0.5, Vec3(0.), Vec3(1.));

    {  // u-face
      const IndexInt gidx = indexUFace(pos, vg);
      if (gidx < 0)
        return;  // debug will fail before

      const Real wx[2] = {Real(1) - wf.x, wf.x};
      const Real wy[2] = {Real(1) - wc.y, wc.y};
      const Real wz[2] = {Real(1) - wc.z, wc.z};

      FOR_INT_IJK(2)
      {
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;  // debug will fail before

        const Real vgx = vg[vidx].x;
        vp[idx].x += wx[i] * wy[j] * wz[k] * vgx;
        cpx[idx].x += gw[i] * wy[j] * wz[k] * vgx;
        cpx[idx].y += wx[i] * gw[j] * wz[k] * vgx;
        cpx[idx].z += wx[i] * wy[j] * gw[k] * vgx;
      }
    }
    {  // v-face
      const IndexInt gidx = indexVFace(pos, vg);
      if (gidx < 0)
        return;

      const Real wx[2] = {Real(1) - wc.x, wc.x};
      const Real wy[2] = {Real(1) - wf.y, wf.y};
      const Real wz[2] = {Real(1) - wc.z, wc.z};

      FOR_INT_IJK(2)
      {
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;

        const Real vgy = vg[vidx].y;
        vp[idx].y += wx[i] * wy[j] * wz[k] * vgy;
        cpy[idx].x += gw[i] * wy[j] * wz[k] * vgy;
        cpy[idx].y += wx[i] * gw[j] * wz[k] * vgy;
        cpy[idx].z += wx[i] * wy[j] * gw[k] * vgy;
      }
    }
    if (!vg.is3D())
      return;
    {  // w-face
      const IndexInt gidx = indexWFace(pos, vg);
      if (gidx < 0)
        return;

      const Real wx[2] = {Real(1) - wc.x, wc.x};
      const Real wy[2] = {Real(1) - wc.y, wc.y};
      const Real wz[2] = {Real(1) - wf.z, wf.z};

      FOR_INT_IJK(2)
      {
        const IndexInt vidx = indexOffset(gidx, i, j, k, vg);
        if (vidx < 0)
          continue;

        const Real vgz = vg[vidx].z;
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
  inline const int &getArg9()
  {
    return boundaryWidth;
  }
  typedef int type9;
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
      op(idx, vp, cpx, cpy, cpz, p, vg, flags, ptype, exclude, boundaryWidth);
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
  const int boundaryWidth;
};

void apicMapMACGridToParts(ParticleDataImpl<Vec3> &partVel,
                           ParticleDataImpl<Vec3> &cpx,
                           ParticleDataImpl<Vec3> &cpy,
                           ParticleDataImpl<Vec3> &cpz,
                           const BasicParticleSystem &parts,
                           const MACGrid &vel,
                           const FlagGrid &flags,
                           const ParticleDataImpl<int> *ptype = nullptr,
                           const int exclude = 0,
                           const int boundaryWidth = 0)
{
  knApicMapLinearMACGridToVec3(
      partVel, cpx, cpy, cpz, parts, vel, flags, ptype, exclude, boundaryWidth);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "apicMapMACGridToParts", !noTiming);
    PyObject *_retval = nullptr;
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
          "ptype", 7, nullptr, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 8, 0, &_lock);
      const int boundaryWidth = _args.getOpt<int>("boundaryWidth", 9, 0, &_lock);
      _retval = getPyNone();
      apicMapMACGridToParts(
          partVel, cpx, cpy, cpz, parts, vel, flags, ptype, exclude, boundaryWidth);
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
