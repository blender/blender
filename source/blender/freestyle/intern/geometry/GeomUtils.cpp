/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Various tools for geometry
 */

#include "GeomUtils.h"

namespace Freestyle {

namespace GeomUtils {

// This internal procedure is defined below.
bool intersect2dSegPoly(Vec2r *seg, Vec2r *poly, unsigned n);

bool intersect2dSeg2dArea(const Vec2r &min, const Vec2r &max, const Vec2r &A, const Vec2r &B)
{
  Vec2r seg[2];
  seg[0] = A;
  seg[1] = B;

  Vec2r poly[5];
  poly[0][0] = min[0];
  poly[0][1] = min[1];
  poly[1][0] = max[0];
  poly[1][1] = min[1];
  poly[2][0] = max[0];
  poly[2][1] = max[1];
  poly[3][0] = min[0];
  poly[3][1] = max[1];
  poly[4][0] = min[0];
  poly[4][1] = min[1];

  return intersect2dSegPoly(seg, poly, 4);
}

bool include2dSeg2dArea(const Vec2r &min, const Vec2r &max, const Vec2r &A, const Vec2r &B)
{
  if ((((max[0] > A[0]) && (A[0] > min[0])) && ((max[0] > B[0]) && (B[0] > min[0]))) &&
      (((max[1] > A[1]) && (A[1] > min[1])) && ((max[1] > B[1]) && (B[1] > min[1])))) {
    return true;
  }
  return false;
}

intersection_test intersect2dSeg2dSeg(
    const Vec2r &p1, const Vec2r &p2, const Vec2r &p3, const Vec2r &p4, Vec2r &res)
{
  real a1, a2, b1, b2, c1, c2;  // Coefficients of line eqns
  real r1, r2, r3, r4;          // 'Sign' values
  real denom, num;              // Intermediate values

  // Compute a1, b1, c1, where line joining points p1 and p2 is "a1 x  +  b1 y  +  c1  =  0".
  a1 = p2[1] - p1[1];
  b1 = p1[0] - p2[0];
  c1 = p2[0] * p1[1] - p1[0] * p2[1];

  // Compute r3 and r4.
  r3 = a1 * p3[0] + b1 * p3[1] + c1;
  r4 = a1 * p4[0] + b1 * p4[1] + c1;

  // Check signs of r3 and r4.  If both point 3 and point 4 lie on same side of line 1,
  // the line segments do not intersect.
  if (r3 != 0 && r4 != 0 && r3 * r4 > 0.0) {
    return (DONT_INTERSECT);
  }

  // Compute a2, b2, c2
  a2 = p4[1] - p3[1];
  b2 = p3[0] - p4[0];
  c2 = p4[0] * p3[1] - p3[0] * p4[1];

  // Compute r1 and r2
  r1 = a2 * p1[0] + b2 * p1[1] + c2;
  r2 = a2 * p2[0] + b2 * p2[1] + c2;

  // Check signs of r1 and r2.  If both point 1 and point 2 lie on same side of second line
  // segment, the line segments do not intersect.
  if (r1 != 0 && r2 != 0 && r1 * r2 > 0.0) {
    return (DONT_INTERSECT);
  }

  // Line segments intersect: compute intersection point.
  denom = a1 * b2 - a2 * b1;
  if (fabs(denom) < M_EPSILON) {
    return (COLINEAR);
  }

  num = b1 * c2 - b2 * c1;
  res[0] = num / denom;

  num = a2 * c1 - a1 * c2;
  res[1] = num / denom;

  return (DO_INTERSECT);
}

intersection_test intersect2dLine2dLine(
    const Vec2r &p1, const Vec2r &p2, const Vec2r &p3, const Vec2r &p4, Vec2r &res)
{
  real a1, a2, b1, b2, c1, c2;  // Coefficients of line eqns
  real denom, num;              // Intermediate values

  // Compute a1, b1, c1, where line joining points p1 and p2 is "a1 x  +  b1 y  +  c1  =  0".
  a1 = p2[1] - p1[1];
  b1 = p1[0] - p2[0];
  c1 = p2[0] * p1[1] - p1[0] * p2[1];

  // Compute a2, b2, c2
  a2 = p4[1] - p3[1];
  b2 = p3[0] - p4[0];
  c2 = p4[0] * p3[1] - p3[0] * p4[1];

  // Line segments intersect: compute intersection point.
  denom = a1 * b2 - a2 * b1;
  if (fabs(denom) < M_EPSILON) {
    return (COLINEAR);
  }

  num = b1 * c2 - b2 * c1;
  res[0] = num / denom;

  num = a2 * c1 - a1 * c2;
  res[1] = num / denom;

  return (DO_INTERSECT);
}

intersection_test intersect2dSeg2dSegParametric(const Vec2r &p1,
                                                const Vec2r &p2,
                                                const Vec2r &p3,
                                                const Vec2r &p4,
                                                real &t,
                                                real &u,
                                                real epsilon)
{
  real a1, a2, b1, b2, c1, c2;  // Coefficients of line eqns
  real r1, r2, r3, r4;          // 'Sign' values
  real denom, num;              // Intermediate values

  // Compute a1, b1, c1, where line joining points p1 and p2 is "a1 x  +  b1 y  +  c1  =  0".
  a1 = p2[1] - p1[1];
  b1 = p1[0] - p2[0];
  c1 = p2[0] * p1[1] - p1[0] * p2[1];

  // Compute r3 and r4.
  r3 = a1 * p3[0] + b1 * p3[1] + c1;
  r4 = a1 * p4[0] + b1 * p4[1] + c1;

  // Check signs of r3 and r4.  If both point 3 and point 4 lie on same side of line 1,
  // the line segments do not intersect.
  if (r3 != 0 && r4 != 0 && r3 * r4 > 0.0) {
    return (DONT_INTERSECT);
  }

  // Compute a2, b2, c2
  a2 = p4[1] - p3[1];
  b2 = p3[0] - p4[0];
  c2 = p4[0] * p3[1] - p3[0] * p4[1];

  // Compute r1 and r2
  r1 = a2 * p1[0] + b2 * p1[1] + c2;
  r2 = a2 * p2[0] + b2 * p2[1] + c2;

  // Check signs of r1 and r2.  If both point 1 and point 2 lie on same side of second line
  // segment, the line segments do not intersect.
  if (r1 != 0 && r2 != 0 && r1 * r2 > 0.0) {
    return (DONT_INTERSECT);
  }

  // Line segments intersect: compute intersection point.
  denom = a1 * b2 - a2 * b1;
  if (fabs(denom) < epsilon) {
    return (COLINEAR);
  }

  real d1, e1;

  d1 = p1[1] - p3[1];
  e1 = p1[0] - p3[0];

  num = -b2 * d1 - a2 * e1;
  t = num / denom;

  num = -b1 * d1 - a1 * e1;
  u = num / denom;

  return (DO_INTERSECT);
}

// AABB-triangle overlap test code by Tomas Akenine-Möller
// Function: int triBoxOverlap(real boxcenter[3], real boxhalfsize[3],real triverts[3][3]);
// History:
//   2001-03-05: released the code in its first version
//   2001-06-18: changed the order of the tests, faster
//
// Acknowledgement: Many thanks to Pierre Terdiman for suggestions and discussions on how to
// optimize code. Thanks to David Hunt for finding a ">="-bug!

#define X 0
#define Y 1
#define Z 2

#define FINDMINMAX(x0, x1, x2, min, max) \
  { \
    min = max = x0; \
    if (x1 < min) \
      min = x1; \
    if (x1 > max) \
      max = x1; \
    if (x2 < min) \
      min = x2; \
    if (x2 > max) \
      max = x2; \
  } \
  (void)0

//======================== X-tests ========================//
#define AXISTEST_X01(a, b, fa, fb) \
  { \
    p0 = a * v0[Y] - b * v0[Z]; \
    p2 = a * v2[Y] - b * v2[Z]; \
    if (p0 < p2) { \
      min = p0; \
      max = p2; \
    } \
    else { \
      min = p2; \
      max = p0; \
    } \
    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

#define AXISTEST_X2(a, b, fa, fb) \
  { \
    p0 = a * v0[Y] - b * v0[Z]; \
    p1 = a * v1[Y] - b * v1[Z]; \
    if (p0 < p1) { \
      min = p0; \
      max = p1; \
    } \
    else { \
      min = p1; \
      max = p0; \
    } \
    rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

//======================== Y-tests ========================//
#define AXISTEST_Y02(a, b, fa, fb) \
  { \
    p0 = -a * v0[X] + b * v0[Z]; \
    p2 = -a * v2[X] + b * v2[Z]; \
    if (p0 < p2) { \
      min = p0; \
      max = p2; \
    } \
    else { \
      min = p2; \
      max = p0; \
    } \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

#define AXISTEST_Y1(a, b, fa, fb) \
  { \
    p0 = -a * v0[X] + b * v0[Z]; \
    p1 = -a * v1[X] + b * v1[Z]; \
    if (p0 < p1) { \
      min = p0; \
      max = p1; \
    } \
    else { \
      min = p1; \
      max = p0; \
    } \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

//======================== Z-tests ========================//
#define AXISTEST_Z12(a, b, fa, fb) \
  { \
    p1 = a * v1[X] - b * v1[Y]; \
    p2 = a * v2[X] - b * v2[Y]; \
    if (p2 < p1) { \
      min = p2; \
      max = p1; \
    } \
    else { \
      min = p1; \
      max = p2; \
    } \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

#define AXISTEST_Z0(a, b, fa, fb) \
  { \
    p0 = a * v0[X] - b * v0[Y]; \
    p1 = a * v1[X] - b * v1[Y]; \
    if (p0 < p1) { \
      min = p0; \
      max = p1; \
    } \
    else { \
      min = p1; \
      max = p0; \
    } \
    rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y]; \
    if (min > rad || max < -rad) \
      return 0; \
  } \
  (void)0

// This internal procedure is defined below.
bool overlapPlaneBox(Vec3r &normal, real d, Vec3r &maxbox);

// Use separating axis theorem to test overlap between triangle and box need to test for overlap in
// these directions: 1) the {x,y,z}-directions (actually, since we use the AABB of the triangle we
// do not even need to test these) 2) normal of the triangle 3) crossproduct(edge from tri,
// {x,y,z}-directin) this gives 3x3=9 more tests
bool overlapTriangleBox(Vec3r &boxcenter, Vec3r &boxhalfsize, Vec3r triverts[3])
{
  Vec3r v0, v1, v2, normal, e0, e1, e2;
  real min, max, d, p0, p1, p2, rad, fex, fey, fez;

  // This is the fastest branch on Sun
  // move everything so that the boxcenter is in (0, 0, 0)
  v0 = triverts[0] - boxcenter;
  v1 = triverts[1] - boxcenter;
  v2 = triverts[2] - boxcenter;

  // compute triangle edges
  e0 = v1 - v0;
  e1 = v2 - v1;
  e2 = v0 - v2;

  // Bullet 3:
  // Do the 9 tests first (this was faster)
  fex = fabs(e0[X]);
  fey = fabs(e0[Y]);
  fez = fabs(e0[Z]);
  AXISTEST_X01(e0[Z], e0[Y], fez, fey);
  AXISTEST_Y02(e0[Z], e0[X], fez, fex);
  AXISTEST_Z12(e0[Y], e0[X], fey, fex);

  fex = fabs(e1[X]);
  fey = fabs(e1[Y]);
  fez = fabs(e1[Z]);
  AXISTEST_X01(e1[Z], e1[Y], fez, fey);
  AXISTEST_Y02(e1[Z], e1[X], fez, fex);
  AXISTEST_Z0(e1[Y], e1[X], fey, fex);

  fex = fabs(e2[X]);
  fey = fabs(e2[Y]);
  fez = fabs(e2[Z]);
  AXISTEST_X2(e2[Z], e2[Y], fez, fey);
  AXISTEST_Y1(e2[Z], e2[X], fez, fex);
  AXISTEST_Z12(e2[Y], e2[X], fey, fex);

  // Bullet 1:
  // first test overlap in the {x,y,z}-directions
  // find min, max of the triangle each direction, and test for overlap in that direction -- this
  // is equivalent to testing a minimal AABB around the triangle against the AABB

  // test in X-direction
  FINDMINMAX(v0[X], v1[X], v2[X], min, max);
  if (min > boxhalfsize[X] || max < -boxhalfsize[X]) {
    return false;
  }

  // test in Y-direction
  FINDMINMAX(v0[Y], v1[Y], v2[Y], min, max);
  if (min > boxhalfsize[Y] || max < -boxhalfsize[Y]) {
    return false;
  }

  // test in Z-direction
  FINDMINMAX(v0[Z], v1[Z], v2[Z], min, max);
  if (min > boxhalfsize[Z] || max < -boxhalfsize[Z]) {
    return false;
  }

  // Bullet 2:
  // test if the box intersects the plane of the triangle
  // compute plane equation of triangle: normal * x + d = 0
  normal = e0 ^ e1;
  d = -(normal * v0);  // plane eq: normal.x + d = 0
  if (!overlapPlaneBox(normal, d, boxhalfsize)) {
    return false;
  }

  return true;  // box and triangle overlaps
}

// Fast, Minimum Storage Ray-Triangle Intersection
//
// Tomas Möller
// Prosolvia Clarus AB
// Sweden
// tompa@clarus.se
//
// Ben Trumbore
// Cornell University
// Ithaca, New York
// wbt@graphics.cornell.edu
bool intersectRayTriangle(const Vec3r &orig,
                          const Vec3r &dir,
                          const Vec3r &v0,
                          const Vec3r &v1,
                          const Vec3r &v2,
                          real &t,
                          real &u,
                          real &v,
                          const real epsilon)
{
  Vec3r edge1, edge2, tvec, pvec, qvec;
  real det, inv_det;

  // find vectors for two edges sharing v0
  edge1 = v1 - v0;
  edge2 = v2 - v0;

  // begin calculating determinant - also used to calculate U parameter
  pvec = dir ^ edge2;

  // if determinant is near zero, ray lies in plane of triangle
  det = edge1 * pvec;

  // calculate distance from v0 to ray origin
  tvec = orig - v0;
  inv_det = 1.0 / det;

  qvec = tvec ^ edge1;

  if (det > epsilon) {
    u = tvec * pvec;
    if (u < 0.0 || u > det) {
      return false;
    }

    // calculate V parameter and test bounds
    v = dir * qvec;
    if (v < 0.0 || u + v > det) {
      return false;
    }
  }
  else if (det < -epsilon) {
    // calculate U parameter and test bounds
    u = tvec * pvec;
    if (u > 0.0 || u < det) {
      return false;
    }

    // calculate V parameter and test bounds
    v = dir * qvec;
    if (v > 0.0 || u + v < det) {
      return false;
    }
  }
  else {
    return false;  // ray is parallel to the plane of the triangle
  }

  u *= inv_det;
  v *= inv_det;
  t = (edge2 * qvec) * inv_det;

  return true;
}

// Intersection between plane and ray, adapted from Graphics Gems, Didier Badouel
// The plane is represented by a set of points P implicitly defined as dot(norm, P) + d = 0.
// The ray is represented as r(t) = orig + dir * t.
intersection_test intersectRayPlane(const Vec3r &orig,
                                    const Vec3r &dir,
                                    const Vec3r &norm,
                                    const real d,
                                    real &t,
                                    const real epsilon)
{
  real denom = norm * dir;

  if (fabs(denom) <= epsilon) {  // plane and ray are parallel
    if (fabs((norm * orig) + d) <= epsilon) {
      return COINCIDENT;  // plane and ray are coincident
    }
    else {
      return COLINEAR;
    }
  }

  t = -(d + (norm * orig)) / denom;

  if (t < 0.0f) {
    return DONT_INTERSECT;
  }

  return DO_INTERSECT;
}

bool intersectRayBBox(const Vec3r &orig,
                      const Vec3r &dir,  // ray origin and direction
                      const Vec3r &boxMin,
                      const Vec3r &boxMax,  // the bbox
                      real t0,
                      real t1,
                      real &tmin,  // I0 = orig + tmin * dir is the first intersection
                      real &tmax,  // I1 = orig + tmax * dir is the second intersection
                      real /*epsilon*/)
{
  float tymin, tymax, tzmin, tzmax;
  Vec3r inv_direction(1.0 / dir[0], 1.0 / dir[1], 1.0 / dir[2]);
  int sign[3];
  sign[0] = (inv_direction.x() < 0);
  sign[1] = (inv_direction.y() < 0);
  sign[2] = (inv_direction.z() < 0);

  Vec3r bounds[2];
  bounds[0] = boxMin;
  bounds[1] = boxMax;

  tmin = (bounds[sign[0]].x() - orig.x()) * inv_direction.x();
  tmax = (bounds[1 - sign[0]].x() - orig.x()) * inv_direction.x();
  tymin = (bounds[sign[1]].y() - orig.y()) * inv_direction.y();
  tymax = (bounds[1 - sign[1]].y() - orig.y()) * inv_direction.y();
  if ((tmin > tymax) || (tymin > tmax)) {
    return false;
  }
  if (tymin > tmin) {
    tmin = tymin;
  }
  if (tymax < tmax) {
    tmax = tymax;
  }
  tzmin = (bounds[sign[2]].z() - orig.z()) * inv_direction.z();
  tzmax = (bounds[1 - sign[2]].z() - orig.z()) * inv_direction.z();
  if ((tmin > tzmax) || (tzmin > tmax)) {
    return false;
  }
  if (tzmin > tmin) {
    tmin = tzmin;
  }
  if (tzmax < tmax) {
    tmax = tzmax;
  }
  return ((tmin < t1) && (tmax > t0));
}

// Checks whether 3D points p lies inside or outside of the triangle ABC
bool includePointTriangle(const Vec3r &P, const Vec3r &A, const Vec3r &B, const Vec3r &C)
{
  Vec3r AB(B - A);
  Vec3r BC(C - B);
  Vec3r CA(A - C);
  Vec3r AP(P - A);
  Vec3r BP(P - B);
  Vec3r CP(P - C);

  Vec3r N(AB ^ BC);  // triangle's normal

  N.normalize();

  Vec3r J(AB ^ AP), K(BC ^ BP), L(CA ^ CP);
  J.normalize();
  K.normalize();
  L.normalize();

  if (J * N < 0) {
    return false;  // on the right of AB
  }

  if (K * N < 0) {
    return false;  // on the right of BC
  }

  if (L * N < 0) {
    return false;  // on the right of CA
  }

  return true;
}

void transformVertex(const Vec3r &vert, const Matrix44r &matrix, Vec3r &res)
{
  HVec3r hvert(vert), res_tmp;
  real scale;
  for (unsigned int j = 0; j < 4; j++) {
    scale = hvert[j];
    for (unsigned int i = 0; i < 4; i++) {
      res_tmp[i] += matrix(i, j) * scale;
    }
  }

  res[0] = res_tmp.x();
  res[1] = res_tmp.y();
  res[2] = res_tmp.z();
}

void transformVertices(const vector<Vec3r> &vertices, const Matrix44r &trans, vector<Vec3r> &res)
{
  size_t i;
  res.resize(vertices.size());
  for (i = 0; i < vertices.size(); i++) {
    transformVertex(vertices[i], trans, res[i]);
  }
}

Vec3r rotateVector(const Matrix44r &mat, const Vec3r &v)
{
  Vec3r res;
  for (unsigned int i = 0; i < 3; i++) {
    res[i] = 0;
    for (unsigned int j = 0; j < 3; j++) {
      res[i] += mat(i, j) * v[j];
    }
  }
  res.normalize();
  return res;
}

// This internal procedure is defined below.
void fromCoordAToCoordB(const Vec3r &p, Vec3r &q, const real transform[4][4]);

void fromWorldToCamera(const Vec3r &p, Vec3r &q, const real model_view_matrix[4][4])
{
  fromCoordAToCoordB(p, q, model_view_matrix);
}

void fromCameraToRetina(const Vec3r &p, Vec3r &q, const real projection_matrix[4][4])
{
  fromCoordAToCoordB(p, q, projection_matrix);
}

void fromRetinaToImage(const Vec3r &p, Vec3r &q, const int viewport[4])
{
  // winX:
  q[0] = viewport[0] + viewport[2] * (p[0] + 1.0) / 2.0;

  // winY:
  q[1] = viewport[1] + viewport[3] * (p[1] + 1.0) / 2.0;

  // winZ:
  q[2] = (p[2] + 1.0) / 2.0;
}

void fromWorldToImage(const Vec3r &p,
                      Vec3r &q,
                      const real model_view_matrix[4][4],
                      const real projection_matrix[4][4],
                      const int viewport[4])
{
  Vec3r p1, p2;
  fromWorldToCamera(p, p1, model_view_matrix);
  fromCameraToRetina(p1, p2, projection_matrix);
  fromRetinaToImage(p2, q, viewport);
  q[2] = p1[2];
}

void fromWorldToImage(const Vec3r &p, Vec3r &q, const real transform[4][4], const int viewport[4])
{
  fromCoordAToCoordB(p, q, transform);

  // winX:
  q[0] = viewport[0] + viewport[2] * (q[0] + 1.0) / 2.0;

  // winY:
  q[1] = viewport[1] + viewport[3] * (q[1] + 1.0) / 2.0;
}

void fromImageToRetina(const Vec3r &p, Vec3r &q, const int viewport[4])
{
  q = p;
  q[0] = 2.0 * (q[0] - viewport[0]) / viewport[2] - 1.0;
  q[1] = 2.0 * (q[1] - viewport[1]) / viewport[3] - 1.0;
}

void fromRetinaToCamera(const Vec3r &p, Vec3r &q, real focal, const real projection_matrix[4][4])
{
  if (projection_matrix[3][3] == 0.0) {  // perspective
    q[0] = (-p[0] * focal) / projection_matrix[0][0];
    q[1] = (-p[1] * focal) / projection_matrix[1][1];
    q[2] = focal;
  }
  else {  // orthogonal
    q[0] = p[0] / projection_matrix[0][0];
    q[1] = p[1] / projection_matrix[1][1];
    q[2] = focal;
  }
}

void fromCameraToWorld(const Vec3r &p, Vec3r &q, const real model_view_matrix[4][4])
{
  real translation[3] = {
      model_view_matrix[0][3],
      model_view_matrix[1][3],
      model_view_matrix[2][3],
  };
  for (unsigned short i = 0; i < 3; i++) {
    q[i] = 0.0;
    for (unsigned short j = 0; j < 3; j++) {
      q[i] += model_view_matrix[j][i] * (p[j] - translation[j]);
    }
  }
}

//
// Internal code
//
/////////////////////////////////////////////////////////////////////////////

// Copyright 2001, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose providing that this copyright notice
// is included with it. SoftSurfer makes no warranty for this code, and cannot be held liable for
// any real or imagined damage resulting from its use. Users of this code must verify correctness
// for their application.

#define PERP(u, v) ((u)[0] * (v)[1] - (u)[1] * (v)[0])  // 2D perp product

inline bool intersect2dSegPoly(Vec2r *seg, Vec2r *poly, unsigned n)
{
  if (seg[0] == seg[1]) {
    return false;
  }

  real tE = 0;                   // the maximum entering segment parameter
  real tL = 1;                   // the minimum leaving segment parameter
  real t, N, D;                  // intersect parameter t = N / D
  Vec2r dseg = seg[1] - seg[0];  // the segment direction vector
  Vec2r e;                       // edge vector

  for (unsigned int i = 0; i < n; i++) {  // process polygon edge poly[i]poly[i+1]
    e = poly[i + 1] - poly[i];
    N = PERP(e, seg[0] - poly[i]);
    D = -PERP(e, dseg);
    if (fabs(D) < M_EPSILON) {
      if (N < 0) {
        return false;
      }
      else {
        continue;
      }
    }

    t = N / D;
    if (D < 0) {     // segment seg is entering across this edge
      if (t > tE) {  // new max tE
        tE = t;
        if (tE > tL) {  // seg enters after leaving polygon
          return false;
        }
      }
    }
    else {           // segment seg is leaving across this edge
      if (t < tL) {  // new min tL
        tL = t;
        if (tL < tE) {  // seg leaves before entering polygon
          return false;
        }
      }
    }
  }

  // tE <= tL implies that there is a valid intersection subsegment
  return true;
}

inline bool overlapPlaneBox(Vec3r &normal, real d, Vec3r &maxbox)
{
  Vec3r vmin, vmax;

  for (unsigned int q = X; q <= Z; q++) {
    if (normal[q] > 0.0f) {
      vmin[q] = -maxbox[q];
      vmax[q] = maxbox[q];
    }
    else {
      vmin[q] = maxbox[q];
      vmax[q] = -maxbox[q];
    }
  }
  if ((normal * vmin) + d > 0.0f) {
    return false;
  }
  if ((normal * vmax) + d >= 0.0f) {
    return true;
  }
  return false;
}

inline void fromCoordAToCoordB(const Vec3r &p, Vec3r &q, const real transform[4][4])
{
  HVec3r hp(p);
  HVec3r hq(0, 0, 0, 0);

  for (unsigned int i = 0; i < 4; i++) {
    for (unsigned int j = 0; j < 4; j++) {
      hq[i] += transform[i][j] * hp[j];
    }
  }

  if (hq[3] == 0) {
    q = p;
    return;
  }

  for (unsigned int k = 0; k < 3; k++) {
    q[k] = hq[k] / hq[3];
  }
}

}  // end of namespace GeomUtils

} /* namespace Freestyle */
