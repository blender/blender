

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
 * Shape classes
 *
 ******************************************************************************/

#include "shapes.h"
#include "commonkernels.h"
#include "mesh.h"

using namespace std;
namespace Manta {

//******************************************************************************
// Shape class members

Shape::Shape(FluidSolver *parent) : PbClass(parent), mType(TypeNone)
{
}

LevelsetGrid Shape::computeLevelset()
{
  // note - 3d check deactivated! TODO double check...
  LevelsetGrid phi(getParent());
  generateLevelset(phi);
  return phi;
}

bool Shape::isInside(const Vec3 &pos) const
{
  return false;
}

//! Kernel: Apply a shape to a grid, setting value inside

template<class T> struct ApplyShapeToGrid : public KernelBase {
  ApplyShapeToGrid(Grid<T> *grid, Shape *shape, T value, FlagGrid *respectFlags)
      : KernelBase(grid, 0), grid(grid), shape(shape), value(value), respectFlags(respectFlags)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, Grid<T> *grid, Shape *shape, T value, FlagGrid *respectFlags) const
  {
    if (respectFlags && respectFlags->isObstacle(i, j, k))
      return;
    if (shape->isInsideGrid(i, j, k))
      (*grid)(i, j, k) = value;
  }
  inline Grid<T> *getArg0()
  {
    return grid;
  }
  typedef Grid<T> type0;
  inline Shape *getArg1()
  {
    return shape;
  }
  typedef Shape type1;
  inline T &getArg2()
  {
    return value;
  }
  typedef T type2;
  inline FlagGrid *getArg3()
  {
    return respectFlags;
  }
  typedef FlagGrid type3;
  void runMessage()
  {
    debMsg("Executing kernel ApplyShapeToGrid ", 3);
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
            op(i, j, k, grid, shape, value, respectFlags);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, shape, value, respectFlags);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> *grid;
  Shape *shape;
  T value;
  FlagGrid *respectFlags;
};

//! Kernel: Apply a shape to a grid, setting value inside (scaling by SDF value)

template<class T> struct ApplyShapeToGridSmooth : public KernelBase {
  ApplyShapeToGridSmooth(
      Grid<T> *grid, Grid<Real> &phi, Real sigma, Real shift, T value, FlagGrid *respectFlags)
      : KernelBase(grid, 0),
        grid(grid),
        phi(phi),
        sigma(sigma),
        shift(shift),
        value(value),
        respectFlags(respectFlags)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 Grid<T> *grid,
                 Grid<Real> &phi,
                 Real sigma,
                 Real shift,
                 T value,
                 FlagGrid *respectFlags) const
  {
    if (respectFlags && respectFlags->isObstacle(i, j, k))
      return;
    const Real p = phi(i, j, k) - shift;
    if (p < -sigma)
      (*grid)(i, j, k) = value;
    else if (p < sigma)
      (*grid)(i, j, k) = value * (0.5f * (1.0f - p / sigma));
  }
  inline Grid<T> *getArg0()
  {
    return grid;
  }
  typedef Grid<T> type0;
  inline Grid<Real> &getArg1()
  {
    return phi;
  }
  typedef Grid<Real> type1;
  inline Real &getArg2()
  {
    return sigma;
  }
  typedef Real type2;
  inline Real &getArg3()
  {
    return shift;
  }
  typedef Real type3;
  inline T &getArg4()
  {
    return value;
  }
  typedef T type4;
  inline FlagGrid *getArg5()
  {
    return respectFlags;
  }
  typedef FlagGrid type5;
  void runMessage()
  {
    debMsg("Executing kernel ApplyShapeToGridSmooth ", 3);
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
            op(i, j, k, grid, phi, sigma, shift, value, respectFlags);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, phi, sigma, shift, value, respectFlags);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<T> *grid;
  Grid<Real> &phi;
  Real sigma;
  Real shift;
  T value;
  FlagGrid *respectFlags;
};

//! Kernel: Apply a shape to a MAC grid, setting value inside

