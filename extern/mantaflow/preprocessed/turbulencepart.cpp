

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.ynu.org/licenses
 *
 * Turbulence particles
 *
 ******************************************************************************/

#include "turbulencepart.h"
#include "shapes.h"
#include "randomstream.h"

using namespace std;
namespace Manta {

TurbulenceParticleSystem::TurbulenceParticleSystem(FluidSolver *parent, WaveletNoiseField &noise)
    : ParticleSystem<TurbulenceParticleData>(parent), noise(noise)
{
}

ParticleBase *TurbulenceParticleSystem::clone()
{
  TurbulenceParticleSystem *nm = new TurbulenceParticleSystem(getParent(), noise);
  compress();

  nm->mData = mData;
  nm->setName(getName());
  return nm;
}

inline Vec3 hsv2rgb(Real h, Real s, Real v)
{
  Real r = 0, g = 0, b = 0;

  int i = (int)(h * 6);
  Real f = h * 6 - i;
  Real p = v * (1 - s);
  Real q = v * (1 - f * s);
  Real t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0:
      r = v, g = t, b = p;
      break;
    case 1:
      r = q, g = v, b = p;
      break;
    case 2:
      r = p, g = v, b = t;
      break;
    case 3:
      r = p, g = q, b = v;
      break;
    case 4:
      r = t, g = p, b = v;
      break;
    case 5:
      r = v, g = p, b = q;
      break;
    default:
      break;
  }

  return Vec3(r, g, b);
}

void TurbulenceParticleSystem::seed(Shape *shape, int num)
{
  static RandomStream rand(34894231);
  Vec3 sz = shape->getExtent(), p0 = shape->getCenter() - sz * 0.5;
  for (int i = 0; i < num; i++) {
    Vec3 p;
    do {
      p = rand.getVec3() * sz + p0;
    } while (!shape->isInside(p));
    Real z = (p.z - p0.z) / sz.z;
    add(TurbulenceParticleData(p, hsv2rgb(z, 0.75, 1.0)));
  }
}

void TurbulenceParticleSystem::resetTexCoords(int num, const Vec3 &inflow)
{
  if (num == 0) {
    for (int i = 0; i < size(); i++)
      mData[i].tex0 = mData[i].pos - inflow;
  }
  else {
    for (int i = 0; i < size(); i++)
      mData[i].tex1 = mData[i].pos - inflow;
  }
}

struct KnSynthesizeTurbulence : public KernelBase {
  KnSynthesizeTurbulence(TurbulenceParticleSystem &p,
                         FlagGrid &flags,
                         WaveletNoiseField &noise,
                         Grid<Real> &kGrid,
                         Real alpha,
                         Real dt,
                         int octaves,
                         Real scale,
                         Real invL0,
                         Real kmin)
      : KernelBase(p.size()),
        p(p),
        flags(flags),
        noise(noise),
        kGrid(kGrid),
        alpha(alpha),
        dt(dt),
        octaves(octaves),
        scale(scale),
        invL0(invL0),
        kmin(kmin)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 TurbulenceParticleSystem &p,
                 FlagGrid &flags,
                 WaveletNoiseField &noise,
                 Grid<Real> &kGrid,
                 Real alpha,
                 Real dt,
                 int octaves,
                 Real scale,
                 Real invL0,
                 Real kmin) const
  {
    const Real PERSISTENCE = 0.56123f;

    const Vec3 pos(p[idx].pos);
    if (flags.isInBounds(pos)) {  // && !flags.isObstacle(pos)) {
      Real k2 = kGrid.getInterpolated(pos) - kmin;
      Real ks = k2 < 0 ? 0.0 : sqrt(k2);

      // Wavelet noise lookup
      Real amplitude = scale * ks;
      Real multiplier = invL0;
      Vec3 vel(0.);
      for (int o = 0; o < octaves; o++) {
        // Vec3 ns = noise.evaluateCurl(p[i].pos * multiplier) * amplitude;
        Vec3 n0 = noise.evaluateCurl(p[idx].tex0 * multiplier) * amplitude;
        Vec3 n1 = noise.evaluateCurl(p[idx].tex1 * multiplier) * amplitude;
        vel += alpha * n0 + (1.0f - alpha) * n1;

        // next scale
        amplitude *= PERSISTENCE;
        multiplier *= 2.0f;
      }

      // advection
      Vec3 dx = vel * dt;
      p[idx].pos += dx;
      p[idx].tex0 += dx;
      p[idx].tex1 += dx;
    }
  }
  inline TurbulenceParticleSystem &getArg0()
  {
    return p;
  }
  typedef TurbulenceParticleSystem type0;
  inline FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline WaveletNoiseField &getArg2()
  {
    return noise;
  }
  typedef WaveletNoiseField type2;
  inline Grid<Real> &getArg3()
  {
    return kGrid;
  }
  typedef Grid<Real> type3;
  inline Real &getArg4()
  {
    return alpha;
  }
  typedef Real type4;
  inline Real &getArg5()
  {
    return dt;
  }
  typedef Real type5;
  inline int &getArg6()
  {
    return octaves;
  }
  typedef int type6;
  inline Real &getArg7()
  {
    return scale;
  }
  typedef Real type7;
  inline Real &getArg8()
  {
    return invL0;
  }
  typedef Real type8;
  inline Real &getArg9()
  {
    return kmin;
  }
  typedef Real type9;
  void runMessage()
  {
    debMsg("Executing kernel KnSynthesizeTurbulence ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, p, flags, noise, kGrid, alpha, dt, octaves, scale, invL0, kmin);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  TurbulenceParticleSystem &p;
  FlagGrid &flags;
  WaveletNoiseField &noise;
  Grid<Real> &kGrid;
  Real alpha;
  Real dt;
  int octaves;
  Real scale;
  Real invL0;
  Real kmin;
};

void TurbulenceParticleSystem::synthesize(FlagGrid &flags,
                                          Grid<Real> &k,
                                          int octaves,
                                          Real switchLength,
                                          Real L0,
                                          Real scale,
                                          Vec3 inflowBias)
{
  static Real ctime = 0;
  static Vec3 inflow(0.);
  Real dt = getParent()->getDt();

  // collect inflow bias
  inflow += inflowBias * dt;

  // alpha: hat function over time
  Real oldAlpha = 2.0f * nmod(ctime / switchLength, Real(1.0));
  ctime += dt;
  Real alpha = 2.0f * nmod(ctime / switchLength, Real(1.0));

  if (oldAlpha < 1.0f && alpha >= 1.0f)
    resetTexCoords(0, inflow);
  if (oldAlpha > alpha)
    resetTexCoords(1, inflow);
  if (alpha > 1.0f)
    alpha = 2.0f - alpha;
  alpha = 1.0;

  KnSynthesizeTurbulence(
      *this, flags, noise, k, alpha, dt, octaves, scale, 1.0f / L0, 1.5 * square(0.1));
}

void TurbulenceParticleSystem::deleteInObstacle(FlagGrid &flags)
{
  for (int i = 0; i < size(); i++)
    if (flags.isObstacle(mData[i].pos))
      mData[i].flag |= PDELETE;
  compress();
}

}  // namespace Manta
