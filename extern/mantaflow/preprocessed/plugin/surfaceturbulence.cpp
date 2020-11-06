

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2016 Olivier Mercier, oli.mercier@gmail.com
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Surface Turbulence for Particle-Based Liquid Simulations
 * Mercier et al., SIGGRAPH Asia 2015
 *
 * Possible speedups :
 * - only initialize surface points around coarse particles near the surface. Use the flags in the
 *fluid grid and only use cells with non-fluid neighbors.
 *
 ******************************************************************************/

// use chrono stl for detailed timing only if available
#ifdef __GNUC__
#  if __GNUC__ < 5
#    define USE_CHRONO 0
#  endif
#endif

#if MANTA_WITHCPP11 == 1
#  ifndef USE_CHRONO
#    define USE_CHRONO 1
#  endif
#endif

#include <iomanip>
#if USE_CHRONO == 1
#  include <chrono>
#endif
#include "particle.h"

using namespace std;
namespace Manta {

// own namespace for globals
namespace SurfaceTurbulence {

//
// **** surface turbulence parameters ****
//
struct SurfaceTurbulenceParameters {
  int res;
  Real outerRadius;
  int surfaceDensity;
  int nbSurfaceMaintenanceIterations;
  Real dt;
  Real waveSpeed;
  Real waveDamping;
  Real waveSeedFrequency;
  Real waveMaxAmplitude;
  Real waveMaxFrequency;
  Real waveMaxSeedingAmplitude;  // as ratio of max amp;
  Real waveSeedingCurvatureThresholdRegionCenter;
  Real waveSeedingCurvatureThresholdRegionRadius;
  Real waveSeedStepSizeRatioOfMax;
  Real innerRadius;
  Real meanFineDistance;
  Real constraintA;
  Real normalRadius;
  Real tangentRadius;
  Real bndXm, bndXp, bndYm, bndYp, bndZm, bndZp;
};
SurfaceTurbulenceParameters params;

//
// **** acceleration grid for particle neighbor queries ****
//
struct ParticleAccelGrid {
  int res;
  vector<int> ***indices;

  void init(int inRes)
  {
    res = inRes;
    indices = new vector<int> **[res];
    for (int i = 0; i < res; i++) {
      indices[i] = new vector<int> *[res];
      for (int j = 0; j < res; j++) {
        indices[i][j] = new vector<int>[res];
      }
    }
  }

  void fillWith(const BasicParticleSystem &particles)
  {
    // clear
    for (int i = 0; i < res; i++) {
      for (int j = 0; j < res; j++) {
        for (int k = 0; k < res; k++) {
          indices[i][j][k].clear();
        }
      }
    }

    // fill
    for (int id = 0; id < particles.size(); id++) {
      Vec3 pos = particles.getPos(id);
      int i = clamp<int>(floor(pos.x / params.res * res), 0, res - 1);
      int j = clamp<int>(floor(pos.y / params.res * res), 0, res - 1);
      int k = clamp<int>(floor(pos.z / params.res * res), 0, res - 1);
      indices[i][j][k].push_back(id);
    }
  }