struct ApplyShapeToMACGrid : public KernelBase {
  ApplyShapeToMACGrid(MACGrid *grid, Shape *shape, Vec3 value, FlagGrid *respectFlags)
      : KernelBase(grid, 0), grid(grid), shape(shape), value(value), respectFlags(respectFlags)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, MACGrid *grid, Shape *shape, Vec3 value, FlagGrid *respectFlags) const
  {
    if (respectFlags && respectFlags->isObstacle(i, j, k))
      return;
    if (shape->isInside(Vec3(i, j + 0.5, k + 0.5)))
      (*grid)(i, j, k).x = value.x;
    if (shape->isInside(Vec3(i + 0.5, j, k + 0.5)))
      (*grid)(i, j, k).y = value.y;
    if (shape->isInside(Vec3(i + 0.5, j + 0.5, k)))
      (*grid)(i, j, k).z = value.z;
  }
  inline MACGrid *getArg0()
  {
    return grid;
  }
  typedef MACGrid type0;
  inline Shape *getArg1()
  {
    return shape;
  }
  typedef Shape type1;
  inline Vec3 &getArg2()
  {
    return value;
  }
  typedef Vec3 type2;
  inline FlagGrid *getArg3()
  {
    return respectFlags;
  }
  typedef FlagGrid type3;
  void runMessage()
  {
    debMsg("Executing kernel ApplyShapeToMACGrid ", 3);
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
            op(i, j, k, grid, shape, value, respectFlags);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, grid, shape, value, respectFlags);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  MACGrid *grid;
  Shape *shape;
  Vec3 value;
  FlagGrid *respectFlags;
};

void Shape::applyToGrid(GridBase *grid, FlagGrid *respectFlags)
{
#if NOPYTHON != 1
  if (grid->getType() & GridBase::TypeInt)
    ApplyShapeToGrid<int>((Grid<int> *)grid, this, _args.get<int>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeReal)
    ApplyShapeToGrid<Real>((Grid<Real> *)grid, this, _args.get<Real>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeMAC)
    ApplyShapeToMACGrid((MACGrid *)grid, this, _args.get<Vec3>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeVec3)
    ApplyShapeToGrid<Vec3>((Grid<Vec3> *)grid, this, _args.get<Vec3>("value"), respectFlags);
  else
    errMsg("Shape::applyToGrid(): unknown grid type");
#else
  errMsg("Not yet supported...");
#endif
}

void Shape::applyToGridSmooth(GridBase *grid, Real sigma, Real shift, FlagGrid *respectFlags)
{
  Grid<Real> phi(grid->getParent());
  generateLevelset(phi);

#if NOPYTHON != 1
  if (grid->getType() & GridBase::TypeInt)
    ApplyShapeToGridSmooth<int>(
        (Grid<int> *)grid, phi, sigma, shift, _args.get<int>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeReal)
    ApplyShapeToGridSmooth<Real>(
        (Grid<Real> *)grid, phi, sigma, shift, _args.get<Real>("value"), respectFlags);
  else if (grid->getType() & GridBase::TypeVec3)
    ApplyShapeToGridSmooth<Vec3>(
        (Grid<Vec3> *)grid, phi, sigma, shift, _args.get<Vec3>("value"), respectFlags);
  else
    errMsg("Shape::applyToGridSmooth(): unknown grid type");
#else
  errMsg("Not yet supported...");
#endif
}

void Shape::collideMesh(Mesh &mesh)
{
  const Real margin = 0.2;

  Grid<Real> phi(getParent());
  Grid<Vec3> grad(getParent());
  generateLevelset(phi);
  GradientOp(grad, phi);

  const int num = mesh.numNodes();
  for (int i = 0; i < num; i++) {
    const Vec3 &p = mesh.nodes(i).pos;
    mesh.nodes(i).flags &= ~(Mesh::NfCollide | Mesh::NfMarked);
    if (!phi.isInBounds(p, 1))
      continue;

    for (int iter = 0; iter < 10; iter++) {
      const Real dist = phi.getInterpolated(p);
      if (dist < margin) {
        Vec3 n = grad.getInterpolated(p);
        normalize(n);
        mesh.nodes(i).pos += (margin - dist) * n;
        mesh.nodes(i).flags |= Mesh::NfCollide | Mesh::NfMarked;
      }
      else
        break;
    }
  }
}

