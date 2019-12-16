

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
 * Vortex particles
 * (warning, the vortex methods are currently experimental, and not fully supported!)
 *
 ******************************************************************************/

#include "vortexpart.h"
#include "integrator.h"
#include "mesh.h"

using namespace std;
namespace Manta {

// vortex particle effect: (cyl coord around wp)
// u = -|wp|*rho*exp( (-rho^2-z^2)/(2sigma^2) ) e_phi
inline Vec3 VortexKernel(const Vec3 &p, const vector<VortexParticleData> &vp, Real scale)
{
  Vec3 u(0.0);
  for (size_t i = 0; i < vp.size(); i++) {
    if (vp[i].flag & ParticleBase::PDELETE)
      continue;

    // cutoff radius
    const Vec3 r = p - vp[i].pos;
    const Real rlen2 = normSquare(r);
    const Real sigma2 = square(vp[i].sigma);
    if (rlen2 > 6.0 * sigma2 || rlen2 < 1e-8)
      continue;

    // split vortex strength
    Vec3 vortNorm = vp[i].vorticity;
    Real strength = normalize(vortNorm) * scale;

    // transform in cylinder coordinate system
    const Real rlen = sqrt(rlen2);
    const Real z = dot(r, vortNorm);
    const Vec3 ePhi = cross(r, vortNorm) / rlen;
    const Real rho2 = rlen2 - z * z;

    Real vortex = 0;
    if (rho2 > 1e-10) {
      // evaluate Kernel
      vortex = strength * sqrt(rho2) * exp(rlen2 * -0.5 / sigma2);
    }
    u += vortex * ePhi;
  }
  return u;
}

struct _KnVpAdvectMesh : public KernelBase {
  _KnVpAdvectMesh(const KernelBase &base,
                  vector<Node> &nodes,
                  const vector<VortexParticleData> &vp,
                  Real scale,
                  vector<Vec3> &u)
      : KernelBase(base), nodes(nodes), vp(vp), scale(scale), u(u)
  {
  }
  inline void op(IndexInt idx,
                 vector<Node> &nodes,
                 const vector<VortexParticleData> &vp,
                 Real scale,
                 vector<Vec3> &u) const
  {
    if (nodes[idx].flags & Mesh::NfFixed)
      u[idx] = 0.0;
    else
      u[idx] = VortexKernel(nodes[idx].pos, vp, scale);
  }
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, nodes, vp, scale, u);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  vector<Node> &nodes;
  const vector<VortexParticleData> &vp;
  Real scale;
  vector<Vec3> &u;
};
struct KnVpAdvectMesh : public KernelBase {
  KnVpAdvectMesh(vector<Node> &nodes, const vector<VortexParticleData> &vp, Real scale)
      : KernelBase(nodes.size()),
        _inner(KernelBase(nodes.size()), nodes, vp, scale, u),
        nodes(nodes),
        vp(vp),
        scale(scale),
        u((size))
  {
    runMessage();
    run();
  }
  void run()
  {
    _inner.run();
  }
  inline operator vector<Vec3>()
  {
    return u;
  }
  inline vector<Vec3> &getRet()
  {
    return u;
  }
  inline vector<Node> &getArg0()
  {
    return nodes;
  }
  typedef vector<Node> type0;
  inline const vector<VortexParticleData> &getArg1()
  {
    return vp;
  }
  typedef vector<VortexParticleData> type1;
  inline Real &getArg2()
  {
    return scale;
  }
  typedef Real type2;
  void runMessage()
  {
    debMsg("Executing kernel KnVpAdvectMesh ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  _KnVpAdvectMesh _inner;
  vector<Node> &nodes;
  const vector<VortexParticleData> &vp;
  Real scale;
  vector<Vec3> u;
};

struct _KnVpAdvectSelf : public KernelBase {
  _KnVpAdvectSelf(const KernelBase &base,
                  vector<VortexParticleData> &vp,
                  Real scale,
                  vector<Vec3> &u)
      : KernelBase(base), vp(vp), scale(scale), u(u)
  {
  }
  inline void op(IndexInt idx, vector<VortexParticleData> &vp, Real scale, vector<Vec3> &u) const
  {
    if (vp[idx].flag & ParticleBase::PDELETE)
      u[idx] = 0.0;
    else
      u[idx] = VortexKernel(vp[idx].pos, vp, scale);
  }
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, vp, scale, u);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  vector<VortexParticleData> &vp;
  Real scale;
  vector<Vec3> &u;
};
struct KnVpAdvectSelf : public KernelBase {
  KnVpAdvectSelf(vector<VortexParticleData> &vp, Real scale)
      : KernelBase(vp.size()),
        _inner(KernelBase(vp.size()), vp, scale, u),
        vp(vp),
        scale(scale),
        u((size))
  {
    runMessage();
    run();
  }
  void run()
  {
    _inner.run();
  }
  inline operator vector<Vec3>()
  {
    return u;
  }
  inline vector<Vec3> &getRet()
  {
    return u;
  }
  inline vector<VortexParticleData> &getArg0()
  {
    return vp;
  }
  typedef vector<VortexParticleData> type0;
  inline Real &getArg1()
  {
    return scale;
  }
  typedef Real type1;
  void runMessage()
  {
    debMsg("Executing kernel KnVpAdvectSelf ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  _KnVpAdvectSelf _inner;
  vector<VortexParticleData> &vp;
  Real scale;
  vector<Vec3> u;
};

VortexParticleSystem::VortexParticleSystem(FluidSolver *parent)
    : ParticleSystem<VortexParticleData>(parent)
{
}

void VortexParticleSystem::advectSelf(Real scale, int integrationMode)
{
  KnVpAdvectSelf kernel(mData, scale * getParent()->getDt());
  integratePointSet(kernel, integrationMode);
}

void VortexParticleSystem::applyToMesh(Mesh &mesh, Real scale, int integrationMode)
{
  KnVpAdvectMesh kernel(mesh.getNodeData(), mData, scale * getParent()->getDt());
  integratePointSet(kernel, integrationMode);
}

ParticleBase *VortexParticleSystem::clone()
{
  VortexParticleSystem *nm = new VortexParticleSystem(getParent());
  compress();

  nm->mData = mData;
  nm->setName(getName());
  return nm;
}

}  // namespace Manta
