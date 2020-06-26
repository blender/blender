

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2017 Georg Kohl, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL)
 * http://www.gnu.org/licenses
 *
 * Secondary particle plugin for FLIP simulations
 *
 ******************************************************************************/

#include "particle.h"
#include "commonkernels.h"

namespace Manta {

#pragma region Secondary Particles for FLIP
//----------------------------------------------------------------------------------------------------------------------------------------------------
// Secondary Particles for FLIP
//----------------------------------------------------------------------------------------------------------------------------------------------------

// helper function that clamps the value in potential to the interval [tauMin, tauMax] and
// normalizes it to [0, 1] afterwards
Real clampPotential(Real potential, Real tauMin, Real tauMax)
{
  return (std::min(potential, tauMax) - std::min(potential, tauMin)) / (tauMax - tauMin);
}

// computes all three potentials(trapped air, wave crest, kinetic energy) and the neighbor ratio
// for every fluid cell and stores it in the respective grid. Is less readable but significantly
// faster than using seperate potential computation

struct knFlipComputeSecondaryParticlePotentials : public KernelBase {
  knFlipComputeSecondaryParticlePotentials(Grid<Real> &potTA,
                                           Grid<Real> &potWC,
                                           Grid<Real> &potKE,
                                           Grid<Real> &neighborRatio,
                                           const FlagGrid &flags,
                                           const MACGrid &v,
                                           const Grid<Vec3> &normal,
                                           const int radius,
                                           const Real tauMinTA,
                                           const Real tauMaxTA,
                                           const Real tauMinWC,
                                           const Real tauMaxWC,
                                           const Real tauMinKE,
                                           const Real tauMaxKE,
                                           const Real scaleFromManta,
                                           const int itype = FlagGrid::TypeFluid,
                                           const int jtype = FlagGrid::TypeObstacle |
                                                             FlagGrid::TypeOutflow |
                                                             FlagGrid::TypeInflow)
      : KernelBase(&potTA, radius),
        potTA(potTA),
        potWC(potWC),
        potKE(potKE),
        neighborRatio(neighborRatio),
        flags(flags),
        v(v),
        normal(normal),
        radius(radius),
        tauMinTA(tauMinTA),
        tauMaxTA(tauMaxTA),
        tauMinWC(tauMinWC),
        tauMaxWC(tauMaxWC),
        tauMinKE(tauMinKE),
        tauMaxKE(tauMaxKE),
        scaleFromManta(scaleFromManta),
        itype(itype),
        jtype(jtype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &potTA,
                 Grid<Real> &potWC,
                 Grid<Real> &potKE,
                 Grid<Real> &neighborRatio,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const Grid<Vec3> &normal,
                 const int radius,
                 const Real tauMinTA,
                 const Real tauMaxTA,
                 const Real tauMinWC,
                 const Real tauMaxWC,
                 const Real tauMinKE,
                 const Real tauMaxKE,
                 const Real scaleFromManta,
                 const int itype = FlagGrid::TypeFluid,
                 const int jtype = FlagGrid::TypeObstacle | FlagGrid::TypeOutflow |
                                   FlagGrid::TypeInflow) const
  {

    if (!(flags(i, j, k) & itype))
      return;

    // compute trapped air potential + wave crest potential + neighbor ratio at once
    const Vec3 &xi = scaleFromManta * Vec3(i, j, k);  // scale to unit cube
    const Vec3 &vi = scaleFromManta * v.getCentered(i, j, k);
    const Vec3 &ni = getNormalized(normal(i, j, k));
    Real vdiff = 0;         // for trapped air
    Real kappa = 0;         // for wave crests
    int countFluid = 0;     // for neighbor ratio
    int countMaxFluid = 0;  // for neighbor ratio

    // iterate over neighboring cells within radius
    for (IndexInt x = i - radius; x <= i + radius; x++) {
      for (IndexInt y = j - radius; y <= j + radius; y++) {
        for (IndexInt z = k - radius; z <= k + radius; z++) {
          // ensure that xyz is in bounds: use bnd=1 to ensure that vel.getCentered() always has a
          // neighbor cell
          if ((x == i && y == j && z == k) || !flags.isInBounds(Vec3i(x, y, z), 1) ||
              (flags(x, y, z) & jtype))
            continue;

          if (flags(x, y, z) & itype) {
            countFluid++;
            countMaxFluid++;
          }
          else {
            countMaxFluid++;
          }

          const Vec3 &xj = scaleFromManta * Vec3(x, y, z);  // scale to unit cube
          const Vec3 &vj = scaleFromManta * v.getCentered(x, y, z);
          const Vec3 &nj = getNormalized(normal(x, y, z));
          const Vec3 xij = xi - xj;
          const Vec3 vij = vi - vj;
          Real h = !potTA.is3D() ?
                       1.414 * radius :
                       1.732 * radius;  // estimate sqrt(2)*radius resp. sqrt(3)*radius for h, due
                                        // to squared resp. cubic neighbor area
          vdiff += norm(vij) * (1 - dot(getNormalized(vij), getNormalized(xij))) *
                   (1 - norm(xij) / h);

          if (dot(getNormalized(xij), ni) < 0) {  // identifies wave crests
            kappa += (1 - dot(ni, nj)) * (1 - norm(xij) / h);
          }
        }
      }
    }

    neighborRatio(i, j, k) = float(countFluid) / float(countMaxFluid);

    potTA(i, j, k) = clampPotential(vdiff, tauMinTA, tauMaxTA);
    if (dot(getNormalized(vi), ni) >= 0.6) {  // avoid to mark boarders of the scene as wave crest
      potWC(i, j, k) = clampPotential(kappa, tauMinWC, tauMaxWC);
    }
    else {
      potWC(i, j, k) = Real(0);
    }

    // compute kinetic energy potential
    Real ek =
        Real(0.5) * 125 *
        normSquare(
            vi);  // use arbitrary constant for mass, potential adjusts with thresholds anyways
    potKE(i, j, k) = clampPotential(ek, tauMinKE, tauMaxKE);
  }
  inline Grid<Real> &getArg0()
  {
    return potTA;
  }
  typedef Grid<Real> type0;
  inline Grid<Real> &getArg1()
  {
    return potWC;
  }
  typedef Grid<Real> type1;
  inline Grid<Real> &getArg2()
  {
    return potKE;
  }
  typedef Grid<Real> type2;
  inline Grid<Real> &getArg3()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type3;
  inline const FlagGrid &getArg4()
  {
    return flags;
  }
  typedef FlagGrid type4;
  inline const MACGrid &getArg5()
  {
    return v;
  }
  typedef MACGrid type5;
  inline const Grid<Vec3> &getArg6()
  {
    return normal;
  }
  typedef Grid<Vec3> type6;
  inline const int &getArg7()
  {
    return radius;
  }
  typedef int type7;
  inline const Real &getArg8()
  {
    return tauMinTA;
  }
  typedef Real type8;
  inline const Real &getArg9()
  {
    return tauMaxTA;
  }
  typedef Real type9;
  inline const Real &getArg10()
  {
    return tauMinWC;
  }
  typedef Real type10;
  inline const Real &getArg11()
  {
    return tauMaxWC;
  }
  typedef Real type11;
  inline const Real &getArg12()
  {
    return tauMinKE;
  }
  typedef Real type12;
  inline const Real &getArg13()
  {
    return tauMaxKE;
  }
  typedef Real type13;
  inline const Real &getArg14()
  {
    return scaleFromManta;
  }
  typedef Real type14;
  inline const int &getArg15()
  {
    return itype;
  }
  typedef int type15;
  inline const int &getArg16()
  {
    return jtype;
  }
  typedef int type16;
  void runMessage()
  {
    debMsg("Executing kernel knFlipComputeSecondaryParticlePotentials ", 3);
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
        for (int j = radius; j < _maxY; j++)
          for (int i = radius; i < _maxX; i++)
            op(i,
               j,
               k,
               potTA,
               potWC,
               potKE,
               neighborRatio,
               flags,
               v,
               normal,
               radius,
               tauMinTA,
               tauMaxTA,
               tauMinWC,
               tauMaxWC,
               tauMinKE,
               tauMaxKE,
               scaleFromManta,
               itype,
               jtype);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = radius; i < _maxX; i++)
          op(i,
             j,
             k,
             potTA,
             potWC,
             potKE,
             neighborRatio,
             flags,
             v,
             normal,
             radius,
             tauMinTA,
             tauMaxTA,
             tauMinWC,
             tauMaxWC,
             tauMinKE,
             tauMaxKE,
             scaleFromManta,
             itype,
             jtype);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(radius, maxY), *this);
  }
  Grid<Real> &potTA;
  Grid<Real> &potWC;
  Grid<Real> &potKE;
  Grid<Real> &neighborRatio;
  const FlagGrid &flags;
  const MACGrid &v;
  const Grid<Vec3> &normal;
  const int radius;
  const Real tauMinTA;
  const Real tauMaxTA;
  const Real tauMinWC;
  const Real tauMaxWC;
  const Real tauMinKE;
  const Real tauMaxKE;
  const Real scaleFromManta;
  const int itype;
  const int jtype;
};