//******************************************************************************
// Derived shape class members

Box::Box(FluidSolver *parent, Vec3 center, Vec3 p0, Vec3 p1, Vec3 size) : Shape(parent)
{
  mType = TypeBox;
  if (center.isValid() && size.isValid()) {
    mP0 = center - size;
    mP1 = center + size;
  }
  else if (p0.isValid() && p1.isValid()) {
    mP0 = p0;
    mP1 = p1;
  }
  else
    errMsg("Box: specify either p0,p1 or size,center");
}

bool Box::isInside(const Vec3 &pos) const
{
  return (pos.x >= mP0.x && pos.y >= mP0.y && pos.z >= mP0.z && pos.x <= mP1.x && pos.y <= mP1.y &&
          pos.z <= mP1.z);
}

void Box::generateMesh(Mesh *mesh)
{
  const int quadidx[24] = {0, 4, 6, 2, 3, 7, 5, 1, 0, 1, 5, 4, 6, 7, 3, 2, 0, 2, 3, 1, 5, 7, 6, 4};
  const int nodebase = mesh->numNodes();
  int oldtri = mesh->numTris();
  for (int i = 0; i < 8; i++) {
    Node p;
    p.flags = 0;
    p.pos = mP0;
    if (i & 1)
      p.pos.x = mP1.x;
    if (i & 2)
      p.pos.y = mP1.y;
    if (i & 4)
      p.pos.z = mP1.z;
    mesh->addNode(p);
  }
  for (int i = 0; i < 6; i++) {
    mesh->addTri(Triangle(nodebase + quadidx[i * 4 + 0],
                          nodebase + quadidx[i * 4 + 1],
                          nodebase + quadidx[i * 4 + 3]));
    mesh->addTri(Triangle(nodebase + quadidx[i * 4 + 1],
                          nodebase + quadidx[i * 4 + 2],
                          nodebase + quadidx[i * 4 + 3]));
  }
  mesh->rebuildCorners(oldtri, -1);
  mesh->rebuildLookup(oldtri, -1);
}