  void fillWith(const ParticleDataImpl<Vec3> &particles)
  {
    // clear
    for (int i = 0; i < res; i++) {
      for (int j = 0; j < res; j++) {
        for (int k = 0; k < res; k++) {
          indices[i][j][k].clear();
        }
      }
    }

    // fill
    for (int id = 0; id < particles.size(); id++) {
      Vec3 pos = particles[id];
      int i = clamp<int>(floor(pos.x / params.res * res), 0, res - 1);
      int j = clamp<int>(floor(pos.y / params.res * res), 0, res - 1);
      int k = clamp<int>(floor(pos.z / params.res * res), 0, res - 1);
      indices[i][j][k].push_back(id);
    }
  }
};

#define LOOP_NEIGHBORS_BEGIN(points, center, radius) \
  int minI = clamp<int>( \
      floor((center.x - radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  int maxI = clamp<int>( \
      floor((center.x + radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  int minJ = clamp<int>( \
      floor((center.y - radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  int maxJ = clamp<int>( \
      floor((center.y + radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  int minK = clamp<int>( \
      floor((center.z - radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  int maxK = clamp<int>( \
      floor((center.z + radius) / params.res * points.accel->res), 0, points.accel->res - 1); \
  for (int i = minI; i <= maxI; i++) { \
    for (int j = minJ; j <= maxJ; j++) { \
      for (int k = minK; k <= maxK; k++) { \
        for (int idLOOPNEIGHBORS = 0; \
             idLOOPNEIGHBORS < (int)points.accel->indices[i][j][k].size(); \
             idLOOPNEIGHBORS++) { \
          int idn = points.accel->indices[i][j][k][idLOOPNEIGHBORS]; \
          if (points.isActive(idn)) {
#define LOOP_NEIGHBORS_END \
  } \
  } \
  } \
  } \
  }

#define LOOP_GHOSTS_POS_BEGIN(pos, radius) \
  int flagLOOPGHOSTS = -1; \
  Vec3 gPos; \
  while (flagLOOPGHOSTS < 6) { \
    if (flagLOOPGHOSTS < 0 && pos.x - params.bndXm <= radius) { \
      flagLOOPGHOSTS = 0; \
      gPos = Vec3(2.f * params.bndXm - pos.x, pos.y, pos.z); \
    } \
    else if (flagLOOPGHOSTS < 1 && params.bndXp - pos.x <= radius) { \
      flagLOOPGHOSTS = 1; \
      gPos = Vec3(2.f * params.bndXp - pos.x, pos.y, pos.z); \
    } \
    else if (flagLOOPGHOSTS < 2 && pos.y - params.bndYm <= radius) { \
      flagLOOPGHOSTS = 2; \
      gPos = Vec3(pos.x, 2.f * params.bndYm - pos.y, pos.z); \
    } \
    else if (flagLOOPGHOSTS < 3 && params.bndYp - pos.y <= radius) { \
      flagLOOPGHOSTS = 3; \
      gPos = Vec3(pos.x, 2.f * params.bndYp - pos.y, pos.z); \
    } \
    else if (flagLOOPGHOSTS < 4 && pos.z - params.bndZm <= radius) { \
      flagLOOPGHOSTS = 4; \
      gPos = Vec3(pos.x, pos.y, 2.f * params.bndZm - pos.z); \
    } \
    else if (flagLOOPGHOSTS < 5 && params.bndZp - pos.Z <= radius) { \
      flagLOOPGHOSTS = 5; \
      gPos = Vec3(pos.x, pos.y, 2.f * params.bndZp - pos.z); \
    } \
    else { \
      flagLOOPGHOSTS = 6; \
      gPos = Vec3(pos.x, pos.y, pos.z); \
    }
#define LOOP_GHOSTS_POS_NORMAL_BEGIN(pos, normal, radius) \
  int flagLOOPGHOSTS = -1; \
  Vec3 gPos, gNormal; \
  while (flagLOOPGHOSTS < 6) { \
    if (flagLOOPGHOSTS < 0 && pos.x - params.bndXm <= radius) { \
      flagLOOPGHOSTS = 0; \
      gPos = Vec3(2.f * params.bndXm - pos.x, pos.y, pos.z); \
      gNormal = Vec3(-normal.x, normal.y, normal.z); \
    } \
    else if (flagLOOPGHOSTS < 1 && params.bndXp - pos.x <= radius) { \
      flagLOOPGHOSTS = 1; \
      gPos = Vec3(2.f * params.bndXp - pos.x, pos.y, pos.z); \
      gNormal = Vec3(-normal.x, normal.y, normal.z); \
    } \
    else if (flagLOOPGHOSTS < 2 && pos.y - params.bndYm <= radius) { \
      flagLOOPGHOSTS = 2; \
      gPos = Vec3(pos.x, 2.f * params.bndYm - pos.y, pos.z); \
      gNormal = Vec3(normal.x, -normal.y, normal.z); \
    } \
    else if (flagLOOPGHOSTS < 3 && params.bndYp - pos.y <= radius) { \
      flagLOOPGHOSTS = 3; \
      gPos = Vec3(pos.x, 2.f * params.bndYp - pos.y, pos.z); \
      gNormal = Vec3(normal.x, -normal.y, normal.z); \
    } \
    else if (flagLOOPGHOSTS < 4 && pos.z - params.bndZm <= radius) { \
      flagLOOPGHOSTS = 4; \
      gPos = Vec3(pos.x, pos.y, 2.f * params.bndZm - pos.z); \
      gNormal = Vec3(normal.x, normal.y, -normal.z); \
    } \
    else if (flagLOOPGHOSTS < 5 && params.bndZp - pos.Z <= radius) { \
      flagLOOPGHOSTS = 5; \
      gPos = Vec3(pos.x, pos.y, 2.f * params.bndZp - pos.z); \
      gNormal = Vec3(normal.x, normal.y, -normal.z); \
    } \
    else { \
      flagLOOPGHOSTS = 6; \
      gPos = pos; \
      gNormal = normal; \
    }
#define LOOP_GHOSTS_END }

//
// **** Wrappers around point sets to attach it an acceleration grid ****
//
struct PointSetWrapper {
  ParticleAccelGrid *accel;

  PointSetWrapper(ParticleAccelGrid *inAccel)
  {
    accel = inAccel;
  }
  virtual void updateAccel() = 0;
};

struct BasicParticleSystemWrapper : PointSetWrapper {
  BasicParticleSystem *points;

  BasicParticleSystemWrapper(ParticleAccelGrid *inAccel) : PointSetWrapper(inAccel)
  {
  }

  Vec3 getPos(int id) const
  {
    return points->getPos(id);
  }
  void setPos(int id, Vec3 pos)
  {
    points->setPos(id, pos);
  }
  void updateAccel()
  {
    accel->fillWith(*points);
  }
  void clear()
  {
    points->clear();
  }
  int size() const
  {
    return points->size();
  }
  bool isActive(int id) const
  {
    return points->isActive(id);
  }
  void addParticle(Vec3 pos)
  {
    points->addParticle(pos);
  }
  int getStatus(int id) const
  {
    return points->getStatus(id);
  }
  void addBuffered(Vec3 pos)
  {
    points->addBuffered(pos);
  }
  void doCompress()
  {
    points->doCompress();
  }
  void insertBufferedParticles()
  {
    points->insertBufferedParticles();
  }
  void kill(int id)
  {
    points->kill(id);
  }

  bool hasNeighbor(Vec3 pos, Real radius) const
  {
    bool answer = false;
    int minI = clamp<int>(floor((pos.x - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxI = clamp<int>(floor((pos.x + radius) / params.res * accel->res), 0, accel->res - 1);
    int minJ = clamp<int>(floor((pos.y - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxJ = clamp<int>(floor((pos.y + radius) / params.res * accel->res), 0, accel->res - 1);
    int minK = clamp<int>(floor((pos.z - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxK = clamp<int>(floor((pos.z + radius) / params.res * accel->res), 0, accel->res - 1);
    for (int i = minI; i <= maxI; i++) {
      for (int j = minJ; j <= maxJ; j++) {
        for (int k = minK; k <= maxK; k++) {
          for (int id = 0; id < (int)accel->indices[i][j][k].size(); id++) {
            if (points->isActive(accel->indices[i][j][k][id]) &&
                norm(points->getPos(accel->indices[i][j][k][id]) - pos) <= radius) {
              answer = true;
              break;
            }
          }
          if (answer)
            break;
        }
        if (answer)
          break;
      }
      if (answer)
        break;
    }
    return answer;
  }

  bool hasNeighborOtherThanItself(int idx, Real radius) const
  {
    bool answer = false;
    Vec3 pos = points->getPos(idx);
    int minI = clamp<int>(floor((pos.x - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxI = clamp<int>(floor((pos.x + radius) / params.res * accel->res), 0, accel->res - 1);
    int minJ = clamp<int>(floor((pos.y - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxJ = clamp<int>(floor((pos.y + radius) / params.res * accel->res), 0, accel->res - 1);
    int minK = clamp<int>(floor((pos.z - radius) / params.res * accel->res), 0, accel->res - 1);
    int maxK = clamp<int>(floor((pos.z + radius) / params.res * accel->res), 0, accel->res - 1);
    for (int i = minI; i <= maxI; i++) {
      for (int j = minJ; j <= maxJ; j++) {
        for (int k = minK; k <= maxK; k++) {
          for (int id = 0; id < (int)accel->indices[i][j][k].size(); id++) {
            if (accel->indices[i][j][k][id] != idx &&
                points->isActive(accel->indices[i][j][k][id]) &&
                norm(points->getPos(accel->indices[i][j][k][id]) - pos) <= radius) {
              answer = true;
              break;
            }
          }
          if (answer)
            break;
        }
        if (answer)
          break;
      }
      if (answer)
        break;
    }
    return answer;
  }

  void removeInvalidIndices(vector<int> &indices)
  {
    vector<int> copy;
    copy.resize(indices.size());
    for (int i = 0; i < (int)indices.size(); i++) {
      copy[i] = indices[i];
    }
    indices.clear();
    for (int i = 0; i < (int)copy.size(); i++) {
      if (points->isActive(copy[i])) {
        indices.push_back(copy[i]);
      }
    }
  }
};

struct ParticleDataImplVec3Wrapper : PointSetWrapper {
  ParticleDataImpl<Vec3> *points;

  ParticleDataImplVec3Wrapper(ParticleAccelGrid *inAccel) : PointSetWrapper(inAccel)
  {
  }

  Vec3 getVec3(int id) const
  {
    return (*points)[id];
  }
  void setVec3(int id, Vec3 vec)
  {
    (*points)[id] = vec;
  }
  void updateAccel()
  {
    accel->fillWith(*points);
  }
  bool isActive(int i) const
  {
    return true;
  }
};

//
// **** globals ****
//
ParticleAccelGrid accelCoarse, accelSurface;
BasicParticleSystemWrapper coarseParticles(&accelCoarse), surfacePoints(&accelSurface);
ParticleDataImplVec3Wrapper coarseParticlesPrevPos(
    &accelCoarse);             // WARNING: reusing the coarse accel grid to save space, don't query
                               // coarseParticlesPrevPos and coarseParticles at the same time.
vector<Vec3> tempSurfaceVec3;  // to store misc info on surface points
vector<Real> tempSurfaceFloat;  // to store misc info on surface points
int frameCount = 0;

//
//**** weighting kernels *****
//
Real triangularWeight(Real distance, Real radius)
{
  return 1.0f - distance / radius;
}
Real exponentialWeight(Real distance, Real radius, Real falloff)
{
  if (distance > radius)
    return 0;
  Real tmp = distance / radius;
  return expf(-falloff * tmp * tmp);
}

Real weightKernelAdvection(Real distance)
{
  if (distance > 2.f * params.outerRadius) {
    return 0;
  }
  else {
    return triangularWeight(distance, 2.f * params.outerRadius);
  }
}

Real weightKernelCoarseDensity(Real distance)
{
  return exponentialWeight(distance, params.outerRadius, 2.0f);
}

Real weightSurfaceNormal(Real distance)
{
  if (distance > params.normalRadius) {
    return 0;
  }
  else {
    return triangularWeight(distance, params.normalRadius);
  }
}

Real weightSurfaceTangent(Real distance)
{
  if (distance > params.tangentRadius) {
    return 0;
  }
  else {
    return triangularWeight(distance, params.tangentRadius);
  }
}

//
// **** utility ****
//

bool isInDomain(Vec3 pos)
{
  return params.bndXm <= pos.x && pos.x <= params.bndXp && params.bndYm <= pos.y &&
         pos.y <= params.bndYp && params.bndZm <= pos.z && pos.z <= params.bndZp;
}

Real smoothstep(Real edgeLeft, Real edgeRight, Real val)
{
  Real x = clamp((val - edgeLeft) / (edgeRight - edgeLeft), Real(0.), Real(1.));
  return x * x * (3 - 2 * x);
}

//
// **** surface initialization ****
//

void initFines(const BasicParticleSystemWrapper &coarseParticles,
               BasicParticleSystemWrapper &surfacePoints,
               const FlagGrid &flags)
{
  unsigned int discretization = (unsigned int)M_PI * (params.outerRadius + params.innerRadius) /
                                params.meanFineDistance;
  Real dtheta = 2 * params.meanFineDistance / (params.outerRadius + params.innerRadius);
  Real outerRadius2 = params.outerRadius * params.outerRadius;

  surfacePoints.clear();
  for (int idx = 0; idx < (int)coarseParticles.size(); idx++) {

    if (idx % 500 == 0) {
      cout << "Initializing surface points : " << setprecision(4)
           << 100.f * idx / coarseParticles.size() << "%" << endl;
    }

    if (coarseParticles.isActive(idx)) {

      // check flags if we are near surface
      bool nearSurface = false;
      Vec3 pos = coarseParticles.getPos(idx);
      for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
          for (int k = -1; k <= 1; k++) {
            if (!flags.isFluid(((int)pos.x) + i, ((int)pos.y) + j, ((int)pos.z) + k)) {
              nearSurface = true;
              break;
            }
          }
        }
      }

      if (nearSurface) {
        for (unsigned int i = 0; i <= discretization / 2; ++i) {
          Real discretization2 = Real(floor(2 * M_PI * sin(i * dtheta) / dtheta) + 1);
          for (Real phi = 0; phi < 2 * M_PI; phi += Real(2 * M_PI / discretization2)) {
            Real theta = i * dtheta;
            Vec3 normal(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
            Vec3 position = coarseParticles.getPos(idx) + params.outerRadius * normal;

            bool valid = true;
            LOOP_NEIGHBORS_BEGIN(coarseParticles, position, 2.f * params.outerRadius)
            if (idx != idn && normSquare(position - coarseParticles.getPos(idn)) < outerRadius2) {
              valid = false;
              break;
            }
            LOOP_NEIGHBORS_END
            if (valid) {
              surfacePoints.addParticle(position);
            }
          }
        }
      }
    }
  }
}

//
// **** surface advection ****
//

struct advectSurfacePoints : public KernelBase {
  advectSurfacePoints(BasicParticleSystemWrapper &surfacePoints,
                      const BasicParticleSystemWrapper &coarseParticles,
                      const ParticleDataImplVec3Wrapper &coarseParticlesPrevPos)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        coarseParticles(coarseParticles),
        coarseParticlesPrevPos(coarseParticlesPrevPos)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystemWrapper &surfacePoints,
                 const BasicParticleSystemWrapper &coarseParticles,
                 const ParticleDataImplVec3Wrapper &coarseParticlesPrevPos) const
  {
    if (surfacePoints.isActive(idx)) {
      Vec3 avgDisplacement(0, 0, 0);
      Real totalWeight = 0;
      Vec3 p = surfacePoints.getPos(idx);
      LOOP_NEIGHBORS_BEGIN(
          coarseParticlesPrevPos, surfacePoints.getPos(idx), 2.0f * params.outerRadius)
      if ((coarseParticles.getStatus(idn) & ParticleBase::PNEW) == 0 &&
          (coarseParticles.getStatus(idn) & ParticleBase::PDELETE) == 0) {
        Vec3 disp = coarseParticles.getPos(idn) - coarseParticlesPrevPos.getVec3(idn);
        Real distance = norm(coarseParticlesPrevPos.getVec3(idn) - p);
        Real w = weightKernelAdvection(distance);
        avgDisplacement += w * disp;
        totalWeight += w;
      }
      LOOP_NEIGHBORS_END
      if (totalWeight != 0)
        avgDisplacement /= totalWeight;
      surfacePoints.setPos(idx, p + avgDisplacement);
    }
  }
  inline BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const BasicParticleSystemWrapper &getArg1()
  {
    return coarseParticles;
  }
  typedef BasicParticleSystemWrapper type1;
  inline const ParticleDataImplVec3Wrapper &getArg2()
  {
    return coarseParticlesPrevPos;
  }
  typedef ParticleDataImplVec3Wrapper type2;
  void runMessage()
  {
    debMsg("Executing kernel advectSurfacePoints ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, coarseParticles, coarseParticlesPrevPos);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystemWrapper &surfacePoints;
  const BasicParticleSystemWrapper &coarseParticles;
  const ParticleDataImplVec3Wrapper &coarseParticlesPrevPos;
};

//
// **** value and gradient of level-set band constraint ****
//
Real computeConstraintLevel(const BasicParticleSystemWrapper &coarseParticles, Vec3 pos)
{
  Real lvl = 0.0f;
  LOOP_NEIGHBORS_BEGIN(coarseParticles, pos, 1.5f * params.outerRadius)
  lvl += expf(-params.constraintA * normSquare(coarseParticles.getPos(idn) - pos));
  LOOP_NEIGHBORS_END
  if (lvl > 1.0f)
    lvl = 1.0f;
  lvl = (sqrtf(-logf(lvl) / params.constraintA) - params.innerRadius) /
        (params.outerRadius - params.innerRadius);
  return lvl;
}

Vec3 computeConstraintGradient(const BasicParticleSystemWrapper &coarseParticles, Vec3 pos)
{
  Vec3 gradient(0, 0, 0);
  LOOP_NEIGHBORS_BEGIN(coarseParticles, pos, 1.5f * params.outerRadius)
  gradient += 2.f * params.constraintA *
              (Real)(expf(-params.constraintA * normSquare(coarseParticles.getPos(idn) - pos))) *
              (pos - coarseParticles.getPos(idn));
  LOOP_NEIGHBORS_END
  return getNormalized(gradient);
}

//
// **** compute surface normals ****
//

struct computeSurfaceNormals : public KernelBase {
  computeSurfaceNormals(const BasicParticleSystemWrapper &surfacePoints,
                        const BasicParticleSystemWrapper &coarseParticles,
                        ParticleDataImpl<Vec3> &surfaceNormals)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        coarseParticles(coarseParticles),
        surfaceNormals(surfaceNormals)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const BasicParticleSystemWrapper &coarseParticles,
                 ParticleDataImpl<Vec3> &surfaceNormals) const
  {
    Vec3 pos = surfacePoints.getPos(idx);

    // approx normal with gradient
    Vec3 gradient = computeConstraintGradient(coarseParticles, pos);

    // get tangent frame
    Vec3 n = getNormalized(gradient);
    Vec3 vx(1, 0, 0);
    Vec3 vy(0, 1, 0);
    Real dotX = dot(n, vx);
    Real dotY = dot(n, vy);
    Vec3 t1 = getNormalized(fabs(dotX) < fabs(dotY) ? cross(n, vx) : cross(n, vy));
    Vec3 t2 = getNormalized(cross(n, t1));  // initial frame

    // linear fit of neighboring surface points in approximated tangent frame
    Real sw = 0, swx = 0, swy = 0, swxy = 0, swx2 = 0, swy2 = 0, swxz = 0, swyz = 0, swz = 0;
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.normalRadius)
    LOOP_GHOSTS_POS_BEGIN(surfacePoints.getPos(idn), params.normalRadius)
    Real x = dot(gPos - pos, t1);
    Real y = dot(gPos - pos, t2);
    Real z = dot(gPos - pos, n);
    Real w = weightSurfaceNormal(norm(pos - gPos));
    swx2 += w * x * x;
    swy2 += w * y * y;
    swxy += w * x * y;
    swxz += w * x * z;
    swyz += w * y * z;
    swx += w * x;
    swy += w * y;
    swz += w * z;
    sw += w;
    LOOP_GHOSTS_END
    LOOP_NEIGHBORS_END
    Real det = -sw * swxy * swxy + 2.f * swx * swxy * swy - swx2 * swy * swy - swx * swx * swy2 +
               sw * swx2 * swy2;
    if (det == 0) {
      surfaceNormals[idx] = Vec3(0, 0, 0);
    }
    else {
      Vec3 abc = 1.f / det *
                 Vec3(swxz * (-swy * swy + sw * swy2) + swyz * (-sw * swxy + swx * swy) +
                          swz * (swxy * swy - swx * swy2),
                      swxz * (-sw * swxy + swx * swy) + swyz * (-swx * swx + sw * swx2) +
                          swz * (swx * swxy - swx2 * swy),
                      swxz * (swxy * swy - swx * swy2) + swyz * (swx * swxy - swx2 * swy) +
                          swz * (-swxy * swxy + swx2 * swy2));
      Vec3 normal = -getNormalized(t1 * abc.x + t2 * abc.y - n);
      if (dot(gradient, normal) < 0) {
        normal = -normal;
      }
      surfaceNormals[idx] = normal;
    }
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const BasicParticleSystemWrapper &getArg1()
  {
    return coarseParticles;
  }
  typedef BasicParticleSystemWrapper type1;
  inline ParticleDataImpl<Vec3> &getArg2()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type2;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceNormals ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, coarseParticles, surfaceNormals);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const BasicParticleSystemWrapper &coarseParticles;
  ParticleDataImpl<Vec3> &surfaceNormals;
};

//
// **** smooth surface normals ****
//

struct computeAveragedNormals : public KernelBase {
  computeAveragedNormals(const BasicParticleSystemWrapper &surfacePoints,
                         const ParticleDataImpl<Vec3> &surfaceNormals)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const ParticleDataImpl<Vec3> &surfaceNormals) const
  {
    Vec3 pos = surfacePoints.getPos(idx);
    Vec3 newNormal = Vec3(0, 0, 0);
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.normalRadius)
    Real w = weightSurfaceNormal(norm(pos - surfacePoints.getPos(idn)));
    newNormal += w * surfaceNormals[idn];
    LOOP_NEIGHBORS_END
    tempSurfaceVec3[idx] = getNormalized(newNormal);
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel computeAveragedNormals ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const ParticleDataImpl<Vec3> &surfaceNormals;
};

struct assignNormals : public KernelBase {
  assignNormals(const BasicParticleSystemWrapper &surfacePoints,
                ParticleDataImpl<Vec3> &surfaceNormals)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Vec3> &surfaceNormals) const
  {
    surfaceNormals[idx] = tempSurfaceVec3[idx];
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel assignNormals ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Vec3> &surfaceNormals;
};

void smoothSurfaceNormals(const BasicParticleSystemWrapper &surfacePoints,
                          ParticleDataImpl<Vec3> &surfaceNormals)
{
  tempSurfaceVec3.resize(surfacePoints.size());

  computeAveragedNormals(surfacePoints, surfaceNormals);
  assignNormals(surfacePoints, surfaceNormals);
}

//
// **** addition/deletion of particles. Not parallel to prevent write/delete conflicts ****
//

void addDeleteSurfacePoints(BasicParticleSystemWrapper &surfacePoints)
{
  int fixedSize = surfacePoints.size();
  for (int idx = 0; idx < fixedSize; idx++) {
    // compute proxy tangent displacement
    Vec3 pos = surfacePoints.getPos(idx);

    Vec3 gradient = computeConstraintGradient(coarseParticles, pos);

    Real wt = 0;
    Vec3 tangentDisplacement(0, 0, 0);
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.tangentRadius)
    if (idn != idx) {
      Vec3 dir = pos - surfacePoints.getPos(idn);
      Real length = norm(dir);
      dir = getNormalized(dir);

      // Decompose direction into normal and tangent directions.
      Vec3 dn = dot(dir, gradient) * gradient;
      Vec3 dt = dir - dn;

      Real w = weightSurfaceTangent(length);
      wt += w;
      tangentDisplacement += w * dt;
    }
    LOOP_NEIGHBORS_END
    if (norm(tangentDisplacement) != 0) {
      tangentDisplacement = getNormalized(tangentDisplacement);
    }

    // check density criterion, add surface point if necessary
    Vec3 creationPos = pos + params.meanFineDistance * tangentDisplacement;
    if (isInDomain(creationPos) &&
        !surfacePoints.hasNeighbor(creationPos, params.meanFineDistance - (1e-6))) {
      // create point
      surfacePoints.addBuffered(creationPos);
    }
  }

  surfacePoints.doCompress();
  surfacePoints.insertBufferedParticles();

  // check density criterion, delete surface points if necessary
  fixedSize = surfacePoints.size();
  for (int idx = 0; idx < fixedSize; idx++) {
    if (!isInDomain(surfacePoints.getPos(idx)) ||
        surfacePoints.hasNeighborOtherThanItself(idx, 0.67 * params.meanFineDistance)) {
      surfacePoints.kill(idx);
    }
  }

  // delete surface points if no coarse neighbors in advection radius
  fixedSize = surfacePoints.size();
  for (int idx = 0; idx < fixedSize; idx++) {
    Vec3 pos = surfacePoints.getPos(idx);
    if (!coarseParticles.hasNeighbor(pos, 2.f * params.outerRadius)) {
      surfacePoints.kill(idx);
    }
  }

  // delete surface point if too far from constraint
  fixedSize = surfacePoints.size();
  for (int idx = 0; idx < fixedSize; idx++) {
    Real level = computeConstraintLevel(coarseParticles, surfacePoints.getPos(idx));
    if (level < -0.2 || level > 1.2) {
      surfacePoints.kill(idx);
    }
  }

  surfacePoints.doCompress();
  surfacePoints.insertBufferedParticles();
}

//
// **** surface maintenance ****
//

struct computeSurfaceDensities : public KernelBase {
  computeSurfaceDensities(const BasicParticleSystemWrapper &surfacePoints, void *dummy)
      : KernelBase(surfacePoints.size()), surfacePoints(surfacePoints), dummy(dummy)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const BasicParticleSystemWrapper &surfacePoints, void *dummy) const
  {
    Vec3 pos = surfacePoints.getPos(idx);
    Real density = 0;
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.normalRadius)
    LOOP_GHOSTS_POS_BEGIN(surfacePoints.getPos(idn), params.normalRadius)
    density += weightSurfaceNormal(norm(pos - gPos));
    LOOP_GHOSTS_END
    LOOP_NEIGHBORS_END
    tempSurfaceFloat[idx] = density;
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline void *getArg1()
  {
    return dummy;
  }
  typedef void type1;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceDensities ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, dummy);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  void *dummy;
};

struct computeSurfaceDisplacements : public KernelBase {
  computeSurfaceDisplacements(const BasicParticleSystemWrapper &surfacePoints,
                              const ParticleDataImpl<Vec3> &surfaceNormals)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const ParticleDataImpl<Vec3> &surfaceNormals) const
  {
    Vec3 pos = surfacePoints.getPos(idx);
    Vec3 normal = surfaceNormals[idx];

    Vec3 displacementNormal(0, 0, 0);
    Vec3 displacementTangent(0, 0, 0);
    Real wTotal = 0;
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.normalRadius)

    LOOP_GHOSTS_POS_NORMAL_BEGIN(
        surfacePoints.getPos(idn), surfaceNormals[idn], params.normalRadius)
    Vec3 dir = pos - gPos;
    Real length = norm(dir);
    Vec3 dn = dot(dir, surfaceNormals[idx]) * surfaceNormals[idx];
    Vec3 dt = dir - dn;
    if (tempSurfaceFloat[idn] == 0) {
      continue;
    }
    Real w = weightSurfaceNormal(length) / tempSurfaceFloat[idn];

    Vec3 crossVec = getNormalized(cross(normal, -dir));
    Vec3 projectedNormal = getNormalized(gNormal - dot(crossVec, gNormal) * crossVec);
    if (dot(projectedNormal, normal) < 0 || abs(dot(normal, normal + projectedNormal)) < 1e-6) {
      continue;
    }
    dn = -dot(normal + projectedNormal, dir) / dot(normal, normal + projectedNormal) * normal;

    displacementNormal += w * dn;
    displacementTangent += w * getNormalized(dt);
    wTotal += w;
    LOOP_GHOSTS_END

    LOOP_NEIGHBORS_END
    if (wTotal != 0) {
      displacementNormal /= wTotal;
      displacementTangent /= wTotal;
    }
    displacementNormal *= .75f;
    displacementTangent *= .25f * params.meanFineDistance;
    tempSurfaceVec3[idx] = displacementNormal + displacementTangent;
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceDisplacements ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const ParticleDataImpl<Vec3> &surfaceNormals;
};

struct applySurfaceDisplacements : public KernelBase {
  applySurfaceDisplacements(BasicParticleSystemWrapper &surfacePoints, void *dummy)
      : KernelBase(surfacePoints.size()), surfacePoints(surfacePoints), dummy(dummy)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, BasicParticleSystemWrapper &surfacePoints, void *dummy) const
  {
    surfacePoints.setPos(idx, surfacePoints.getPos(idx) + tempSurfaceVec3[idx]);
  }
  inline BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline void *getArg1()
  {
    return dummy;
  }
  typedef void type1;
  void runMessage()
  {
    debMsg("Executing kernel applySurfaceDisplacements ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, dummy);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystemWrapper &surfacePoints;
  void *dummy;
};

void regularizeSurfacePoints(BasicParticleSystemWrapper &surfacePoints,
                             const ParticleDataImpl<Vec3> &surfaceNormals)
{
  tempSurfaceVec3.resize(surfacePoints.size());
  tempSurfaceFloat.resize(surfacePoints.size());

  computeSurfaceDensities(surfacePoints, 0);
  computeSurfaceDisplacements(surfacePoints, surfaceNormals);
  applySurfaceDisplacements(surfacePoints, 0);
}

struct constrainSurface : public KernelBase {
  constrainSurface(BasicParticleSystemWrapper &surfacePoints,
                   const BasicParticleSystemWrapper &coarseParticles)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        coarseParticles(coarseParticles)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 BasicParticleSystemWrapper &surfacePoints,
                 const BasicParticleSystemWrapper &coarseParticles) const
  {
    Vec3 pos = surfacePoints.getPos(idx);
    Real level = computeConstraintLevel(coarseParticles, surfacePoints.getPos(idx));
    if (level > 1) {
      surfacePoints.setPos(
          idx,
          pos - (params.outerRadius - params.innerRadius) * (level - 1) *
                    computeConstraintGradient(coarseParticles, surfacePoints.getPos(idx)));
    }
    else if (level < 0) {
      surfacePoints.setPos(
          idx,
          pos - (params.outerRadius - params.innerRadius) * level *
                    computeConstraintGradient(coarseParticles, surfacePoints.getPos(idx)));
    }
  }
  inline BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const BasicParticleSystemWrapper &getArg1()
  {
    return coarseParticles;
  }
  typedef BasicParticleSystemWrapper type1;
  void runMessage()
  {
    debMsg("Executing kernel constrainSurface ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, coarseParticles);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  BasicParticleSystemWrapper &surfacePoints;
  const BasicParticleSystemWrapper &coarseParticles;
};

struct interpolateNewWaveData : public KernelBase {
  interpolateNewWaveData(const BasicParticleSystemWrapper &surfacePoints,
                         ParticleDataImpl<Real> &surfaceWaveH,
                         ParticleDataImpl<Real> &surfaceWaveDtH,
                         ParticleDataImpl<Real> &surfaceWaveSeed,
                         ParticleDataImpl<Real> &surfaceWaveSeedAmplitude)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceWaveH(surfaceWaveH),
        surfaceWaveDtH(surfaceWaveDtH),
        surfaceWaveSeed(surfaceWaveSeed),
        surfaceWaveSeedAmplitude(surfaceWaveSeedAmplitude)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Real> &surfaceWaveH,
                 ParticleDataImpl<Real> &surfaceWaveDtH,
                 ParticleDataImpl<Real> &surfaceWaveSeed,
                 ParticleDataImpl<Real> &surfaceWaveSeedAmplitude) const
  {
    if (surfacePoints.getStatus(idx) & ParticleBase::PNEW) {
      Vec3 pos = surfacePoints.getPos(idx);
      surfaceWaveH[idx] = 0;
      surfaceWaveDtH[idx] = 0;
      Real wTotal = 0;
      LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.tangentRadius)
      if (!(surfacePoints.getStatus(idn) & ParticleBase::PNEW)) {
        Real w = weightSurfaceTangent(norm(pos - surfacePoints.getPos(idn)));
        surfaceWaveH[idx] += w * surfaceWaveH[idn];
        surfaceWaveDtH[idx] += w * surfaceWaveDtH[idn];
        surfaceWaveSeed[idx] += w * surfaceWaveSeed[idn];
        surfaceWaveSeedAmplitude[idx] += w * surfaceWaveSeedAmplitude[idn];
        wTotal += w;
      }
      LOOP_NEIGHBORS_END
      if (wTotal != 0) {
        surfaceWaveH[idx] /= wTotal;
        surfaceWaveDtH[idx] /= wTotal;
        surfaceWaveSeed[idx] /= wTotal;
        surfaceWaveSeedAmplitude[idx] /= wTotal;
      }
    }
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Real> &getArg1()
  {
    return surfaceWaveH;
  }
  typedef ParticleDataImpl<Real> type1;
  inline ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveDtH;
  }
  typedef ParticleDataImpl<Real> type2;
  inline ParticleDataImpl<Real> &getArg3()
  {
    return surfaceWaveSeed;
  }
  typedef ParticleDataImpl<Real> type3;
  inline ParticleDataImpl<Real> &getArg4()
  {
    return surfaceWaveSeedAmplitude;
  }
  typedef ParticleDataImpl<Real> type4;
  void runMessage()
  {
    debMsg("Executing kernel interpolateNewWaveData ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx,
         surfacePoints,
         surfaceWaveH,
         surfaceWaveDtH,
         surfaceWaveSeed,
         surfaceWaveSeedAmplitude);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Real> &surfaceWaveH;
  ParticleDataImpl<Real> &surfaceWaveDtH;
  ParticleDataImpl<Real> &surfaceWaveSeed;
  ParticleDataImpl<Real> &surfaceWaveSeedAmplitude;
};

void surfaceMaintenance(const BasicParticleSystemWrapper &coarseParticles,
                        BasicParticleSystemWrapper &surfacePoints,
                        ParticleDataImpl<Vec3> &surfaceNormals,
                        ParticleDataImpl<Real> &surfaceWaveH,
                        ParticleDataImpl<Real> &surfaceWaveDtH,
                        ParticleDataImpl<Real> &surfaceWaveSeed,
                        ParticleDataImpl<Real> &surfaceWaveSeedAmplitude,
                        int nbIterations)
{
  int countIterations = nbIterations;
  while (countIterations > 0) {
    addDeleteSurfacePoints(surfacePoints);
    surfacePoints.updateAccel();
    computeSurfaceNormals(surfacePoints, coarseParticles, surfaceNormals);
    smoothSurfaceNormals(surfacePoints, surfaceNormals);

    regularizeSurfacePoints(surfacePoints, surfaceNormals);
    surfacePoints.updateAccel();
    constrainSurface(surfacePoints, coarseParticles);
    surfacePoints.updateAccel();

    interpolateNewWaveData(
        surfacePoints, surfaceWaveH, surfaceWaveDtH, surfaceWaveSeed, surfaceWaveSeedAmplitude);

    countIterations--;
  }
}

//
// **** surface wave seeding and evolution ****
//

struct addSeed : public KernelBase {
  addSeed(const BasicParticleSystemWrapper &surfacePoints,
          ParticleDataImpl<Real> &surfaceWaveH,
          const ParticleDataImpl<Real> &surfaceWaveSeed)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceWaveH(surfaceWaveH),
        surfaceWaveSeed(surfaceWaveSeed)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Real> &surfaceWaveH,
                 const ParticleDataImpl<Real> &surfaceWaveSeed) const
  {
    surfaceWaveH[idx] += surfaceWaveSeed[idx];
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Real> &getArg1()
  {
    return surfaceWaveH;
  }
  typedef ParticleDataImpl<Real> type1;
  inline const ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveSeed;
  }
  typedef ParticleDataImpl<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel addSeed ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceWaveH, surfaceWaveSeed);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Real> &surfaceWaveH;
  const ParticleDataImpl<Real> &surfaceWaveSeed;
};

struct computeSurfaceWaveNormal : public KernelBase {
  computeSurfaceWaveNormal(const BasicParticleSystemWrapper &surfacePoints,
                           const ParticleDataImpl<Vec3> &surfaceNormals,
                           const ParticleDataImpl<Real> &surfaceWaveH)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals),
        surfaceWaveH(surfaceWaveH)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const ParticleDataImpl<Vec3> &surfaceNormals,
                 const ParticleDataImpl<Real> &surfaceWaveH) const
  {
    Vec3 pos = surfacePoints.getPos(idx);

    // get tangent frame
    Vec3 n = getNormalized(surfaceNormals[idx]);
    Vec3 vx(1, 0, 0);
    Vec3 vy(0, 1, 0);
    Real dotX = dot(n, vx);
    Real dotY = dot(n, vy);
    Vec3 t1 = getNormalized(fabs(dotX) < fabs(dotY) ? cross(n, vx) : cross(n, vy));
    Vec3 t2 = getNormalized(cross(n, t1));

    // linear fit
    Real sw = 0, swx = 0, swy = 0, swxy = 0, swx2 = 0, swy2 = 0, swxz = 0, swyz = 0, swz = 0;
    LOOP_NEIGHBORS_BEGIN(surfacePoints, pos, params.tangentRadius)
    LOOP_GHOSTS_POS_BEGIN(surfacePoints.getPos(idn), params.tangentRadius)
    Real x = dot(gPos - pos, t1);
    Real y = dot(gPos - pos, t2);
    Real z = surfaceWaveH[idn];
    Real w = weightSurfaceTangent(norm(pos - gPos));
    swx2 += w * x * x;
    swy2 += w * y * y;
    swxy += w * x * y;
    swxz += w * x * z;
    swyz += w * y * z;
    swx += w * x;
    swy += w * y;
    swz += w * z;
    sw += w;
    LOOP_GHOSTS_END
    LOOP_NEIGHBORS_END
    Real det = -sw * swxy * swxy + 2.f * swx * swxy * swy - swx2 * swy * swy - swx * swx * swy2 +
               sw * swx2 * swy2;
    if (det == 0) {
      tempSurfaceVec3[idx] = Vec3(0, 0, 0);
    }
    else {
      Vec3 abc = 1.f / det *
                 Vec3(swxz * (-swy * swy + sw * swy2) + swyz * (-sw * swxy + swx * swy) +
                          swz * (swxy * swy - swx * swy2),
                      swxz * (-sw * swxy + swx * swy) + swyz * (-swx * swx + sw * swx2) +
                          swz * (swx * swxy - swx2 * swy),
                      swxz * (swxy * swy - swx * swy2) + swyz * (swx * swxy - swx2 * swy) +
                          swz * (-swxy * swxy + swx2 * swy2));
      Vec3 waveNormal = -getNormalized(vx * abc.x + vy * abc.y - Vec3(0, 0, 1));
      tempSurfaceVec3[idx] = waveNormal;
    }
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline const ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveH;
  }
  typedef ParticleDataImpl<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceWaveNormal ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals, surfaceWaveH);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const ParticleDataImpl<Vec3> &surfaceNormals;
  const ParticleDataImpl<Real> &surfaceWaveH;
};

struct computeSurfaceWaveLaplacians : public KernelBase {
  computeSurfaceWaveLaplacians(const BasicParticleSystemWrapper &surfacePoints,
                               const ParticleDataImpl<Vec3> &surfaceNormals,
                               const ParticleDataImpl<Real> &surfaceWaveH)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals),
        surfaceWaveH(surfaceWaveH)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const ParticleDataImpl<Vec3> &surfaceNormals,
                 const ParticleDataImpl<Real> &surfaceWaveH) const
  {
    Real laplacian = 0;
    Real wTotal = 0;
    Vec3 pPos = surfacePoints.getPos(idx);
    Vec3 pNormal = surfaceNormals[idx];

    Vec3 vx(1, 0, 0);
    Vec3 vy(0, 1, 0);
    Real dotX = dot(pNormal, vx);
    Real dotY = dot(pNormal, vy);
    Vec3 t1 = getNormalized(fabs(dotX) < fabs(dotY) ? cross(pNormal, vx) : cross(pNormal, vy));
    Vec3 t2 = getNormalized(cross(pNormal, t1));

    Vec3 pWaveNormal = tempSurfaceVec3[idx];
    Real ph = surfaceWaveH[idx];
    if (pWaveNormal.z == 0) {
      tempSurfaceFloat[idx] = 0;
    }
    else {

      LOOP_NEIGHBORS_BEGIN(surfacePoints, pPos, params.tangentRadius)
      Real nh = surfaceWaveH[idn];
      LOOP_GHOSTS_POS_BEGIN(surfacePoints.getPos(idn), params.tangentRadius)
      Vec3 dir = gPos - pPos;
      Real lengthDir = norm(dir);
      if (lengthDir < 1e-5)
        continue;
      Vec3 tangentDir = lengthDir * getNormalized(dir - dot(dir, pNormal) * pNormal);
      Real dirX = dot(tangentDir, t1);
      Real dirY = dot(tangentDir, t2);
      Real dz = nh - ph - (-pWaveNormal.x / pWaveNormal.z) * dirX -
                (-pWaveNormal.y / pWaveNormal.z) * dirY;
      Real w = weightSurfaceTangent(norm(pPos - gPos));
      wTotal += w;
      laplacian += clamp(w * 4 * dz / (lengthDir * lengthDir), Real(-100.), Real(100.));
      LOOP_GHOSTS_END
      LOOP_NEIGHBORS_END
      if (wTotal != 0) {
        tempSurfaceFloat[idx] = laplacian / wTotal;
      }
      else {
        tempSurfaceFloat[idx] = 0;
      }
    }
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  inline const ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveH;
  }
  typedef ParticleDataImpl<Real> type2;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceWaveLaplacians ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals, surfaceWaveH);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const ParticleDataImpl<Vec3> &surfaceNormals;
  const ParticleDataImpl<Real> &surfaceWaveH;
};

struct evolveWave : public KernelBase {
  evolveWave(const BasicParticleSystemWrapper &surfacePoints,
             ParticleDataImpl<Real> &surfaceWaveH,
             ParticleDataImpl<Real> &surfaceWaveDtH,
             const ParticleDataImpl<Real> &surfaceWaveSeed)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceWaveH(surfaceWaveH),
        surfaceWaveDtH(surfaceWaveDtH),
        surfaceWaveSeed(surfaceWaveSeed)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Real> &surfaceWaveH,
                 ParticleDataImpl<Real> &surfaceWaveDtH,
                 const ParticleDataImpl<Real> &surfaceWaveSeed) const
  {
    surfaceWaveDtH[idx] += params.waveSpeed * params.waveSpeed * params.dt * tempSurfaceFloat[idx];
    surfaceWaveDtH[idx] /= (1 + params.dt * params.waveDamping);
    surfaceWaveH[idx] += params.dt * surfaceWaveDtH[idx];
    surfaceWaveH[idx] /= (1 + params.dt * params.waveDamping);
    surfaceWaveH[idx] -= surfaceWaveSeed[idx];

    // clamp H and DtH (to prevent rare extreme behaviors)
    surfaceWaveDtH[idx] = clamp(surfaceWaveDtH[idx],
                                -params.waveMaxFrequency * params.waveMaxAmplitude,
                                params.waveMaxFrequency * params.waveMaxAmplitude);
    surfaceWaveH[idx] = clamp(
        surfaceWaveH[idx], -params.waveMaxAmplitude, params.waveMaxAmplitude);
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Real> &getArg1()
  {
    return surfaceWaveH;
  }
  typedef ParticleDataImpl<Real> type1;
  inline ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveDtH;
  }
  typedef ParticleDataImpl<Real> type2;
  inline const ParticleDataImpl<Real> &getArg3()
  {
    return surfaceWaveSeed;
  }
  typedef ParticleDataImpl<Real> type3;
  void runMessage()
  {
    debMsg("Executing kernel evolveWave ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceWaveH, surfaceWaveDtH, surfaceWaveSeed);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Real> &surfaceWaveH;
  ParticleDataImpl<Real> &surfaceWaveDtH;
  const ParticleDataImpl<Real> &surfaceWaveSeed;
};

struct computeSurfaceCurvature : public KernelBase {
  computeSurfaceCurvature(const BasicParticleSystemWrapper &surfacePoints,
                          const ParticleDataImpl<Vec3> &surfaceNormals)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceNormals(surfaceNormals)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 const ParticleDataImpl<Vec3> &surfaceNormals) const
  {
    Vec3 pPos = surfacePoints.getPos(idx);
    Real wTotal = 0;
    Real curv = 0;
    Vec3 pNormal = surfaceNormals[idx];

    LOOP_NEIGHBORS_BEGIN(surfacePoints, pPos, params.normalRadius)
    LOOP_GHOSTS_POS_NORMAL_BEGIN(
        surfacePoints.getPos(idn), surfaceNormals[idn], params.normalRadius)
    Vec3 dir = pPos - gPos;
    if (dot(pNormal, gNormal) < 0) {
      continue;
    }  // backfacing
    Real dist = norm(dir);
    if (dist < params.normalRadius / 100.f) {
      continue;
    }

    Real distn = dot(dir, pNormal);

    Real w = weightSurfaceNormal(dist);
    curv += w * distn;
    wTotal += w;
    LOOP_GHOSTS_END
    LOOP_NEIGHBORS_END
    if (wTotal != 0) {
      curv /= wTotal;
    }
    tempSurfaceFloat[idx] = fabs(curv);
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline const ParticleDataImpl<Vec3> &getArg1()
  {
    return surfaceNormals;
  }
  typedef ParticleDataImpl<Vec3> type1;
  void runMessage()
  {
    debMsg("Executing kernel computeSurfaceCurvature ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceNormals);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  const ParticleDataImpl<Vec3> &surfaceNormals;
};

struct smoothCurvature : public KernelBase {
  smoothCurvature(const BasicParticleSystemWrapper &surfacePoints,
                  ParticleDataImpl<Real> &surfaceWaveSource)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceWaveSource(surfaceWaveSource)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Real> &surfaceWaveSource) const
  {
    Vec3 pPos = surfacePoints.getPos(idx);
    Real curv = 0;
    Real wTotal = 0;

    LOOP_NEIGHBORS_BEGIN(surfacePoints, pPos, params.normalRadius)
    Real w = weightSurfaceNormal(norm(pPos - surfacePoints.getPos(idn)));
    curv += w * tempSurfaceFloat[idn];
    wTotal += w;
    LOOP_NEIGHBORS_END
    if (wTotal != 0) {
      curv /= wTotal;
    }
    surfaceWaveSource[idx] = curv;
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Real> &getArg1()
  {
    return surfaceWaveSource;
  }
  typedef ParticleDataImpl<Real> type1;
  void runMessage()
  {
    debMsg("Executing kernel smoothCurvature ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceWaveSource);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Real> &surfaceWaveSource;
};

struct seedWaves : public KernelBase {
  seedWaves(const BasicParticleSystemWrapper &surfacePoints,
            ParticleDataImpl<Real> &surfaceWaveSeed,
            ParticleDataImpl<Real> &surfaceWaveSeedAmplitude,
            ParticleDataImpl<Real> &surfaceWaveSource)
      : KernelBase(surfacePoints.size()),
        surfacePoints(surfacePoints),
        surfaceWaveSeed(surfaceWaveSeed),
        surfaceWaveSeedAmplitude(surfaceWaveSeedAmplitude),
        surfaceWaveSource(surfaceWaveSource)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 const BasicParticleSystemWrapper &surfacePoints,
                 ParticleDataImpl<Real> &surfaceWaveSeed,
                 ParticleDataImpl<Real> &surfaceWaveSeedAmplitude,
                 ParticleDataImpl<Real> &surfaceWaveSource) const
  {
    Real source = smoothstep(params.waveSeedingCurvatureThresholdRegionCenter -
                                 params.waveSeedingCurvatureThresholdRegionRadius,
                             params.waveSeedingCurvatureThresholdRegionCenter +
                                 params.waveSeedingCurvatureThresholdRegionRadius,
                             (Real)surfaceWaveSource[idx]) *
                      2.f -
                  1.f;
    Real freq = params.waveSeedFrequency;
    Real theta = params.dt * frameCount * params.waveSpeed * freq;
    Real costheta = cosf(theta);
    Real maxSeedAmplitude = params.waveMaxSeedingAmplitude * params.waveMaxAmplitude;

    surfaceWaveSeedAmplitude[idx] = clamp<Real>(surfaceWaveSeedAmplitude[idx] +
                                                    source * params.waveSeedStepSizeRatioOfMax *
                                                        maxSeedAmplitude,
                                                0.f,
                                                maxSeedAmplitude);
    surfaceWaveSeed[idx] = surfaceWaveSeedAmplitude[idx] * costheta;

    // source values for display (not used after this point anyway)
    surfaceWaveSource[idx] = (source >= 0) ? 1 : 0;
  }
  inline const BasicParticleSystemWrapper &getArg0()
  {
    return surfacePoints;
  }
  typedef BasicParticleSystemWrapper type0;
  inline ParticleDataImpl<Real> &getArg1()
  {
    return surfaceWaveSeed;
  }
  typedef ParticleDataImpl<Real> type1;
  inline ParticleDataImpl<Real> &getArg2()
  {
    return surfaceWaveSeedAmplitude;
  }
  typedef ParticleDataImpl<Real> type2;
  inline ParticleDataImpl<Real> &getArg3()
  {
    return surfaceWaveSource;
  }
  typedef ParticleDataImpl<Real> type3;
  void runMessage()
  {
    debMsg("Executing kernel seedWaves ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, surfacePoints, surfaceWaveSeed, surfaceWaveSeedAmplitude, surfaceWaveSource);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const BasicParticleSystemWrapper &surfacePoints;
  ParticleDataImpl<Real> &surfaceWaveSeed;
  ParticleDataImpl<Real> &surfaceWaveSeedAmplitude;
  ParticleDataImpl<Real> &surfaceWaveSource;
};

void surfaceWaves(const BasicParticleSystemWrapper &surfacePoints,
                  const ParticleDataImpl<Vec3> &surfaceNormals,
                  ParticleDataImpl<Real> &surfaceWaveH,
                  ParticleDataImpl<Real> &surfaceWaveDtH,
                  ParticleDataImpl<Real> &surfaceWaveSource,
                  ParticleDataImpl<Real> &surfaceWaveSeed,
                  ParticleDataImpl<Real> &surfaceWaveSeedAmplitude)
{
  addSeed(surfacePoints, surfaceWaveH, surfaceWaveSeed);
  computeSurfaceWaveNormal(surfacePoints, surfaceNormals, surfaceWaveH);
  computeSurfaceWaveLaplacians(surfacePoints, surfaceNormals, surfaceWaveH);
  evolveWave(surfacePoints, surfaceWaveH, surfaceWaveDtH, surfaceWaveSeed);
  computeSurfaceCurvature(surfacePoints, surfaceNormals);
  smoothCurvature(surfacePoints, surfaceWaveSource);
  seedWaves(surfacePoints, surfaceWaveSeed, surfaceWaveSeedAmplitude, surfaceWaveSource);
}

//
// **** main function ****
//

void particleSurfaceTurbulence(const FlagGrid &flags,
                               BasicParticleSystem &coarseParts,
                               ParticleDataImpl<Vec3> &coarsePartsPrevPos,
                               BasicParticleSystem &surfPoints,
                               ParticleDataImpl<Vec3> &surfaceNormals,
                               ParticleDataImpl<Real> &surfaceWaveH,
                               ParticleDataImpl<Real> &surfaceWaveDtH,
                               BasicParticleSystem &surfacePointsDisplaced,
                               ParticleDataImpl<Real> &surfaceWaveSource,
                               ParticleDataImpl<Real> &surfaceWaveSeed,
                               ParticleDataImpl<Real> &surfaceWaveSeedAmplitude,
                               int res,
                               Real outerRadius = 1.0f,
                               int surfaceDensity = 20,
                               int nbSurfaceMaintenanceIterations = 4,
                               Real dt = 0.005f,
                               Real waveSpeed = 16.0f,
                               Real waveDamping = 0.0f,
                               Real waveSeedFrequency = 4,
                               Real waveMaxAmplitude = 0.25f,
                               Real waveMaxFrequency = 800,
                               Real waveMaxSeedingAmplitude = 0.5,
                               Real waveSeedingCurvatureThresholdRegionCenter = 0.025f,
                               Real waveSeedingCurvatureThresholdRegionRadius = 0.01f,
                               Real waveSeedStepSizeRatioOfMax = 0.05f)
{
#if USE_CHRONO == 1
  static std::chrono::high_resolution_clock::time_point begin, end;
  end = std::chrono::high_resolution_clock::now();
  cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1000000000.f
       << " : time sim" << endl;
  begin = std::chrono::high_resolution_clock::now();
#endif

  // wrap data
  coarseParticles.points = &coarseParts;
  coarseParticlesPrevPos.points = &coarsePartsPrevPos;
  surfacePoints.points = &surfPoints;

  // copy parameters
  params.res = res;
  params.outerRadius = outerRadius;
  params.surfaceDensity = surfaceDensity;
  params.nbSurfaceMaintenanceIterations = nbSurfaceMaintenanceIterations;
  params.dt = dt;
  params.waveSpeed = waveSpeed;
  params.waveDamping = waveDamping;
  params.waveSeedFrequency = waveSeedFrequency;
  params.waveMaxAmplitude = waveMaxAmplitude;
  params.waveMaxFrequency = waveMaxFrequency;
  params.waveMaxSeedingAmplitude = waveMaxSeedingAmplitude;
  params.waveSeedingCurvatureThresholdRegionCenter = waveSeedingCurvatureThresholdRegionCenter;
  params.waveSeedingCurvatureThresholdRegionRadius = waveSeedingCurvatureThresholdRegionRadius;
  params.waveSeedStepSizeRatioOfMax = waveSeedStepSizeRatioOfMax;

  // compute other parameters
  params.innerRadius = params.outerRadius / 2.0;
  params.meanFineDistance = M_PI * (params.outerRadius + params.innerRadius) /
                            params.surfaceDensity;
  params.constraintA = logf(2.0f / (1.0f + weightKernelCoarseDensity(params.outerRadius +
                                                                     params.innerRadius))) /
                       (powf((params.outerRadius + params.innerRadius) / 2, 2) -
                        params.innerRadius * params.innerRadius);
  params.normalRadius = 0.5f * (params.outerRadius + params.innerRadius);
  params.tangentRadius = 2.1f * params.meanFineDistance;
  params.bndXm = params.bndYm = params.bndZm = 2;
  params.bndXp = params.bndYp = params.bndZp = params.res - 2;

  if (frameCount == 0) {

    // initialize accel grids
    accelCoarse.init(2.f * res / params.outerRadius);
    accelSurface.init(1.f * res / (2.f * params.meanFineDistance));

    // update coarse accel structure
    coarseParticles.updateAccel();

    // create surface points
    initFines(coarseParticles, surfacePoints, flags);

    // smooth surface
    surfaceMaintenance(coarseParticles,
                       surfacePoints,
                       surfaceNormals,
                       surfaceWaveH,
                       surfaceWaveDtH,
                       surfaceWaveSeed,
                       surfaceWaveSeedAmplitude,
                       6 * params.nbSurfaceMaintenanceIterations);

    // set wave values to zero
    for (int idx = 0; idx < surfacePoints.size(); idx++) {
      surfaceWaveH[idx] = 0;
      surfaceWaveDtH[idx] = 0;
      surfaceWaveSeed[idx] = 0;
      surfaceWaveSeedAmplitude[idx] = 0;
    }
  }
  else {

    // update coarse accel structure with previous coarse particles positions
    coarseParticlesPrevPos.updateAccel();

    // advect surface points following coarse particles
    advectSurfacePoints(surfacePoints, coarseParticles, coarseParticlesPrevPos);
    surfacePoints.updateAccel();

    // update acceleration structure for surface points
    coarseParticles.updateAccel();

    // surface maintenance
    surfaceMaintenance(coarseParticles,
                       surfacePoints,
                       surfaceNormals,
                       surfaceWaveH,
                       surfaceWaveDtH,
                       surfaceWaveSeed,
                       surfaceWaveSeedAmplitude,
                       params.nbSurfaceMaintenanceIterations);

    // surface waves
    surfaceWaves(surfacePoints,
                 surfaceNormals,
                 surfaceWaveH,
                 surfaceWaveDtH,
                 surfaceWaveSource,
                 surfaceWaveSeed,
                 surfaceWaveSeedAmplitude);
  }
  frameCount++;

  // save positions as previous positions for next step
  for (int id = 0; id < coarseParticles.size(); id++) {
    if ((coarseParticles.getStatus(id) & ParticleBase::PNEW) == 0 &&
        (coarseParticles.getStatus(id) & ParticleBase::PDELETE) == 0) {
      coarseParticlesPrevPos.setVec3(id, coarseParticles.getPos(id));
    }
  }

  // create displaced points for display
  surfacePointsDisplaced.clear();
  for (int idx = 0; idx < surfacePoints.size(); idx++) {
    if ((surfacePoints.getStatus(idx) & ParticleBase::PDELETE) == 0) {
      surfacePointsDisplaced.addParticle(surfacePoints.getPos(idx) +
                                         surfaceNormals[idx] * surfaceWaveH[idx]);
    }
  }

#if USE_CHRONO == 1
  end = std::chrono::high_resolution_clock::now();
  cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1000000000.f
       << " : time upres" << endl;
  begin = std::chrono::high_resolution_clock::now();
#endif
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "particleSurfaceTurbulence", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 0, &_lock);
      BasicParticleSystem &coarseParts = *_args.getPtr<BasicParticleSystem>(
          "coarseParts", 1, &_lock);
      ParticleDataImpl<Vec3> &coarsePartsPrevPos = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "coarsePartsPrevPos", 2, &_lock);
      BasicParticleSystem &surfPoints = *_args.getPtr<BasicParticleSystem>(
          "surfPoints", 3, &_lock);
      ParticleDataImpl<Vec3> &surfaceNormals = *_args.getPtr<ParticleDataImpl<Vec3>>(
          "surfaceNormals", 4, &_lock);
      ParticleDataImpl<Real> &surfaceWaveH = *_args.getPtr<ParticleDataImpl<Real>>(
          "surfaceWaveH", 5, &_lock);
      ParticleDataImpl<Real> &surfaceWaveDtH = *_args.getPtr<ParticleDataImpl<Real>>(
          "surfaceWaveDtH", 6, &_lock);
      BasicParticleSystem &surfacePointsDisplaced = *_args.getPtr<BasicParticleSystem>(
          "surfacePointsDisplaced", 7, &_lock);
      ParticleDataImpl<Real> &surfaceWaveSource = *_args.getPtr<ParticleDataImpl<Real>>(
          "surfaceWaveSource", 8, &_lock);
      ParticleDataImpl<Real> &surfaceWaveSeed = *_args.getPtr<ParticleDataImpl<Real>>(
          "surfaceWaveSeed", 9, &_lock);
      ParticleDataImpl<Real> &surfaceWaveSeedAmplitude = *_args.getPtr<ParticleDataImpl<Real>>(
          "surfaceWaveSeedAmplitude", 10, &_lock);
      int res = _args.get<int>("res", 11, &_lock);
      Real outerRadius = _args.getOpt<Real>("outerRadius", 12, 1.0f, &_lock);
      int surfaceDensity = _args.getOpt<int>("surfaceDensity", 13, 20, &_lock);
      int nbSurfaceMaintenanceIterations = _args.getOpt<int>(
          "nbSurfaceMaintenanceIterations", 14, 4, &_lock);
      Real dt = _args.getOpt<Real>("dt", 15, 0.005f, &_lock);
      Real waveSpeed = _args.getOpt<Real>("waveSpeed", 16, 16.0f, &_lock);
      Real waveDamping = _args.getOpt<Real>("waveDamping", 17, 0.0f, &_lock);
      Real waveSeedFrequency = _args.getOpt<Real>("waveSeedFrequency", 18, 4, &_lock);
      Real waveMaxAmplitude = _args.getOpt<Real>("waveMaxAmplitude", 19, 0.25f, &_lock);
      Real waveMaxFrequency = _args.getOpt<Real>("waveMaxFrequency", 20, 800, &_lock);
      Real waveMaxSeedingAmplitude = _args.getOpt<Real>(
          "waveMaxSeedingAmplitude", 21, 0.5, &_lock);
      Real waveSeedingCurvatureThresholdRegionCenter = _args.getOpt<Real>(
          "waveSeedingCurvatureThresholdRegionCenter", 22, 0.025f, &_lock);
      Real waveSeedingCurvatureThresholdRegionRadius = _args.getOpt<Real>(
          "waveSeedingCurvatureThresholdRegionRadius", 23, 0.01f, &_lock);
      Real waveSeedStepSizeRatioOfMax = _args.getOpt<Real>(
          "waveSeedStepSizeRatioOfMax", 24, 0.05f, &_lock);
      _retval = getPyNone();
      particleSurfaceTurbulence(flags,
                                coarseParts,
                                coarsePartsPrevPos,
                                surfPoints,
                                surfaceNormals,
                                surfaceWaveH,
                                surfaceWaveDtH,
                                surfacePointsDisplaced,
                                surfaceWaveSource,
                                surfaceWaveSeed,
                                surfaceWaveSeedAmplitude,
                                res,
                                outerRadius,
                                surfaceDensity,
                                nbSurfaceMaintenanceIterations,
                                dt,
                                waveSpeed,
                                waveDamping,
                                waveSeedFrequency,
                                waveMaxAmplitude,
                                waveMaxFrequency,
                                waveMaxSeedingAmplitude,
                                waveSeedingCurvatureThresholdRegionCenter,
                                waveSeedingCurvatureThresholdRegionRadius,
                                waveSeedStepSizeRatioOfMax);
      _args.check();
    }
    pbFinalizePlugin(parent, "particleSurfaceTurbulence", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("particleSurfaceTurbulence", e.what());
    return 0;
  }
}
static const Pb::Register _RP_particleSurfaceTurbulence("", "particleSurfaceTurbulence", _W_0);
extern "C" {
void PbRegister_particleSurfaceTurbulence()
{
  KEEP_UNUSED(_RP_particleSurfaceTurbulence);
}
}

void debugCheckParts(const BasicParticleSystem &parts, const FlagGrid &flags)
{
  for (int idx = 0; idx < parts.size(); idx++) {
    Vec3i p = toVec3i(parts.getPos(idx));
    if (!flags.isInBounds(p)) {
      debMsg("bad position??? " << idx << " " << parts.getPos(idx), 1);
      exit(1);
    }
  }
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "debugCheckParts", !noTiming);
    PyObject *_retval = nullptr;
    {
      ArgLocker _lock;
      const BasicParticleSystem &parts = *_args.getPtr<BasicParticleSystem>("parts", 0, &_lock);
      const FlagGrid &flags = *_args.getPtr<FlagGrid>("flags", 1, &_lock);
      _retval = getPyNone();
      debugCheckParts(parts, flags);
      _args.check();
    }
    pbFinalizePlugin(parent, "debugCheckParts", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("debugCheckParts", e.what());
    return 0;
  }
}
static const Pb::Register _RP_debugCheckParts("", "debugCheckParts", _W_1);
extern "C" {
void PbRegister_debugCheckParts()
{
  KEEP_UNUSED(_RP_debugCheckParts);
}
}

}  // namespace SurfaceTurbulence

}  // namespace Manta