void flipComputeSecondaryParticlePotentials(Grid<Real> &potTA,
                                            Grid<Real> &potWC,
                                            Grid<Real> &potKE,
                                            Grid<Real> &neighborRatio,
                                            const FlagGrid &flags,
                                            const MACGrid &v,
                                            Grid<Vec3> &normal,
                                            const Grid<Real> &phi,
                                            const int radius,
                                            const Real tauMinTA,
                                            const Real tauMaxTA,
                                            const Real tauMinWC,
                                            const Real tauMaxWC,
                                            const Real tauMinKE,
                                            const Real tauMaxKE,
                                            const Real scaleFromManta,
                                            const int itype = FlagGrid::TypeFluid,
                                            const int jtype = FlagGrid::TypeObstacle |
                                                              FlagGrid::TypeOutflow |
                                                              FlagGrid::TypeInflow)
{
  potTA.clear();
  potWC.clear();
  potKE.clear();
  neighborRatio.clear();
  GradientOp(normal, phi);
  knFlipComputeSecondaryParticlePotentials(potTA,
                                           potWC,
                                           potKE,
                                           neighborRatio,
                                           flags,
                                           v,
                                           normal,
                                           radius,
                                           tauMinTA,
                                           tauMaxTA,
                                           tauMinWC,
                                           tauMaxWC,
                                           tauMinKE,
                                           tauMaxKE,
                                           scaleFromManta,
                                           itype,
                                           jtype);
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipComputeSecondaryParticlePotentials", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &potTA = *_args.getPtr<Grid<Real>>("potTA", 0, &_lock);
      Grid<Real> &potWC = *_args.getPtr<Grid<Real>>("potWC", 1, &_lock);
      Grid<Real> &potKE = *_args.getPtr<Grid<Real>>("potKE", 2, &_lock);
      Grid<Real> &neighborRatio = *_args.getPtr<Grid<Real>>("neighborRatio", 3, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 4, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 5, &_lock);
      Grid<Vec3> &normal = *_args.getPtr<Grid<Vec3>>("normal", 6, &_lock);
      const Grid<Real> &phi = *_args.getPtr<Grid<Real>>("phi", 7, &_lock);
      const int radius = _args.get<int>("radius", 8, &_lock);
      const Real tauMinTA = _args.get<Real>("tauMinTA", 9, &_lock);
      const Real tauMaxTA = _args.get<Real>("tauMaxTA", 10, &_lock);
      const Real tauMinWC = _args.get<Real>("tauMinWC", 11, &_lock);
      const Real tauMaxWC = _args.get<Real>("tauMaxWC", 12, &_lock);
      const Real tauMinKE = _args.get<Real>("tauMinKE", 13, &_lock);
      const Real tauMaxKE = _args.get<Real>("tauMaxKE", 14, &_lock);
      const Real scaleFromManta = _args.get<Real>("scaleFromManta", 15, &_lock);
      const int itype = _args.getOpt<int>("itype", 16, FlagGrid::TypeFluid, &_lock);
      const int jtype = _args.getOpt<int>("jtype",
                                          17,
                                          FlagGrid::TypeObstacle | FlagGrid::TypeOutflow |
                                              FlagGrid::TypeInflow,
                                          &_lock);
      _retval = getPyNone();
      flipComputeSecondaryParticlePotentials(potTA,
                                             potWC,
                                             potKE,
                                             neighborRatio,
                                             flags,
                                             v,
                                             normal,
                                             phi,
                                             radius,
                                             tauMinTA,
                                             tauMaxTA,
                                             tauMinWC,
                                             tauMaxWC,
                                             tauMinKE,
                                             tauMaxKE,
                                             scaleFromManta,
                                             itype,
                                             jtype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipComputeSecondaryParticlePotentials", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipComputeSecondaryParticlePotentials", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipComputeSecondaryParticlePotentials(
    "", "flipComputeSecondaryParticlePotentials", _W_0);
extern "C" {
void PbRegister_flipComputeSecondaryParticlePotentials()
{
  KEEP_UNUSED(_RP_flipComputeSecondaryParticlePotentials);
}
}

// adds secondary particles to &pts_sec for every fluid cell in &flags according to the potential
// grids &potTA, &potWC and &potKE secondary particles are uniformly sampled in every fluid cell in
// a randomly offset cylinder in fluid movement direction In contrast to
// flipSampleSecondaryParticles this uses more cylinders per cell and interpolates velocity and
// potentials. To control number of cylinders in each dimension adjust radius(0.25=>2 cyl,
// 0.1666=>3 cyl, 0.125=>3cyl etc.).

struct knFlipSampleSecondaryParticlesMoreCylinders : public KernelBase {
  knFlipSampleSecondaryParticlesMoreCylinders(const FlagGrid &flags,
                                              const MACGrid &v,
                                              BasicParticleSystem &pts_sec,
                                              ParticleDataImpl<Vec3> &v_sec,
                                              ParticleDataImpl<Real> &l_sec,
                                              const Real lMin,
                                              const Real lMax,
                                              const Grid<Real> &potTA,
                                              const Grid<Real> &potWC,
                                              const Grid<Real> &potKE,
                                              const Grid<Real> &neighborRatio,
                                              const Real c_s,
                                              const Real c_b,
                                              const Real k_ta,
                                              const Real k_wc,
                                              const Real dt,
                                              const int itype = FlagGrid::TypeFluid)
      : KernelBase(&flags, 0),
        flags(flags),
        v(v),
        pts_sec(pts_sec),
        v_sec(v_sec),
        l_sec(l_sec),
        lMin(lMin),
        lMax(lMax),
        potTA(potTA),
        potWC(potWC),
        potKE(potKE),
        neighborRatio(neighborRatio),
        c_s(c_s),
        c_b(c_b),
        k_ta(k_ta),
        k_wc(k_wc),
        dt(dt),
        itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 BasicParticleSystem &pts_sec,
                 ParticleDataImpl<Vec3> &v_sec,
                 ParticleDataImpl<Real> &l_sec,
                 const Real lMin,
                 const Real lMax,
                 const Grid<Real> &potTA,
                 const Grid<Real> &potWC,
                 const Grid<Real> &potKE,
                 const Grid<Real> &neighborRatio,
                 const Real c_s,
                 const Real c_b,
                 const Real k_ta,
                 const Real k_wc,
                 const Real dt,
                 const int itype = FlagGrid::TypeFluid)
  {

    if (!(flags(i, j, k) & itype))
      return;

    static RandomStream mRand(9832);
    Real radius =
        0.25;  // diameter=0.5 => sampling with two cylinders in each dimension since cell size=1
    for (Real x = i - radius; x <= i + radius; x += 2 * radius) {
      for (Real y = j - radius; y <= j + radius; y += 2 * radius) {
        for (Real z = k - radius; z <= k + radius; z += 2 * radius) {

          Vec3 xi = Vec3(x, y, z);
          Real KE = potKE.getInterpolated(xi);
          Real TA = potTA.getInterpolated(xi);
          Real WC = potWC.getInterpolated(xi);

          const int n = KE * (k_ta * TA + k_wc * WC) * dt;  // number of secondary particles
          if (n == 0)
            continue;
          Vec3 vi = v.getInterpolated(xi);
          Vec3 dir = dt * vi;  // direction of movement of current particle
          Vec3 e1 = getNormalized(Vec3(dir.z, 0, -dir.x));  // perpendicular to dir
          Vec3 e2 = getNormalized(
              cross(e1, dir));  // perpendicular to dir and e1, so e1 and e1 create reference plane

          for (int di = 0; di < n; di++) {
            const Real r = radius * sqrt(mRand.getReal());        // distance to cylinder axis
            const Real theta = mRand.getReal() * Real(2) * M_PI;  // azimuth
            const Real h = mRand.getReal() * norm(dt * vi);       // distance to reference plane
            Vec3 xd = xi + r * cos(theta) * e1 + r * sin(theta) * e2 + h * getNormalized(vi);
            if (!flags.is3D())
              xd.z = 0;
            pts_sec.add(xd);

            v_sec[v_sec.size() - 1] = r * cos(theta) * e1 + r * sin(theta) * e2 +
                                      vi;  // init velocity of new particle
            Real temp = (KE + TA + WC) / 3;
            l_sec[l_sec.size() - 1] = ((lMax - lMin) * temp) + lMin +
                                      mRand.getReal() * 0.1;  // init lifetime of new particle

            // init type of new particle
            if (neighborRatio(i, j, k) < c_s) {
              pts_sec[pts_sec.size() - 1].flag = ParticleBase::PSPRAY;
            }
            else if (neighborRatio(i, j, k) > c_b) {
              pts_sec[pts_sec.size() - 1].flag = ParticleBase::PBUBBLE;
            }
            else {
              pts_sec[pts_sec.size() - 1].flag = ParticleBase::PFOAM;
            }
          }
        }
      }
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const MACGrid &getArg1()
  {
    return v;
  }
  typedef MACGrid type1;
  inline BasicParticleSystem &getArg2()
  {
    return pts_sec;
  }
  typedef BasicParticleSystem type2;
  inline ParticleDataImpl<Vec3> &getArg3()
  {
    return v_sec;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline ParticleDataImpl<Real> &getArg4()
  {
    return l_sec;
  }
  typedef ParticleDataImpl<Real> type4;
  inline const Real &getArg5()
  {
    return lMin;
  }
  typedef Real type5;
  inline const Real &getArg6()
  {
    return lMax;
  }
  typedef Real type6;
  inline const Grid<Real> &getArg7()
  {
    return potTA;
  }
  typedef Grid<Real> type7;
  inline const Grid<Real> &getArg8()
  {
    return potWC;
  }
  typedef Grid<Real> type8;
  inline const Grid<Real> &getArg9()
  {
    return potKE;
  }
  typedef Grid<Real> type9;
  inline const Grid<Real> &getArg10()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type10;
  inline const Real &getArg11()
  {
    return c_s;
  }
  typedef Real type11;
  inline const Real &getArg12()
  {
    return c_b;
  }
  typedef Real type12;
  inline const Real &getArg13()
  {
    return k_ta;
  }
  typedef Real type13;
  inline const Real &getArg14()
  {
    return k_wc;
  }
  typedef Real type14;
  inline const Real &getArg15()
  {
    return dt;
  }
  typedef Real type15;
  inline const int &getArg16()
  {
    return itype;
  }
  typedef int type16;
  void runMessage()
  {
    debMsg("Executing kernel knFlipSampleSecondaryParticlesMoreCylinders ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void run()
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    for (int k = minZ; k < maxZ; k++)
      for (int j = 0; j < _maxY; j++)
        for (int i = 0; i < _maxX; i++)
          op(i,
             j,
             k,
             flags,
             v,
             pts_sec,
             v_sec,
             l_sec,
             lMin,
             lMax,
             potTA,
             potWC,
             potKE,
             neighborRatio,
             c_s,
             c_b,
             k_ta,
             k_wc,
             dt,
             itype);
  }
  const FlagGrid &flags;
  const MACGrid &v;
  BasicParticleSystem &pts_sec;
  ParticleDataImpl<Vec3> &v_sec;
  ParticleDataImpl<Real> &l_sec;
  const Real lMin;
  const Real lMax;
  const Grid<Real> &potTA;
  const Grid<Real> &potWC;
  const Grid<Real> &potKE;
  const Grid<Real> &neighborRatio;
  const Real c_s;
  const Real c_b;
  const Real k_ta;
  const Real k_wc;
  const Real dt;
  const int itype;
};

// adds secondary particles to &pts_sec for every fluid cell in &flags according to the potential
// grids &potTA, &potWC and &potKE secondary particles are uniformly sampled in every fluid cell in
// a randomly offset cylinder in fluid movement direction

struct knFlipSampleSecondaryParticles : public KernelBase {
  knFlipSampleSecondaryParticles(const FlagGrid &flags,
                                 const MACGrid &v,
                                 BasicParticleSystem &pts_sec,
                                 ParticleDataImpl<Vec3> &v_sec,
                                 ParticleDataImpl<Real> &l_sec,
                                 const Real lMin,
                                 const Real lMax,
                                 const Grid<Real> &potTA,
                                 const Grid<Real> &potWC,
                                 const Grid<Real> &potKE,
                                 const Grid<Real> &neighborRatio,
                                 const Real c_s,
                                 const Real c_b,
                                 const Real k_ta,
                                 const Real k_wc,
                                 const Real dt,
                                 const int itype = FlagGrid::TypeFluid)
      : KernelBase(&flags, 0),
        flags(flags),
        v(v),
        pts_sec(pts_sec),
        v_sec(v_sec),
        l_sec(l_sec),
        lMin(lMin),
        lMax(lMax),
        potTA(potTA),
        potWC(potWC),
        potKE(potKE),
        neighborRatio(neighborRatio),
        c_s(c_s),
        c_b(c_b),
        k_ta(k_ta),
        k_wc(k_wc),
        dt(dt),
        itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 BasicParticleSystem &pts_sec,
                 ParticleDataImpl<Vec3> &v_sec,
                 ParticleDataImpl<Real> &l_sec,
                 const Real lMin,
                 const Real lMax,
                 const Grid<Real> &potTA,
                 const Grid<Real> &potWC,
                 const Grid<Real> &potKE,
                 const Grid<Real> &neighborRatio,
                 const Real c_s,
                 const Real c_b,
                 const Real k_ta,
                 const Real k_wc,
                 const Real dt,
                 const int itype = FlagGrid::TypeFluid)
  {

    if (!(flags(i, j, k) & itype))
      return;

    Real KE = potKE(i, j, k);
    Real TA = potTA(i, j, k);
    Real WC = potWC(i, j, k);

    const int n = KE * (k_ta * TA + k_wc * WC) * dt;  // number of secondary particles
    if (n == 0)
      return;
    static RandomStream mRand(9832);

    Vec3 xi = Vec3(i, j, k) + mRand.getVec3();  // randomized offset uniform in cell
    Vec3 vi = v.getInterpolated(xi);
    Vec3 dir = dt * vi;                               // direction of movement of current particle
    Vec3 e1 = getNormalized(Vec3(dir.z, 0, -dir.x));  // perpendicular to dir
    Vec3 e2 = getNormalized(
        cross(e1, dir));  // perpendicular to dir and e1, so e1 and e1 create reference plane

    for (int di = 0; di < n; di++) {
      const Real r = Real(0.5) * sqrt(mRand.getReal());     // distance to cylinder axis
      const Real theta = mRand.getReal() * Real(2) * M_PI;  // azimuth
      const Real h = mRand.getReal() * norm(dt * vi);       // distance to reference plane
      Vec3 xd = xi + r * cos(theta) * e1 + r * sin(theta) * e2 + h * getNormalized(vi);
      if (!flags.is3D())
        xd.z = 0;
      pts_sec.add(xd);

      v_sec[v_sec.size() - 1] = r * cos(theta) * e1 + r * sin(theta) * e2 +
                                vi;  // init velocity of new particle
      Real temp = (KE + TA + WC) / 3;
      l_sec[l_sec.size() - 1] = ((lMax - lMin) * temp) + lMin +
                                mRand.getReal() * 0.1;  // init lifetime of new particle

      // init type of new particle
      if (neighborRatio(i, j, k) < c_s) {
        pts_sec[pts_sec.size() - 1].flag = ParticleBase::PSPRAY;
      }
      else if (neighborRatio(i, j, k) > c_b) {
        pts_sec[pts_sec.size() - 1].flag = ParticleBase::PBUBBLE;
      }
      else {
        pts_sec[pts_sec.size() - 1].flag = ParticleBase::PFOAM;
      }
    }
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const MACGrid &getArg1()
  {
    return v;
  }
  typedef MACGrid type1;
  inline BasicParticleSystem &getArg2()
  {
    return pts_sec;
  }
  typedef BasicParticleSystem type2;
  inline ParticleDataImpl<Vec3> &getArg3()
  {
    return v_sec;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline ParticleDataImpl<Real> &getArg4()
  {
    return l_sec;
  }
  typedef ParticleDataImpl<Real> type4;
  inline const Real &getArg5()
  {
    return lMin;
  }
  typedef Real type5;
  inline const Real &getArg6()
  {
    return lMax;
  }
  typedef Real type6;
  inline const Grid<Real> &getArg7()
  {
    return potTA;
  }
  typedef Grid<Real> type7;
  inline const Grid<Real> &getArg8()
  {
    return potWC;
  }
  typedef Grid<Real> type8;
  inline const Grid<Real> &getArg9()
  {
    return potKE;
  }
  typedef Grid<Real> type9;
  inline const Grid<Real> &getArg10()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type10;
  inline const Real &getArg11()
  {
    return c_s;
  }
  typedef Real type11;
  inline const Real &getArg12()
  {
    return c_b;
  }
  typedef Real type12;
  inline const Real &getArg13()
  {
    return k_ta;
  }
  typedef Real type13;
  inline const Real &getArg14()
  {
    return k_wc;
  }
  typedef Real type14;
  inline const Real &getArg15()
  {
    return dt;
  }
  typedef Real type15;
  inline const int &getArg16()
  {
    return itype;
  }
  typedef int type16;
  void runMessage()
  {
    debMsg("Executing kernel knFlipSampleSecondaryParticles ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void run()
  {
    const int _maxX = maxX;
    const int _maxY = maxY;
    for (int k = minZ; k < maxZ; k++)
      for (int j = 0; j < _maxY; j++)
        for (int i = 0; i < _maxX; i++)
          op(i,
             j,
             k,
             flags,
             v,
             pts_sec,
             v_sec,
             l_sec,
             lMin,
             lMax,
             potTA,
             potWC,
             potKE,
             neighborRatio,
             c_s,
             c_b,
             k_ta,
             k_wc,
             dt,
             itype);
  }
  const FlagGrid &flags;
  const MACGrid &v;
  BasicParticleSystem &pts_sec;
  ParticleDataImpl<Vec3> &v_sec;
  ParticleDataImpl<Real> &l_sec;
  const Real lMin;
  const Real lMax;
  const Grid<Real> &potTA;
  const Grid<Real> &potWC;
  const Grid<Real> &potKE;
  const Grid<Real> &neighborRatio;
  const Real c_s;
  const Real c_b;
  const Real k_ta;
  const Real k_wc;
  const Real dt;
  const int itype;
};

void flipSampleSecondaryParticles(const std::string mode,
                                  const FlagGrid &flags,
                                  const MACGrid &v,
                                  BasicParticleSystem &pts_sec,
                                  ParticleDataImpl<Vec3> &v_sec,
                                  ParticleDataImpl<Real> &l_sec,
                                  const Real lMin,
                                  const Real lMax,
                                  const Grid<Real> &potTA,
                                  const Grid<Real> &potWC,
                                  const Grid<Real> &potKE,
                                  const Grid<Real> &neighborRatio,
                                  const Real c_s,
                                  const Real c_b,
                                  const Real k_ta,
                                  const Real k_wc,
                                  const Real dt = 0,
                                  const int itype = FlagGrid::TypeFluid)
{

  float timestep = dt;
  if (dt <= 0)
    timestep = flags.getParent()->getDt();

  if (mode == "single") {
    knFlipSampleSecondaryParticles(flags,
                                   v,
                                   pts_sec,
                                   v_sec,
                                   l_sec,
                                   lMin,
                                   lMax,
                                   potTA,
                                   potWC,
                                   potKE,
                                   neighborRatio,
                                   c_s,
                                   c_b,
                                   k_ta,
                                   k_wc,
                                   timestep,
                                   itype);
  }
  else if (mode == "multiple") {
    knFlipSampleSecondaryParticlesMoreCylinders(flags,
                                                v,
                                                pts_sec,
                                                v_sec,
                                                l_sec,
                                                lMin,
                                                lMax,
                                                potTA,
                                                potWC,
                                                potKE,
                                                neighborRatio,
                                                c_s,
                                                c_b,
                                                k_ta,
                                                k_wc,
                                                timestep,
                                                itype);
  }
  else {
    throw std::invalid_argument("Unknown mode: use \"single\" or \"multiple\" instead!");
  }
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipSampleSecondaryParticles", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const std::string mode = _args.get<std::string>("mode", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 2, &_lock);
      BasicParticleSystem &pts_sec = *_args.getPtr<BasicParticleSystem>("pts_sec", 3, &_lock);
      ParticleDataImpl<Vec3> &v_sec = *_args.getPtr<ParticleDataImpl<Vec3>>("v_sec", 4, &_lock);
      ParticleDataImpl<Real> &l_sec = *_args.getPtr<ParticleDataImpl<Real>>("l_sec", 5, &_lock);
      const Real lMin = _args.get<Real>("lMin", 6, &_lock);
      const Real lMax = _args.get<Real>("lMax", 7, &_lock);
      const Grid<Real> &potTA = *_args.getPtr<Grid<Real>>("potTA", 8, &_lock);
      const Grid<Real> &potWC = *_args.getPtr<Grid<Real>>("potWC", 9, &_lock);
      const Grid<Real> &potKE = *_args.getPtr<Grid<Real>>("potKE", 10, &_lock);
      const Grid<Real> &neighborRatio = *_args.getPtr<Grid<Real>>("neighborRatio", 11, &_lock);
      const Real c_s = _args.get<Real>("c_s", 12, &_lock);
      const Real c_b = _args.get<Real>("c_b", 13, &_lock);
      const Real k_ta = _args.get<Real>("k_ta", 14, &_lock);
      const Real k_wc = _args.get<Real>("k_wc", 15, &_lock);
      const Real dt = _args.getOpt<Real>("dt", 16, 0, &_lock);
      const int itype = _args.getOpt<int>("itype", 17, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      flipSampleSecondaryParticles(mode,
                                   flags,
                                   v,
                                   pts_sec,
                                   v_sec,
                                   l_sec,
                                   lMin,
                                   lMax,
                                   potTA,
                                   potWC,
                                   potKE,
                                   neighborRatio,
                                   c_s,
                                   c_b,
                                   k_ta,
                                   k_wc,
                                   dt,
                                   itype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipSampleSecondaryParticles", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipSampleSecondaryParticles", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipSampleSecondaryParticles("",
                                                           "flipSampleSecondaryParticles",
                                                           _W_1);
extern "C" {
void PbRegister_flipSampleSecondaryParticles()
{
  KEEP_UNUSED(_RP_flipSampleSecondaryParticles);
}
}

// evaluates cubic spline with radius h and distance l in dim dimensions
Real cubicSpline(const Real h, const Real l, const int dim)
{
  const Real h2 = square(h), h3 = h2 * h;
  const Real c[] = {
      Real(2e0 / (3e0 * h)), Real(10e0 / (7e0 * M_PI * h2)), Real(1e0 / (M_PI * h3))};
  const Real q = l / h;
  if (q < 1e0)
    return c[dim - 1] * (1e0 - 1.5 * square(q) + 0.75 * cubed(q));
  else if (q < 2e0)
    return c[dim - 1] * (0.25 * cubed(2e0 - q));
  return 0;
}

// updates position &pts_sec.pos and velocity &v_sec of secondary particles according to the
// particle type determined by the neighbor ratio with linear interpolation

struct knFlipUpdateSecondaryParticlesLinear : public KernelBase {
  knFlipUpdateSecondaryParticlesLinear(BasicParticleSystem &pts_sec,
                                       ParticleDataImpl<Vec3> &v_sec,
                                       ParticleDataImpl<Real> &l_sec,
                                       const ParticleDataImpl<Vec3> &f_sec,
                                       const FlagGrid &flags,
                                       const MACGrid &v,
                                       const Grid<Real> &neighborRatio,
                                       const Vec3 gravity,
                                       const Real k_b,
                                       const Real k_d,
                                       const Real c_s,
                                       const Real c_b,
                                       const Real dt,
                                       const int exclude,
                                       const int antitunneling)
      : KernelBase(pts_sec.size()),
        pts_sec(pts_sec),
        v_sec(v_sec),
        l_sec(l_sec),
        f_sec(f_sec),
        flags(flags),
        v(v),
        neighborRatio(neighborRatio),
        gravity(gravity),
        k_b(k_b),
        k_d(k_d),
        c_s(c_s),
        c_b(c_b),
        dt(dt),
        exclude(exclude),
        antitunneling(antitunneling)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystem &pts_sec,
                 ParticleDataImpl<Vec3> &v_sec,
                 ParticleDataImpl<Real> &l_sec,
                 const ParticleDataImpl<Vec3> &f_sec,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const Grid<Real> &neighborRatio,
                 const Vec3 gravity,
                 const Real k_b,
                 const Real k_d,
                 const Real c_s,
                 const Real c_b,
                 const Real dt,
                 const int exclude,
                 const int antitunneling) const
  {

    if (!pts_sec.isActive(idx) || pts_sec[idx].flag & exclude)
      return;
    if (!flags.isInBounds(pts_sec[idx].pos)) {
      pts_sec.kill(idx);
      return;
    }

    Vec3i gridpos = toVec3i(pts_sec[idx].pos);

    // spray particle
    if (neighborRatio(gridpos) < c_s) {
      pts_sec[idx].flag |= ParticleBase::PSPRAY;
      pts_sec[idx].flag &= ~(ParticleBase::PBUBBLE | ParticleBase::PFOAM);
      v_sec[idx] += dt *
                    ((f_sec[idx] / 1) + gravity);  // TODO: if forces are added (e.g. fluid
                                                   // guiding), add parameter for mass instead of 1

      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos +
                                ct * (1 / Real(antitunneling)) * dt * v_sec[idx]);
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * v_sec[idx];
    }

    // air bubble particle
    else if (neighborRatio(gridpos) > c_b) {
      pts_sec[idx].flag |= ParticleBase::PBUBBLE;
      pts_sec[idx].flag &= ~(ParticleBase::PSPRAY | ParticleBase::PFOAM);

      const Vec3 vj = (v.getInterpolated(pts_sec[idx].pos) - v_sec[idx]) / dt;
      v_sec[idx] += dt * (k_b * -gravity + k_d * vj);

      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos +
                                ct * (1 / Real(antitunneling)) * dt * v_sec[idx]);
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * v_sec[idx];
    }

    // foam particle
    else {
      pts_sec[idx].flag |= ParticleBase::PFOAM;
      pts_sec[idx].flag &= ~(ParticleBase::PBUBBLE | ParticleBase::PSPRAY);

      const Vec3 vj = v.getInterpolated(pts_sec[idx].pos);
      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos + ct * (1 / Real(antitunneling)) * dt * vj);
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * v.getInterpolated(pts_sec[idx].pos);
    }

    // lifetime
    l_sec[idx] -= dt;
    if (l_sec[idx] <= Real(0)) {
      pts_sec.kill(idx);
    }
  }
  inline BasicParticleSystem &getArg0()
  {
    return pts_sec;
  }
  typedef BasicParticleSystem type0;
  inline ParticleDataImpl<Vec3> &getArg1()
  {
    return v_sec;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline ParticleDataImpl<Real> &getArg2()
  {
    return l_sec;
  }
  typedef ParticleDataImpl<Real> type2;
  inline const ParticleDataImpl<Vec3> &getArg3()
  {
    return f_sec;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline const FlagGrid &getArg4()
  {
    return flags;
  }
  typedef FlagGrid type4;
  inline const MACGrid &getArg5()
  {
    return v;
  }
  typedef MACGrid type5;
  inline const Grid<Real> &getArg6()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type6;
  inline const Vec3 &getArg7()
  {
    return gravity;
  }
  typedef Vec3 type7;
  inline const Real &getArg8()
  {
    return k_b;
  }
  typedef Real type8;
  inline const Real &getArg9()
  {
    return k_d;
  }
  typedef Real type9;
  inline const Real &getArg10()
  {
    return c_s;
  }
  typedef Real type10;
  inline const Real &getArg11()
  {
    return c_b;
  }
  typedef Real type11;
  inline const Real &getArg12()
  {
    return dt;
  }
  typedef Real type12;
  inline const int &getArg13()
  {
    return exclude;
  }
  typedef int type13;
  inline const int &getArg14()
  {
    return antitunneling;
  }
  typedef int type14;
  void runMessage()
  {
    debMsg("Executing kernel knFlipUpdateSecondaryParticlesLinear ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx,
         pts_sec,
         v_sec,
         l_sec,
         f_sec,
         flags,
         v,
         neighborRatio,
         gravity,
         k_b,
         k_d,
         c_s,
         c_b,
         dt,
         exclude,
         antitunneling);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystem &pts_sec;
  ParticleDataImpl<Vec3> &v_sec;
  ParticleDataImpl<Real> &l_sec;
  const ParticleDataImpl<Vec3> &f_sec;
  const FlagGrid &flags;
  const MACGrid &v;
  const Grid<Real> &neighborRatio;
  const Vec3 gravity;
  const Real k_b;
  const Real k_d;
  const Real c_s;
  const Real c_b;
  const Real dt;
  const int exclude;
  const int antitunneling;
};
// updates position &pts_sec.pos and velocity &v_sec of secondary particles according to the
// particle type determined by the neighbor ratio with cubic spline interpolation

struct knFlipUpdateSecondaryParticlesCubic : public KernelBase {
  knFlipUpdateSecondaryParticlesCubic(BasicParticleSystem &pts_sec,
                                      ParticleDataImpl<Vec3> &v_sec,
                                      ParticleDataImpl<Real> &l_sec,
                                      const ParticleDataImpl<Vec3> &f_sec,
                                      const FlagGrid &flags,
                                      const MACGrid &v,
                                      const Grid<Real> &neighborRatio,
                                      const int radius,
                                      const Vec3 gravity,
                                      const Real k_b,
                                      const Real k_d,
                                      const Real c_s,
                                      const Real c_b,
                                      const Real dt,
                                      const int exclude,
                                      const int antitunneling,
                                      const int itype)
      : KernelBase(pts_sec.size()),
        pts_sec(pts_sec),
        v_sec(v_sec),
        l_sec(l_sec),
        f_sec(f_sec),
        flags(flags),
        v(v),
        neighborRatio(neighborRatio),
        radius(radius),
        gravity(gravity),
        k_b(k_b),
        k_d(k_d),
        c_s(c_s),
        c_b(c_b),
        dt(dt),
        exclude(exclude),
        antitunneling(antitunneling),
        itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystem &pts_sec,
                 ParticleDataImpl<Vec3> &v_sec,
                 ParticleDataImpl<Real> &l_sec,
                 const ParticleDataImpl<Vec3> &f_sec,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const Grid<Real> &neighborRatio,
                 const int radius,
                 const Vec3 gravity,
                 const Real k_b,
                 const Real k_d,
                 const Real c_s,
                 const Real c_b,
                 const Real dt,
                 const int exclude,
                 const int antitunneling,
                 const int itype) const
  {

    if (!pts_sec.isActive(idx) || pts_sec[idx].flag & exclude)
      return;
    if (!flags.isInBounds(pts_sec[idx].pos)) {
      pts_sec.kill(idx);
      return;
    }

    Vec3i gridpos = toVec3i(pts_sec[idx].pos);
    int i = gridpos.x;
    int j = gridpos.y;
    int k = gridpos.z;

    // spray particle
    if (neighborRatio(gridpos) < c_s) {
      pts_sec[idx].flag |= ParticleBase::PSPRAY;
      pts_sec[idx].flag &= ~(ParticleBase::PBUBBLE | ParticleBase::PFOAM);
      v_sec[idx] += dt *
                    ((f_sec[idx] / 1) + gravity);  // TODO: if forces are added (e.g. fluid
                                                   // guiding), add parameter for mass instead of 1

      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos +
                                ct * (1 / Real(antitunneling)) * dt * v_sec[idx]);
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * v_sec[idx];
    }

    // air bubble particle
    else if (neighborRatio(gridpos) > c_b) {
      pts_sec[idx].flag |= ParticleBase::PBUBBLE;
      pts_sec[idx].flag &= ~(ParticleBase::PSPRAY | ParticleBase::PFOAM);
      const Vec3 &xi = pts_sec[idx].pos;
      Vec3 sumNumerator = Vec3(0, 0, 0);
      Real sumDenominator = 0;
      for (IndexInt x = i - radius; x <= i + radius; x++) {
        for (IndexInt y = j - radius; y <= j + radius; y++) {
          for (IndexInt z = k - radius; z <= k + radius; z++) {
            Vec3i xj = Vec3i(x, y, z);
            if ((x == i && y == j && z == k) || !flags.isInBounds(xj))
              continue;
            if (!(flags(xj) & itype))
              continue;
            const Real len_xij = norm(xi - Vec3(x, y, z));

            int dim = flags.is3D() ? 3 : 2;
            Real dist = flags.is3D() ? 1.732 : 1.414;
            Real weight = cubicSpline(radius * dist, len_xij, dim);
            sumNumerator += v.getCentered(xj) *
                            weight;  // estimate next position by current velocity
            sumDenominator += weight;
          }
        }
      }
      const Vec3 temp = ((sumNumerator / sumDenominator) - v_sec[idx]) / dt;
      v_sec[idx] += dt * (k_b * -gravity + k_d * temp);

      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos +
                                ct * (1 / Real(antitunneling)) * dt * v_sec[idx]);
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * v_sec[idx];
    }

    // foam particle
    else {
      pts_sec[idx].flag |= ParticleBase::PFOAM;
      pts_sec[idx].flag &= ~(ParticleBase::PBUBBLE | ParticleBase::PSPRAY);
      const Vec3 &xi = pts_sec[idx].pos;
      Vec3 sumNumerator = Vec3(0, 0, 0);
      Real sumDenominator = 0;
      for (IndexInt x = i - radius; x <= i + radius; x++) {
        for (IndexInt y = j - radius; y <= j + radius; y++) {
          for (IndexInt z = k - radius; z <= k + radius; z++) {
            Vec3i xj = Vec3i(x, y, z);
            if ((x == i && y == j && z == k) || !flags.isInBounds(xj))
              continue;
            if (!(flags(xj) & itype))
              continue;
            const Real len_xij = norm(xi - Vec3(x, y, z));

            int dim = flags.is3D() ? 3 : 2;
            Real dist = flags.is3D() ? 1.732 : 1.414;
            Real weight = cubicSpline(radius * dist, len_xij, dim);
            sumNumerator += v.getCentered(xj) *
                            weight;  // estimate next position by current velocity
            sumDenominator += weight;
          }
        }
      }

      // anti tunneling for small obstacles
      for (int ct = 1; ct < antitunneling; ct++) {
        Vec3i tempPos = toVec3i(pts_sec[idx].pos + ct * (1 / Real(antitunneling)) * dt *
                                                       (sumNumerator / sumDenominator));
        if (!flags.isInBounds(tempPos) || flags(tempPos) & FlagGrid::TypeObstacle) {
          pts_sec.kill(idx);
          return;
        }
      }
      pts_sec[idx].pos += dt * (sumNumerator / sumDenominator);
    }

    // lifetime
    l_sec[idx] -= dt;
    if (l_sec[idx] <= Real(0)) {
      pts_sec.kill(idx);
    }
  }
  inline BasicParticleSystem &getArg0()
  {
    return pts_sec;
  }
  typedef BasicParticleSystem type0;
  inline ParticleDataImpl<Vec3> &getArg1()
  {
    return v_sec;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline ParticleDataImpl<Real> &getArg2()
  {
    return l_sec;
  }
  typedef ParticleDataImpl<Real> type2;
  inline const ParticleDataImpl<Vec3> &getArg3()
  {
    return f_sec;
  }
  typedef ParticleDataImpl<Vec3> type3;
  inline const FlagGrid &getArg4()
  {
    return flags;
  }
  typedef FlagGrid type4;
  inline const MACGrid &getArg5()
  {
    return v;
  }
  typedef MACGrid type5;
  inline const Grid<Real> &getArg6()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type6;
  inline const int &getArg7()
  {
    return radius;
  }
  typedef int type7;
  inline const Vec3 &getArg8()
  {
    return gravity;
  }
  typedef Vec3 type8;
  inline const Real &getArg9()
  {
    return k_b;
  }
  typedef Real type9;
  inline const Real &getArg10()
  {
    return k_d;
  }
  typedef Real type10;
  inline const Real &getArg11()
  {
    return c_s;
  }
  typedef Real type11;
  inline const Real &getArg12()
  {
    return c_b;
  }
  typedef Real type12;
  inline const Real &getArg13()
  {
    return dt;
  }
  typedef Real type13;
  inline const int &getArg14()
  {
    return exclude;
  }
  typedef int type14;
  inline const int &getArg15()
  {
    return antitunneling;
  }
  typedef int type15;
  inline const int &getArg16()
  {
    return itype;
  }
  typedef int type16;
  void runMessage()
  {
    debMsg("Executing kernel knFlipUpdateSecondaryParticlesCubic ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx,
         pts_sec,
         v_sec,
         l_sec,
         f_sec,
         flags,
         v,
         neighborRatio,
         radius,
         gravity,
         k_b,
         k_d,
         c_s,
         c_b,
         dt,
         exclude,
         antitunneling,
         itype);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystem &pts_sec;
  ParticleDataImpl<Vec3> &v_sec;
  ParticleDataImpl<Real> &l_sec;
  const ParticleDataImpl<Vec3> &f_sec;
  const FlagGrid &flags;
  const MACGrid &v;
  const Grid<Real> &neighborRatio;
  const int radius;
  const Vec3 gravity;
  const Real k_b;
  const Real k_d;
  const Real c_s;
  const Real c_b;
  const Real dt;
  const int exclude;
  const int antitunneling;
  const int itype;
};

void flipUpdateSecondaryParticles(const std::string mode,
                                  BasicParticleSystem &pts_sec,
                                  ParticleDataImpl<Vec3> &v_sec,
                                  ParticleDataImpl<Real> &l_sec,
                                  const ParticleDataImpl<Vec3> &f_sec,
                                  FlagGrid &flags,
                                  const MACGrid &v,
                                  const Grid<Real> &neighborRatio,
                                  const int radius,
                                  const Vec3 gravity,
                                  const Real k_b,
                                  const Real k_d,
                                  const Real c_s,
                                  const Real c_b,
                                  const Real dt = 0,
                                  bool scale = true,
                                  const int exclude = ParticleBase::PTRACER,
                                  const int antitunneling = 0,
                                  const int itype = FlagGrid::TypeFluid)
{

  float gridScale = (scale) ? flags.getParent()->getDx() : 1;
  Vec3 g = gravity / gridScale;

  float timestep = dt;
  if (dt <= 0)
    timestep = flags.getParent()->getDt();

  if (mode == "linear") {
    knFlipUpdateSecondaryParticlesLinear(pts_sec,
                                         v_sec,
                                         l_sec,
                                         f_sec,
                                         flags,
                                         v,
                                         neighborRatio,
                                         g,
                                         k_b,
                                         k_d,
                                         c_s,
                                         c_b,
                                         timestep,
                                         exclude,
                                         antitunneling);
  }
  else if (mode == "cubic") {
    knFlipUpdateSecondaryParticlesCubic(pts_sec,
                                        v_sec,
                                        l_sec,
                                        f_sec,
                                        flags,
                                        v,
                                        neighborRatio,
                                        radius,
                                        g,
                                        k_b,
                                        k_d,
                                        c_s,
                                        c_b,
                                        timestep,
                                        exclude,
                                        antitunneling,
                                        itype);
  }
  else {
    throw std::invalid_argument("Unknown mode: use \"linear\" or \"cubic\" instead!");
  }
  pts_sec.doCompress();
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipUpdateSecondaryParticles", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const std::string mode = _args.get<std::string>("mode", 0, &_lock);
      BasicParticleSystem &pts_sec = *_args.getPtr<BasicParticleSystem>("pts_sec", 1, &_lock);
      ParticleDataImpl<Vec3> &v_sec = *_args.getPtr<ParticleDataImpl<Vec3>>("v_sec", 2, &_lock);
      ParticleDataImpl<Real> &l_sec = *_args.getPtr<ParticleDataImpl<Real>>("l_sec", 3, &_lock);
      const ParticleDataImpl<Vec3> &f_sec = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "f_sec", 4, &_lock);
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 5, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 6, &_lock);
      const Grid<Real> &neighborRatio = *_args.getPtr<Grid<Real>>("neighborRatio", 7, &_lock);
      const int radius = _args.get<int>("radius", 8, &_lock);
      const Vec3 gravity = _args.get<Vec3>("gravity", 9, &_lock);
      const Real k_b = _args.get<Real>("k_b", 10, &_lock);
      const Real k_d = _args.get<Real>("k_d", 11, &_lock);
      const Real c_s = _args.get<Real>("c_s", 12, &_lock);
      const Real c_b = _args.get<Real>("c_b", 13, &_lock);
      const Real dt = _args.getOpt<Real>("dt", 14, 0, &_lock);
      bool scale = _args.getOpt<bool>("scale", 15, true, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 16, ParticleBase::PTRACER, &_lock);
      const int antitunneling = _args.getOpt<int>("antitunneling", 17, 0, &_lock);
      const int itype = _args.getOpt<int>("itype", 18, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      flipUpdateSecondaryParticles(mode,
                                   pts_sec,
                                   v_sec,
                                   l_sec,
                                   f_sec,
                                   flags,
                                   v,
                                   neighborRatio,
                                   radius,
                                   gravity,
                                   k_b,
                                   k_d,
                                   c_s,
                                   c_b,
                                   dt,
                                   scale,
                                   exclude,
                                   antitunneling,
                                   itype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipUpdateSecondaryParticles", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipUpdateSecondaryParticles", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipUpdateSecondaryParticles("",
                                                           "flipUpdateSecondaryParticles",
                                                           _W_2);
extern "C" {
void PbRegister_flipUpdateSecondaryParticles()
{
  KEEP_UNUSED(_RP_flipUpdateSecondaryParticles);
}
}

// removes secondary particles in &pts_sec that are inside boundaries (cells that are marked as
// obstacle/outflow in &flags)

struct knFlipDeleteParticlesInObstacle : public KernelBase {
  knFlipDeleteParticlesInObstacle(BasicParticleSystem &pts, const FlagGrid &flags)
      : KernelBase(pts.size()), pts(pts), flags(flags)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, BasicParticleSystem &pts, const FlagGrid &flags) const
  {

    if (!pts.isActive(idx))
      return;

    const Vec3 &xi = pts[idx].pos;
    const Vec3i xidx = toVec3i(xi);
    // remove particles that completely left the bounds
    if (!flags.isInBounds(xidx)) {
      pts.kill(idx);
      return;
    }
    int gridIndex = flags.index(xidx);
    // remove particles that penetrate obstacles
    if (flags.isObstacle(gridIndex) || flags.isOutflow(gridIndex)) {
      pts.kill(idx);
    }
  }
  inline BasicParticleSystem &getArg0()
  {
    return pts;
  }
  typedef BasicParticleSystem type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  void runMessage()
  {
    debMsg("Executing kernel knFlipDeleteParticlesInObstacle ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, pts, flags);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystem &pts;
  const FlagGrid &flags;
};

void flipDeleteParticlesInObstacle(BasicParticleSystem &pts, const FlagGrid &flags)
{

  knFlipDeleteParticlesInObstacle(pts, flags);
  pts.doCompress();
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipDeleteParticlesInObstacle", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      BasicParticleSystem &pts = *_args.getPtr<BasicParticleSystem>("pts", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      _retval = getPyNone();
      flipDeleteParticlesInObstacle(pts, flags);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipDeleteParticlesInObstacle", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipDeleteParticlesInObstacle", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipDeleteParticlesInObstacle("",
                                                            "flipDeleteParticlesInObstacle",
                                                            _W_3);
extern "C" {
void PbRegister_flipDeleteParticlesInObstacle()
{
  KEEP_UNUSED(_RP_flipDeleteParticlesInObstacle);
}
}

// helper method to debug statistical data from grid

void debugGridInfo(const FlagGrid &flags,
                   Grid<Real> &grid,
                   std::string name,
                   const int itype = FlagGrid::TypeFluid)
{
  FluidSolver *s = flags.getParent();
  int countFluid = 0;
  int countLargerZero = 0;
  Real avg = 0;
  Real max = 0;
  Real sum = 0;
  Real avgLargerZero = 0;
  FOR_IJK_BND(grid, 1)
  {
    if (!(flags(i, j, k) & itype))
      continue;
    countFluid++;
    if (grid(i, j, k) > 0)
      countLargerZero++;
    sum += grid(i, j, k);
    if (grid(i, j, k) > max)
      max = grid(i, j, k);
  }
  avg = sum / std::max(Real(countFluid), Real(1));
  avgLargerZero = sum / std::max(Real(countLargerZero), Real(1));

  debMsg("Step: " << s->mFrame << " - Grid " << name << "\n\tcountFluid \t\t" << countFluid
                  << "\n\tcountLargerZero \t" << countLargerZero << "\n\tsum \t\t\t" << sum
                  << "\n\tavg \t\t\t" << avg << "\n\tavgLargerZero \t\t" << avgLargerZero
                  << "\n\tmax \t\t\t" << max,
         1);
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "debugGridInfo", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &grid = *_args.getPtr<Grid<Real>>("grid", 1, &_lock);
      std::string name = _args.get<std::string>("name", 2, &_lock);
      const int itype = _args.getOpt<int>("itype", 3, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      debugGridInfo(flags, grid, name, itype);
      _args.check();
    }
    pbFinalizePlugin(parent, "debugGridInfo", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("debugGridInfo", e.what());
    return 0;
  }
}
static const Pb::Register _RP_debugGridInfo("", "debugGridInfo", _W_4);
extern "C" {
void PbRegister_debugGridInfo()
{
  KEEP_UNUSED(_RP_debugGridInfo);
}
}

// The following methods are helper functions to recreate the velocity and flag grid from the
// underlying FLIP simulation. They cannot simply be loaded because of the upres to a higher
// resolution, instead a levelset is used.

struct knSetFlagsFromLevelset : public KernelBase {
  knSetFlagsFromLevelset(FlagGrid &flags,
                         const Grid<Real> &phi,
                         const int exclude = FlagGrid::TypeObstacle,
                         const int itype = FlagGrid::TypeFluid)
      : KernelBase(&flags, 0), flags(flags), phi(phi), exclude(exclude), itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 FlagGrid &flags,
                 const Grid<Real> &phi,
                 const int exclude = FlagGrid::TypeObstacle,
                 const int itype = FlagGrid::TypeFluid) const
  {
    if (phi(idx) < 0 && !(flags(idx) & exclude))
      flags(idx) = itype;
  }
  inline FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline const Grid<Real> &getArg1()
  {
    return phi;
  }
  typedef Grid<Real> type1;
  inline const int &getArg2()
  {
    return exclude;
  }
  typedef int type2;
  inline const int &getArg3()
  {
    return itype;
  }
  typedef int type3;
  void runMessage()
  {
    debMsg("Executing kernel knSetFlagsFromLevelset ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, flags, phi, exclude, itype);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  FlagGrid &flags;
  const Grid<Real> &phi;
  const int exclude;
  const int itype;
};

void setFlagsFromLevelset(FlagGrid &flags,
                          const Grid<Real> &phi,
                          const int exclude = FlagGrid::TypeObstacle,
                          const int itype = FlagGrid::TypeFluid)
{
  knSetFlagsFromLevelset(flags, phi, exclude, itype);
}
static PyObject *_W_5(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setFlagsFromLevelset", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      const Grid<Real> &phi = *_args.getPtr<Grid<Real>>("phi", 1, &_lock);
      const int exclude = _args.getOpt<int>("exclude", 2, FlagGrid::TypeObstacle, &_lock);
      const int itype = _args.getOpt<int>("itype", 3, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      setFlagsFromLevelset(flags, phi, exclude, itype);
      _args.check();
    }
    pbFinalizePlugin(parent, "setFlagsFromLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setFlagsFromLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setFlagsFromLevelset("", "setFlagsFromLevelset", _W_5);
extern "C" {
void PbRegister_setFlagsFromLevelset()
{
  KEEP_UNUSED(_RP_setFlagsFromLevelset);
}
}

struct knSetMACFromLevelset : public KernelBase {
  knSetMACFromLevelset(MACGrid &v, const Grid<Real> &phi, const Vec3 c)
      : KernelBase(&v, 0), v(v), phi(phi), c(c)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, MACGrid &v, const Grid<Real> &phi, const Vec3 c) const
  {
    if (phi.getInterpolated(Vec3(i, j, k)) > 0)
      v(i, j, k) = c;
  }
  inline MACGrid &getArg0()
  {
    return v;
  }
  typedef MACGrid type0;
  inline const Grid<Real> &getArg1()
  {
    return phi;
  }
  typedef Grid<Real> type1;
  inline const Vec3 &getArg2()
  {
    return c;
  }
  typedef Vec3 type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetMACFromLevelset ", 3);
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
            op(i, j, k, v, phi, c);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, v, phi, c);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid &v;
  const Grid<Real> &phi;
  const Vec3 c;
};

void setMACFromLevelset(MACGrid &v, const Grid<Real> &phi, const Vec3 c)
{
  knSetMACFromLevelset(v, phi, c);
}
static PyObject *_W_6(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "setMACFromLevelset", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      MACGrid &v = *_args.getPtr<MACGrid>("v", 0, &_lock);
      const Grid<Real> &phi = *_args.getPtr<Grid<Real>>("phi", 1, &_lock);
      const Vec3 c = _args.get<Vec3>("c", 2, &_lock);
      _retval = getPyNone();
      setMACFromLevelset(v, phi, c);
      _args.check();
    }
    pbFinalizePlugin(parent, "setMACFromLevelset", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("setMACFromLevelset", e.what());
    return 0;
  }
}
static const Pb::Register _RP_setMACFromLevelset("", "setMACFromLevelset", _W_6);
extern "C" {
void PbRegister_setMACFromLevelset()
{
  KEEP_UNUSED(_RP_setMACFromLevelset);
}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
// END Secondary Particles for FLIP
//----------------------------------------------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Legacy Methods(still useful for debugging)
//-----------------------------------------------------------------------------------------------------------------------------------
//-----------------
// Legacy Methods (still useful for debugging)
//----------------------------------------------------------------------------------------------------------------------------------------------------

// LEGACY METHOD! Use flipComputeSecondaryParticlePotentials instead!
// computes trapped air potential for all fluid cells in &flags and saves it in &pot

struct knFlipComputePotentialTrappedAir : public KernelBase {
  knFlipComputePotentialTrappedAir(Grid<Real> &pot,
                                   const FlagGrid &flags,
                                   const MACGrid &v,
                                   const int radius,
                                   const Real tauMin,
                                   const Real tauMax,
                                   const Real scaleFromManta,
                                   const int itype = FlagGrid::TypeFluid,
                                   const int jtype = FlagGrid::TypeFluid)
      : KernelBase(&pot, 1),
        pot(pot),
        flags(flags),
        v(v),
        radius(radius),
        tauMin(tauMin),
        tauMax(tauMax),
        scaleFromManta(scaleFromManta),
        itype(itype),
        jtype(jtype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &pot,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const int radius,
                 const Real tauMin,
                 const Real tauMax,
                 const Real scaleFromManta,
                 const int itype = FlagGrid::TypeFluid,
                 const int jtype = FlagGrid::TypeFluid) const
  {

    if (!(flags(i, j, k) & itype))
      return;

    const Vec3 &xi = scaleFromManta * Vec3(i, j, k);  // scale to unit cube
    const Vec3 &vi = scaleFromManta * v.getCentered(i, j, k);
    Real vdiff = 0;
    for (IndexInt x = i - radius; x <= i + radius; x++) {
      for (IndexInt y = j - radius; y <= j + radius; y++) {
        for (IndexInt z = k - radius; z <= k + radius; z++) {
          if ((x == i && y == j && z == k) || !(flags(x, y, z) & jtype))
            continue;

          const Vec3 &xj = scaleFromManta * Vec3(x, y, z);  // scale to unit cube
          const Vec3 &vj = scaleFromManta * v.getCentered(x, y, z);
          const Vec3 xij = xi - xj;
          const Vec3 vij = vi - vj;
          Real h = !pot.is3D() ? 1.414 * radius :
                                 1.732 * radius;  // estimate sqrt(2)*radius resp. sqrt(3)*radius
                                                  // for h, due to squared resp. cubic neighbor area
          vdiff += norm(vij) * (1 - dot(getNormalized(vij), getNormalized(xij))) *
                   (1 - norm(xij) / h);
        }
      }
    }
    pot(i, j, k) = (std::min(vdiff, tauMax) - std::min(vdiff, tauMin)) / (tauMax - tauMin);
  }
  inline Grid<Real> &getArg0()
  {
    return pot;
  }
  typedef Grid<Real> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return v;
  }
  typedef MACGrid type2;
  inline const int &getArg3()
  {
    return radius;
  }
  typedef int type3;
  inline const Real &getArg4()
  {
    return tauMin;
  }
  typedef Real type4;
  inline const Real &getArg5()
  {
    return tauMax;
  }
  typedef Real type5;
  inline const Real &getArg6()
  {
    return scaleFromManta;
  }
  typedef Real type6;
  inline const int &getArg7()
  {
    return itype;
  }
  typedef int type7;
  inline const int &getArg8()
  {
    return jtype;
  }
  typedef int type8;
  void runMessage()
  {
    debMsg("Executing kernel knFlipComputePotentialTrappedAir ", 3);
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
            op(i, j, k, pot, flags, v, radius, tauMin, tauMax, scaleFromManta, itype, jtype);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, pot, flags, v, radius, tauMin, tauMax, scaleFromManta, itype, jtype);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &pot;
  const FlagGrid &flags;
  const MACGrid &v;
  const int radius;
  const Real tauMin;
  const Real tauMax;
  const Real scaleFromManta;
  const int itype;
  const int jtype;
};

void flipComputePotentialTrappedAir(Grid<Real> &pot,
                                    const FlagGrid &flags,
                                    const MACGrid &v,
                                    const int radius,
                                    const Real tauMin,
                                    const Real tauMax,
                                    const Real scaleFromManta,
                                    const int itype = FlagGrid::TypeFluid,
                                    const int jtype = FlagGrid::TypeFluid)
{
  pot.clear();
  knFlipComputePotentialTrappedAir(
      pot, flags, v, radius, tauMin, tauMax, scaleFromManta, itype, jtype);
}
static PyObject *_W_7(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipComputePotentialTrappedAir", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &pot = *_args.getPtr<Grid<Real>>("pot", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 2, &_lock);
      const int radius = _args.get<int>("radius", 3, &_lock);
      const Real tauMin = _args.get<Real>("tauMin", 4, &_lock);
      const Real tauMax = _args.get<Real>("tauMax", 5, &_lock);
      const Real scaleFromManta = _args.get<Real>("scaleFromManta", 6, &_lock);
      const int itype = _args.getOpt<int>("itype", 7, FlagGrid::TypeFluid, &_lock);
      const int jtype = _args.getOpt<int>("jtype", 8, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      flipComputePotentialTrappedAir(
          pot, flags, v, radius, tauMin, tauMax, scaleFromManta, itype, jtype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipComputePotentialTrappedAir", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipComputePotentialTrappedAir", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipComputePotentialTrappedAir("",
                                                             "flipComputePotentialTrappedAir",
                                                             _W_7);
extern "C" {
void PbRegister_flipComputePotentialTrappedAir()
{
  KEEP_UNUSED(_RP_flipComputePotentialTrappedAir);
}
}

// LEGACY METHOD! Use flipComputeSecondaryParticlePotentials instead!
// computes kinetic energy potential for all fluid cells in &flags and saves it in &pot

struct knFlipComputePotentialKineticEnergy : public KernelBase {
  knFlipComputePotentialKineticEnergy(Grid<Real> &pot,
                                      const FlagGrid &flags,
                                      const MACGrid &v,
                                      const Real tauMin,
                                      const Real tauMax,
                                      const Real scaleFromManta,
                                      const int itype = FlagGrid::TypeFluid)
      : KernelBase(&pot, 0),
        pot(pot),
        flags(flags),
        v(v),
        tauMin(tauMin),
        tauMax(tauMax),
        scaleFromManta(scaleFromManta),
        itype(itype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &pot,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const Real tauMin,
                 const Real tauMax,
                 const Real scaleFromManta,
                 const int itype = FlagGrid::TypeFluid) const
  {

    if (!(flags(i, j, k) & itype))
      return;

    const Vec3 &vi = scaleFromManta * v.getCentered(i, j, k);  // scale to unit cube
    Real ek =
        Real(0.5) * 125 *
        normSquare(
            vi);  // use arbitrary constant for mass, potential adjusts with thresholds anyways
    pot(i, j, k) = (std::min(ek, tauMax) - std::min(ek, tauMin)) / (tauMax - tauMin);
  }
  inline Grid<Real> &getArg0()
  {
    return pot;
  }
  typedef Grid<Real> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return v;
  }
  typedef MACGrid type2;
  inline const Real &getArg3()
  {
    return tauMin;
  }
  typedef Real type3;
  inline const Real &getArg4()
  {
    return tauMax;
  }
  typedef Real type4;
  inline const Real &getArg5()
  {
    return scaleFromManta;
  }
  typedef Real type5;
  inline const int &getArg6()
  {
    return itype;
  }
  typedef int type6;
  void runMessage()
  {
    debMsg("Executing kernel knFlipComputePotentialKineticEnergy ", 3);
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
            op(i, j, k, pot, flags, v, tauMin, tauMax, scaleFromManta, itype);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, pot, flags, v, tauMin, tauMax, scaleFromManta, itype);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &pot;
  const FlagGrid &flags;
  const MACGrid &v;
  const Real tauMin;
  const Real tauMax;
  const Real scaleFromManta;
  const int itype;
};

void flipComputePotentialKineticEnergy(Grid<Real> &pot,
                                       const FlagGrid &flags,
                                       const MACGrid &v,
                                       const Real tauMin,
                                       const Real tauMax,
                                       const Real scaleFromManta,
                                       const int itype = FlagGrid::TypeFluid)
{
  pot.clear();
  knFlipComputePotentialKineticEnergy(pot, flags, v, tauMin, tauMax, scaleFromManta, itype);
}
static PyObject *_W_8(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipComputePotentialKineticEnergy", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &pot = *_args.getPtr<Grid<Real>>("pot", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 2, &_lock);
      const Real tauMin = _args.get<Real>("tauMin", 3, &_lock);
      const Real tauMax = _args.get<Real>("tauMax", 4, &_lock);
      const Real scaleFromManta = _args.get<Real>("scaleFromManta", 5, &_lock);
      const int itype = _args.getOpt<int>("itype", 6, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      flipComputePotentialKineticEnergy(pot, flags, v, tauMin, tauMax, scaleFromManta, itype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipComputePotentialKineticEnergy", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipComputePotentialKineticEnergy", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipComputePotentialKineticEnergy(
    "", "flipComputePotentialKineticEnergy", _W_8);
extern "C" {
void PbRegister_flipComputePotentialKineticEnergy()
{
  KEEP_UNUSED(_RP_flipComputePotentialKineticEnergy);
}
}

// LEGACY METHOD! Use flipComputeSecondaryParticlePotentials instead!
// computes wave crest potential for all fluid cells in &flags and saves it in &pot

struct knFlipComputePotentialWaveCrest : public KernelBase {
  knFlipComputePotentialWaveCrest(Grid<Real> &pot,
                                  const FlagGrid &flags,
                                  const MACGrid &v,
                                  const int radius,
                                  Grid<Vec3> &normal,
                                  const Real tauMin,
                                  const Real tauMax,
                                  const Real scaleFromManta,
                                  const int itype = FlagGrid::TypeFluid,
                                  const int jtype = FlagGrid::TypeFluid)
      : KernelBase(&pot, 1),
        pot(pot),
        flags(flags),
        v(v),
        radius(radius),
        normal(normal),
        tauMin(tauMin),
        tauMax(tauMax),
        scaleFromManta(scaleFromManta),
        itype(itype),
        jtype(jtype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<Real> &pot,
                 const FlagGrid &flags,
                 const MACGrid &v,
                 const int radius,
                 Grid<Vec3> &normal,
                 const Real tauMin,
                 const Real tauMax,
                 const Real scaleFromManta,
                 const int itype = FlagGrid::TypeFluid,
                 const int jtype = FlagGrid::TypeFluid) const
  {

    if (!(flags(i, j, k) & itype))
      return;

    const Vec3 &xi = scaleFromManta * Vec3(i, j, k);  // scale to unit cube
    const Vec3 &vi = scaleFromManta * v.getCentered(i, j, k);
    const Vec3 &ni = normal(i, j, k);
    Real kappa = 0;
    for (IndexInt x = i - radius; x <= i + radius; x++) {
      for (IndexInt y = j - radius; y <= j + radius; y++) {
        for (IndexInt z = k - radius; z <= k + radius; z++) {
          if ((x == i && y == j && z == k) || !(flags(x, y, z) & jtype))
            continue;
          const Vec3 &xj = scaleFromManta * Vec3(x, y, z);  // scale to unit cube
          const Vec3 &nj = normal(x, y, z);
          const Vec3 xij = xi - xj;
          if (dot(getNormalized(xij), ni) < 0) {  // identifies wave crests
            Real h = !pot.is3D() ?
                         1.414 * radius :
                         1.732 * radius;  // estimate sqrt(2)*radius resp. sqrt(3)*radius for h,
                                          // due to squared resp. cubic neighbor area
            kappa += (1 - dot(ni, nj)) * (1 - norm(xij) / h);
          }
        }
      }
    }

    if (dot(getNormalized(vi), ni) >= 0.6) {  // avoid to mark boarders of the scene as wave crest
      pot(i, j, k) = (std::min(kappa, tauMax) - std::min(kappa, tauMin)) / (tauMax - tauMin);
    }
    else {
      pot(i, j, k) = Real(0);
    }
  }
  inline Grid<Real> &getArg0()
  {
    return pot;
  }
  typedef Grid<Real> type0;
  inline const FlagGrid &getArg1()
  {
    return flags;
  }
  typedef FlagGrid type1;
  inline const MACGrid &getArg2()
  {
    return v;
  }
  typedef MACGrid type2;
  inline const int &getArg3()
  {
    return radius;
  }
  typedef int type3;
  inline Grid<Vec3> &getArg4()
  {
    return normal;
  }
  typedef Grid<Vec3> type4;
  inline const Real &getArg5()
  {
    return tauMin;
  }
  typedef Real type5;
  inline const Real &getArg6()
  {
    return tauMax;
  }
  typedef Real type6;
  inline const Real &getArg7()
  {
    return scaleFromManta;
  }
  typedef Real type7;
  inline const int &getArg8()
  {
    return itype;
  }
  typedef int type8;
  inline const int &getArg9()
  {
    return jtype;
  }
  typedef int type9;
  void runMessage()
  {
    debMsg("Executing kernel knFlipComputePotentialWaveCrest ", 3);
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
            op(i,
               j,
               k,
               pot,
               flags,
               v,
               radius,
               normal,
               tauMin,
               tauMax,
               scaleFromManta,
               itype,
               jtype);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, pot, flags, v, radius, normal, tauMin, tauMax, scaleFromManta, itype, jtype);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(1, maxY), *this);
  }
  Grid<Real> &pot;
  const FlagGrid &flags;
  const MACGrid &v;
  const int radius;
  Grid<Vec3> &normal;
  const Real tauMin;
  const Real tauMax;
  const Real scaleFromManta;
  const int itype;
  const int jtype;
};

void flipComputePotentialWaveCrest(Grid<Real> &pot,
                                   const FlagGrid &flags,
                                   const MACGrid &v,
                                   const int radius,
                                   Grid<Vec3> &normal,
                                   const Real tauMin,
                                   const Real tauMax,
                                   const Real scaleFromManta,
                                   const int itype = FlagGrid::TypeFluid,
                                   const int jtype = FlagGrid::TypeFluid)
{

  pot.clear();
  knFlipComputePotentialWaveCrest(
      pot, flags, v, radius, normal, tauMin, tauMax, scaleFromManta, itype, jtype);
}
static PyObject *_W_9(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipComputePotentialWaveCrest", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &pot = *_args.getPtr<Grid<Real>>("pot", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      const MACGrid &v = *_args.getPtr<MACGrid>("v", 2, &_lock);
      const int radius = _args.get<int>("radius", 3, &_lock);
      Grid<Vec3> &normal = *_args.getPtr<Grid<Vec3>>("normal", 4, &_lock);
      const Real tauMin = _args.get<Real>("tauMin", 5, &_lock);
      const Real tauMax = _args.get<Real>("tauMax", 6, &_lock);
      const Real scaleFromManta = _args.get<Real>("scaleFromManta", 7, &_lock);
      const int itype = _args.getOpt<int>("itype", 8, FlagGrid::TypeFluid, &_lock);
      const int jtype = _args.getOpt<int>("jtype", 9, FlagGrid::TypeFluid, &_lock);
      _retval = getPyNone();
      flipComputePotentialWaveCrest(
          pot, flags, v, radius, normal, tauMin, tauMax, scaleFromManta, itype, jtype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipComputePotentialWaveCrest", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipComputePotentialWaveCrest", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipComputePotentialWaveCrest("",
                                                            "flipComputePotentialWaveCrest",
                                                            _W_9);
extern "C" {
void PbRegister_flipComputePotentialWaveCrest()
{
  KEEP_UNUSED(_RP_flipComputePotentialWaveCrest);
}
}

// LEGACY METHOD! Use flipComputeSecondaryParticlePotentials instead!
// computes normal grid &normal as gradient of levelset &phi and normalizes it

struct knFlipComputeSurfaceNormals : public KernelBase {
  knFlipComputeSurfaceNormals(Grid<Vec3> &normal, const Grid<Real> &phi)
      : KernelBase(&normal, 0), normal(normal), phi(phi)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Vec3> &normal, const Grid<Real> &phi) const
  {
    normal[idx] = getNormalized(normal[idx]);
  }
  inline Grid<Vec3> &getArg0()
  {
    return normal;
  }
  typedef Grid<Vec3> type0;
  inline const Grid<Real> &getArg1()
  {
    return phi;
  }
  typedef Grid<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel knFlipComputeSurfaceNormals ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, normal, phi);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Vec3> &normal;
  const Grid<Real> &phi;
};

void flipComputeSurfaceNormals(Grid<Vec3> &normal, const Grid<Real> &phi)
{
  GradientOp(normal, phi);
  knFlipComputeSurfaceNormals(normal, phi);
}
static PyObject *_W_10(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipComputeSurfaceNormals", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &normal = *_args.getPtr<Grid<Vec3>>("normal", 0, &_lock);
      const Grid<Real> &phi = *_args.getPtr<Grid<Real>>("phi", 1, &_lock);
      _retval = getPyNone();
      flipComputeSurfaceNormals(normal, phi);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipComputeSurfaceNormals", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipComputeSurfaceNormals", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipComputeSurfaceNormals("", "flipComputeSurfaceNormals", _W_10);
extern "C" {
void PbRegister_flipComputeSurfaceNormals()
{
  KEEP_UNUSED(_RP_flipComputeSurfaceNormals);
}
}

// LEGACY METHOD! Use flipComputeSecondaryParticlePotentials instead!
// computes the neighbor ratio for every fluid cell in &flags as the number of fluid neighbors over
// the maximum possible number of fluid neighbors

struct knFlipUpdateNeighborRatio : public KernelBase {
  knFlipUpdateNeighborRatio(const FlagGrid &flags,
                            Grid<Real> &neighborRatio,
                            const int radius,
                            const int itype = FlagGrid::TypeFluid,
                            const int jtype = FlagGrid::TypeObstacle)
      : KernelBase(&flags, 1),
        flags(flags),
        neighborRatio(neighborRatio),
        radius(radius),
        itype(itype),
        jtype(jtype)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const FlagGrid &flags,
                 Grid<Real> &neighborRatio,
                 const int radius,
                 const int itype = FlagGrid::TypeFluid,
                 const int jtype = FlagGrid::TypeObstacle) const
  {

    if (!(flags(i, j, k) & itype))
      return;

    int countFluid = 0;
    int countMaxFluid = 0;
    for (IndexInt x = i - radius; x <= i + radius; x++) {
      for (IndexInt y = j - radius; y <= j + radius; y++) {
        for (IndexInt z = k - radius; z <= k + radius; z++) {
          if ((x == i && y == j && z == k) || (flags(x, y, z) & jtype))
            continue;
          if (flags(x, y, z) & itype) {
            countFluid++;
            countMaxFluid++;
          }
          else {
            countMaxFluid++;
          }
        }
      }
    }
    neighborRatio(i, j, k) = float(countFluid) / float(countMaxFluid);
  }
  inline const FlagGrid &getArg0()
  {
    return flags;
  }
  typedef FlagGrid type0;
  inline Grid<Real> &getArg1()
  {
    return neighborRatio;
  }
  typedef Grid<Real> type1;
  inline const int &getArg2()
  {
    return radius;
  }
  typedef int type2;
  inline const int &getArg3()
  {
    return itype;
  }
  typedef int type3;
  inline const int &getArg4()
  {
    return jtype;
  }
  typedef int type4;
  void runMessage()
  {
    debMsg("Executing kernel knFlipUpdateNeighborRatio ", 3);
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
            op(i, j, k, flags, neighborRatio, radius, itype, jtype);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 1; i < _maxX; i++)
          op(i, j, k, flags, neighborRatio, radius, itype, jtype);
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
  Grid<Real> &neighborRatio;
  const int radius;
  const int itype;
  const int jtype;
};

void flipUpdateNeighborRatio(const FlagGrid &flags,
                             Grid<Real> &neighborRatio,
                             const int radius,
                             const int itype = FlagGrid::TypeFluid,
                             const int jtype = FlagGrid::TypeObstacle)
{

  neighborRatio.clear();
  knFlipUpdateNeighborRatio(flags, neighborRatio, radius, itype, jtype);
}
static PyObject *_W_11(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "flipUpdateNeighborRatio", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      Grid<Real> &neighborRatio = *_args.getPtr<Grid<Real>>("neighborRatio", 1, &_lock);
      const int radius = _args.get<int>("radius", 2, &_lock);
      const int itype = _args.getOpt<int>("itype", 3, FlagGrid::TypeFluid, &_lock);
      const int jtype = _args.getOpt<int>("jtype", 4, FlagGrid::TypeObstacle, &_lock);
      _retval = getPyNone();
      flipUpdateNeighborRatio(flags, neighborRatio, radius, itype, jtype);
      _args.check();
    }
    pbFinalizePlugin(parent, "flipUpdateNeighborRatio", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("flipUpdateNeighborRatio", e.what());
    return 0;
  }
}
static const Pb::Register _RP_flipUpdateNeighborRatio("", "flipUpdateNeighborRatio", _W_11);
extern "C" {
void PbRegister_flipUpdateNeighborRatio()
{
  KEEP_UNUSED(_RP_flipUpdateNeighborRatio);
}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
// Legacy Methods (still useful for debugging)
//----------------------------------------------------------------------------------------------------------------------------------------------------
#pragma endregion

}  // namespace Manta