//! Kernel: Analytic SDF for box shape
struct BoxSDF : public KernelBase {
  BoxSDF(Grid<Real> &phi, const Vec3 &p1, const Vec3 &p2)
      : KernelBase(&phi, 0), phi(phi), p1(p1), p2(p2)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &phi, const Vec3 &p1, const Vec3 &p2) const
  {
    const Vec3 p(i + 0.5, j + 0.5, k + 0.5);
    if (p.x <= p2.x && p.x >= p1.x && p.y <= p2.y && p.y >= p1.y && p.z <= p2.z && p.z >= p1.z) {
      // inside: minimal surface distance
      Real mx = max(p.x - p2.x, p1.x - p.x);
      Real my = max(p.y - p2.y, p1.y - p.y);
      Real mz = max(p.z - p2.z, p1.z - p.z);
      if (!phi.is3D())
        mz = mx;  // skip for 2d...
      phi(i, j, k) = max(mx, max(my, mz));
    }
    else if (p.y <= p2.y && p.y >= p1.y && p.z <= p2.z && p.z >= p1.z) {
      // outside plane X
      phi(i, j, k) = max(p.x - p2.x, p1.x - p.x);
    }
    else if (p.x <= p2.x && p.x >= p1.x && p.z <= p2.z && p.z >= p1.z) {
      // outside plane Y
      phi(i, j, k) = max(p.y - p2.y, p1.y - p.y);
    }
    else if (p.x <= p2.x && p.x >= p1.x && p.y <= p2.y && p.y >= p1.y) {
      // outside plane Z
      phi(i, j, k) = max(p.z - p2.z, p1.z - p.z);
    }
    else if (p.x > p1.x && p.x < p2.x) {
      // lines X
      Real m1 = sqrt(square(p1.y - p.y) + square(p1.z - p.z));
      Real m2 = sqrt(square(p2.y - p.y) + square(p1.z - p.z));
      Real m3 = sqrt(square(p1.y - p.y) + square(p2.z - p.z));
      Real m4 = sqrt(square(p2.y - p.y) + square(p2.z - p.z));
      phi(i, j, k) = min(m1, min(m2, min(m3, m4)));
    }
    else if (p.y > p1.y && p.y < p2.y) {
      // lines Y
      Real m1 = sqrt(square(p1.x - p.x) + square(p1.z - p.z));
      Real m2 = sqrt(square(p2.x - p.x) + square(p1.z - p.z));
      Real m3 = sqrt(square(p1.x - p.x) + square(p2.z - p.z));
      Real m4 = sqrt(square(p2.x - p.x) + square(p2.z - p.z));
      phi(i, j, k) = min(m1, min(m2, min(m3, m4)));
    }
    else if (p.z > p1.x && p.z < p2.z) {
      // lines Z
      Real m1 = sqrt(square(p1.y - p.y) + square(p1.x - p.x));
      Real m2 = sqrt(square(p2.y - p.y) + square(p1.x - p.x));
      Real m3 = sqrt(square(p1.y - p.y) + square(p2.x - p.x));
      Real m4 = sqrt(square(p2.y - p.y) + square(p2.x - p.x));
      phi(i, j, k) = min(m1, min(m2, min(m3, m4)));
    }
    else {
      // points
      Real m = norm(p - Vec3(p1.x, p1.y, p1.z));
      m = min(m, norm(p - Vec3(p1.x, p1.y, p2.z)));
      m = min(m, norm(p - Vec3(p1.x, p2.y, p1.z)));
      m = min(m, norm(p - Vec3(p1.x, p2.y, p2.z)));
      m = min(m, norm(p - Vec3(p2.x, p1.y, p1.z)));
      m = min(m, norm(p - Vec3(p2.x, p1.y, p2.z)));
      m = min(m, norm(p - Vec3(p2.x, p2.y, p1.z)));
      m = min(m, norm(p - Vec3(p2.x, p2.y, p2.z)));
      phi(i, j, k) = m;
    }
  }
  inline Grid<Real> &getArg0()
  {
    return phi;
  }
  typedef Grid<Real> type0;
  inline const Vec3 &getArg1()
  {
    return p1;
  }
  typedef Vec3 type1;
  inline const Vec3 &getArg2()
  {
    return p2;
  }
  typedef Vec3 type2;
  void runMessage()
  {
    debMsg("Executing kernel BoxSDF ", 3);
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
            op(i, j, k, phi, p1, p2);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, phi, p1, p2);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &phi;
  const Vec3 &p1;
  const Vec3 &p2;
};
void Box::generateLevelset(Grid<Real> &phi)
{
  BoxSDF(phi, mP0, mP1);
}

Sphere::Sphere(FluidSolver *parent, Vec3 center, Real radius, Vec3 scale)
    : Shape(parent), mCenter(center), mScale(scale), mRadius(radius)
{
  mType = TypeSphere;
}

bool Sphere::isInside(const Vec3 &pos) const
{
  return normSquare((pos - mCenter) / mScale) <= mRadius * mRadius;
}

struct Tri {
  Vec3 t[3];
  int i[3];
  Tri(Vec3 a, Vec3 b, Vec3 c)
  {
    t[0] = a;
    t[1] = b;
    t[2] = c;
  }
};
void Sphere::generateMesh(Mesh *mesh)
{
  vector<Tri> tris;
  const int iterations = 3;
  int oldtri = mesh->numTris();

  // start with octahedron
  const Real d = sqrt(0.5);
  Vec3 p[6] = {Vec3(0, 1, 0),
               Vec3(0, -1, 0),
               Vec3(-d, 0, -d),
               Vec3(d, 0, -d),
               Vec3(d, 0, d),
               Vec3(-d, 0, d)};
  tris.push_back(Tri(p[0], p[4], p[3]));
  tris.push_back(Tri(p[0], p[5], p[4]));
  tris.push_back(Tri(p[0], p[2], p[5]));
  tris.push_back(Tri(p[0], p[3], p[2]));
  tris.push_back(Tri(p[1], p[3], p[4]));
  tris.push_back(Tri(p[1], p[4], p[5]));
  tris.push_back(Tri(p[1], p[5], p[2]));
  tris.push_back(Tri(p[1], p[2], p[3]));

  // Bisect each edge and move to the surface of a unit sphere
  for (int it = 0; it < iterations; it++) {
    int ntold = tris.size();
    for (int i = 0; i < ntold; i++) {
      Vec3 pa = 0.5 * (tris[i].t[0] + tris[i].t[1]);
      Vec3 pb = 0.5 * (tris[i].t[1] + tris[i].t[2]);
      Vec3 pc = 0.5 * (tris[i].t[2] + tris[i].t[0]);
      normalize(pa);
      normalize(pb);
      normalize(pc);

      tris.push_back(Tri(tris[i].t[0], pa, pc));
      tris.push_back(Tri(pa, tris[i].t[1], pb));
      tris.push_back(Tri(pb, tris[i].t[2], pc));
      tris[i].t[0] = pa;
      tris[i].t[1] = pb;
      tris[i].t[2] = pc;
    }
  }

  // index + scale
  vector<Vec3> nodes;
  for (size_t i = 0; i < tris.size(); i++) {
    for (int t = 0; t < 3; t++) {
      Vec3 p = mCenter + tris[i].t[t] * mRadius * mScale;
      // vector already there ?
      int idx = nodes.size();
      for (size_t j = 0; j < nodes.size(); j++) {
        if (p == nodes[j]) {
          idx = j;
          break;
        }
      }
      if (idx == (int)nodes.size())
        nodes.push_back(p);
      tris[i].i[t] = idx;
    }
  }

  // add the to mesh
  const int ni = mesh->numNodes();
  for (size_t i = 0; i < nodes.size(); i++) {
    mesh->addNode(Node(nodes[i]));
  }
  for (size_t t = 0; t < tris.size(); t++)
    mesh->addTri(Triangle(tris[t].i[0] + ni, tris[t].i[1] + ni, tris[t].i[2] + ni));

  mesh->rebuildCorners(oldtri, -1);
  mesh->rebuildLookup(oldtri, -1);
}

struct SphereSDF : public KernelBase {
  SphereSDF(Grid<Real> &phi, Vec3 center, Real radius, Vec3 scale)
      : KernelBase(&phi, 0), phi(phi), center(center), radius(radius), scale(scale)
  {
    runMessage();
    run();
  }
  inline void op(int i, int j, int k, Grid<Real> &phi, Vec3 center, Real radius, Vec3 scale) const
  {
    phi(i, j, k) = norm((Vec3(i + 0.5, j + 0.5, k + 0.5) - center) / scale) - radius;
  }
  inline Grid<Real> &getArg0()
  {
    return phi;
  }
  typedef Grid<Real> type0;
  inline Vec3 &getArg1()
  {
    return center;
  }
  typedef Vec3 type1;
  inline Real &getArg2()
  {
    return radius;
  }
  typedef Real type2;
  inline Vec3 &getArg3()
  {
    return scale;
  }
  typedef Vec3 type3;
  void runMessage()
  {
    debMsg("Executing kernel SphereSDF ", 3);
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
            op(i, j, k, phi, center, radius, scale);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, phi, center, radius, scale);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &phi;
  Vec3 center;
  Real radius;
  Vec3 scale;
};
void Sphere::generateLevelset(Grid<Real> &phi)
{
  SphereSDF(phi, mCenter, mRadius, mScale);
}

Cylinder::Cylinder(FluidSolver *parent, Vec3 center, Real radius, Vec3 z)
    : Shape(parent), mCenter(center), mRadius(radius)
{
  mType = TypeCylinder;
  mZDir = z;
  mZ = normalize(mZDir);
}

bool Cylinder::isInside(const Vec3 &pos) const
{
  Real z = dot(pos - mCenter, mZDir);
  if (fabs(z) > mZ)
    return false;
  Real r2 = normSquare(pos - mCenter) - square(z);
  return r2 < square(mRadius);
}

void Cylinder::generateMesh(Mesh *mesh)
{
  // generate coordinate system
  Vec3 x = getOrthogonalVector(mZDir) * mRadius;
  Vec3 y = cross(x, mZDir);
  Vec3 z = mZDir * mZ;
  int oldtri = mesh->numTris();

  // construct node ring
  const int N = 20;
  const int base = mesh->numNodes();
  for (int i = 0; i < N; i++) {
    const Real phi = 2.0 * M_PI * (Real)i / (Real)N;
    Vec3 r = x * cos(phi) + y * sin(phi) + mCenter;
    mesh->addNode(Node(r + z));
    mesh->addNode(Node(r - z));
  }
  // top/bottom center
  mesh->addNode(Node(mCenter + z));
  mesh->addNode(Node(mCenter - z));

  // connect with tris
  for (int i = 0; i < N; i++) {
    int cur = base + 2 * i;
    int next = base + 2 * ((i + 1) % N);
    // outside
    mesh->addTri(Triangle(cur, next, cur + 1));
    mesh->addTri(Triangle(next, next + 1, cur + 1));
    // upper / lower
    mesh->addTri(Triangle(cur, base + 2 * N, next));
    mesh->addTri(Triangle(cur + 1, next + 1, base + 2 * N + 1));
  }

  mesh->rebuildCorners(oldtri, -1);
  mesh->rebuildLookup(oldtri, -1);
}

struct CylinderSDF : public KernelBase {
  CylinderSDF(Grid<Real> &phi, Vec3 center, Real radius, Vec3 zaxis, Real maxz)
      : KernelBase(&phi, 0), phi(phi), center(center), radius(radius), zaxis(zaxis), maxz(maxz)
  {
    runMessage();
    run();
  }
  inline void op(
      int i, int j, int k, Grid<Real> &phi, Vec3 center, Real radius, Vec3 zaxis, Real maxz) const
  {
    Vec3 p = Vec3(i + 0.5, j + 0.5, k + 0.5) - center;
    Real z = fabs(dot(p, zaxis));
    Real r = sqrt(normSquare(p) - z * z);
    if (z < maxz) {
      // cylinder z area
      if (r < radius)
        phi(i, j, k) = max(r - radius, z - maxz);
      else
        phi(i, j, k) = r - radius;
    }
    else if (r < radius) {
      // cylinder top area
      phi(i, j, k) = fabs(z - maxz);
    }
    else {
      // edge
      phi(i, j, k) = sqrt(square(z - maxz) + square(r - radius));
    }
  }
  inline Grid<Real> &getArg0()
  {
    return phi;
  }
  typedef Grid<Real> type0;
  inline Vec3 &getArg1()
  {
    return center;
  }
  typedef Vec3 type1;
  inline Real &getArg2()
  {
    return radius;
  }
  typedef Real type2;
  inline Vec3 &getArg3()
  {
    return zaxis;
  }
  typedef Vec3 type3;
  inline Real &getArg4()
  {
    return maxz;
  }
  typedef Real type4;
  void runMessage()
  {
    debMsg("Executing kernel CylinderSDF ", 3);
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
            op(i, j, k, phi, center, radius, zaxis, maxz);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, phi, center, radius, zaxis, maxz);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  Grid<Real> &phi;
  Vec3 center;
  Real radius;
  Vec3 zaxis;
  Real maxz;
};
void Cylinder::generateLevelset(Grid<Real> &phi)
{
  CylinderSDF(phi, mCenter, mRadius, mZDir, mZ);
}

Slope::Slope(FluidSolver *parent, Real anglexy, Real angleyz, Real origin, Vec3 gs)
    : Shape(parent), mAnglexy(anglexy), mAngleyz(angleyz), mOrigin(origin), mGs(gs)
{
  mType = TypeSlope;
}

void Slope::generateMesh(Mesh *mesh)
{

  const int oldtri = mesh->numTris();

  Vec3 v1(0., mOrigin, 0.);
  mesh->addNode(Node(v1));

  Real dy1 = mGs.z * std::tan(mAngleyz);
  Vec3 v2(0., mOrigin - dy1, mGs.z);
  mesh->addNode(Node(v2));

  Real dy2 = mGs.x * std::tan(mAnglexy);
  Vec3 v3(mGs.x, v2.y - dy2, mGs.z);
  mesh->addNode(Node(v3));

  Vec3 v4(mGs.x, mOrigin - dy2, 0.);
  mesh->addNode(Node(v4));

  mesh->addTri(Triangle(0, 1, 2));
  mesh->addTri(Triangle(2, 3, 0));

  mesh->rebuildCorners(oldtri, -1);
  mesh->rebuildLookup(oldtri, -1);
}

bool Slope::isInside(const Vec3 &pos) const
{

  const Real alpha = -mAnglexy * M_PI / 180.;
  const Real beta = -mAngleyz * M_PI / 180.;

  Vec3 n(0, 1, 0);

  n.x = std::sin(alpha) * std::cos(beta);
  n.y = std::cos(alpha) * std::cos(beta);
  n.z = std::sin(beta);

  normalize(n);

  const Real fac = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);

  return ((n.x * (double)pos.x + n.y * (double)pos.y + n.z * (double)pos.z - mOrigin) / fac) <= 0.;
}

struct SlopeSDF : public KernelBase {
  SlopeSDF(const Vec3 &n, Grid<Real> &phiObs, const Real &fac, const Real &origin)
      : KernelBase(&phiObs, 0), n(n), phiObs(phiObs), fac(fac), origin(origin)
  {
    runMessage();
    run();
  }
  inline void op(int i,
                 int j,
                 int k,
                 const Vec3 &n,
                 Grid<Real> &phiObs,
                 const Real &fac,
                 const Real &origin) const
  {

    phiObs(i, j, k) = (n.x * (double)i + n.y * (double)j + n.z * (double)k - origin) * fac;
  }
  inline const Vec3 &getArg0()
  {
    return n;
  }
  typedef Vec3 type0;
  inline Grid<Real> &getArg1()
  {
    return phiObs;
  }
  typedef Grid<Real> type1;
  inline const Real &getArg2()
  {
    return fac;
  }
  typedef Real type2;
  inline const Real &getArg3()
  {
    return origin;
  }
  typedef Real type3;
  void runMessage()
  {
    debMsg("Executing kernel SlopeSDF ", 3);
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
            op(i, j, k, n, phiObs, fac, origin);
    }
    else {
      const int k = 0;
      for (int j = __r.begin(); j != (int)__r.end(); j++)
        for (int i = 0; i < _maxX; i++)
          op(i, j, k, n, phiObs, fac, origin);
    }
  }
  void run()
  {
    if (maxZ > 1)
      tbb::parallel_for(tbb::blocked_range<IndexInt>(minZ, maxZ), *this);
    else
      tbb::parallel_for(tbb::blocked_range<IndexInt>(0, maxY), *this);
  }
  const Vec3 &n;
  Grid<Real> &phiObs;
  const Real &fac;
  const Real &origin;
};

void Slope::generateLevelset(Grid<Real> &phi)
{

  const Real alpha = -mAnglexy * M_PI / 180.;
  const Real beta = -mAngleyz * M_PI / 180.;

  Vec3 n(0, 1, 0);

  n.x = std::sin(alpha) * std::cos(beta);
  n.y = std::cos(alpha) * std::cos(beta);
  n.z = std::sin(beta);

  normalize(n);

  const Real fac = 1. / std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);

  SlopeSDF(n, phi, fac, mOrigin);
}

}  // namespace Manta
