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

#ifndef __GEOMUTILS_H__
#define __GEOMUTILS_H__

/** \file
 * \ingroup freestyle
 * \brief Various tools for geometry
 */

#include <vector>

#include "Geom.h"

#include "../system/FreestyleConfig.h"

using namespace std;

namespace Freestyle {

using namespace Geometry;

namespace GeomUtils {

//
// Templated procedures
//
/////////////////////////////////////////////////////////////////////////////

/*! Computes the distance from a point P to a segment AB */
template<class T> real distPointSegment(const T &P, const T &A, const T &B)
{
  T AB, AP, BP;
  AB = B - A;
  AP = P - A;
  BP = P - B;

  real c1(AB * AP);
  if (c1 <= 0)
    return AP.norm();

  real c2(AB * AB);
  if (c2 <= c1)
    return BP.norm();

  real b = c1 / c2;
  T Pb, PPb;
  Pb = A + b * AB;
  PPb = P - Pb;

  return PPb.norm();
}

//
// Non-templated procedures
//
/////////////////////////////////////////////////////////////////////////////
typedef enum {
  DONT_INTERSECT,
  DO_INTERSECT,
  COLINEAR,
  COINCIDENT,
} intersection_test;

intersection_test intersect2dSeg2dSeg(const Vec2r &p1,
                                      const Vec2r &p2,  // first segment
                                      const Vec2r &p3,
                                      const Vec2r &p4,  // second segment
                                      Vec2r &res);      // found intersection point

intersection_test intersect2dLine2dLine(const Vec2r &p1,
                                        const Vec2r &p2,  // first segment
                                        const Vec2r &p3,
                                        const Vec2r &p4,  // second segment
                                        Vec2r &res);      // found intersection point

intersection_test intersect2dSeg2dSegParametric(const Vec2r &p1,
                                                const Vec2r &p2,  // first segment
                                                const Vec2r &p3,
                                                const Vec2r &p4,  // second segment
                                                real &t,          // I = P1 + t * P1P2)
                                                real &u,          // I = P3 + u * P3P4
                                                real epsilon = M_EPSILON);

/*! check whether a 2D segment intersect a 2D region or not */
bool intersect2dSeg2dArea(const Vec2r &min, const Vec2r &max, const Vec2r &A, const Vec2r &B);

/*! check whether a 2D segment is included in a 2D region or not */
bool include2dSeg2dArea(const Vec2r &min, const Vec2r &max, const Vec2r &A, const Vec2r &B);

/*! Box-triangle overlap test, adapted from Tomas Akenine-Möller code */
bool overlapTriangleBox(Vec3r &boxcenter, Vec3r &boxhalfsize, Vec3r triverts[3]);

/*! Fast, Minimum Storage Ray-Triangle Intersection, adapted from Tomas Möller and Ben Trumbore
 * code. */
bool intersectRayTriangle(const Vec3r &orig,
                          const Vec3r &dir,
                          const Vec3r &v0,
                          const Vec3r &v1,
                          const Vec3r &v2,
                          real &t,  // I = orig + t * dir
                          real &u,
                          real &v,  // I = (1 - u - v) * v0 + u * v1 + v * v2
                          const real epsilon = M_EPSILON);  // the epsilon to use

/*! Intersection between plane and ray adapted from Graphics Gems, Didier Badouel */
intersection_test intersectRayPlane(const Vec3r &orig,
                                    const Vec3r &dir,  // ray origin and direction
                                    // plane's normal and offset (plane = { P / P.N + d = 0 })
                                    const Vec3r &norm,
                                    const real d,
                                    real &t,                          // I = orig + t * dir
                                    const real epsilon = M_EPSILON);  // the epsilon to use

/*! Intersection Ray-Bounding box (axis aligned).
 *  Adapted from Williams et al, "An Efficient Robust Ray-Box Intersection Algorithm", JGT 10:1
 * (2005), pp. 49-54.
 */
bool intersectRayBBox(const Vec3r &orig,
                      const Vec3r &dir,  // ray origin and direction
                      const Vec3r &boxMin,
                      const Vec3r &boxMax,  // the bbox
                      // the interval in which at least on of the intersections must happen
                      real t0,
                      real t1,
                      real &tmin,  // Imin = orig + tmin * dir is the first intersection
                      real &tmax,  // Imax = orig + tmax * dir is the second intersection
                      real epsilon = M_EPSILON);  // the epsilon to use

/*! Checks whether 3D point P lies inside or outside of the triangle ABC */
bool includePointTriangle(const Vec3r &P, const Vec3r &A, const Vec3r &B, const Vec3r &C);

void transformVertex(const Vec3r &vert, const Matrix44r &matrix, Vec3r &res);

void transformVertices(const vector<Vec3r> &vertices, const Matrix44r &trans, vector<Vec3r> &res);

Vec3r rotateVector(const Matrix44r &mat, const Vec3r &v);

//
// Coordinates systems changing procedures
//
/////////////////////////////////////////////////////////////////////////////

/*! From world to image
 *  p
 *    point's coordinates expressed in world coordinates system
 *  q
 *    vector in which the result will be stored
 *  model_view_matrix
 *    The model view matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 *  projection_matrix
 *    The projection matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 *  viewport
 *    The viewport: x,y coordinates followed by width and height (OpenGL like viewport)
 */
void fromWorldToImage(const Vec3r &p,
                      Vec3r &q,
                      const real model_view_matrix[4][4],
                      const real projection_matrix[4][4],
                      const int viewport[4]);

/*! From world to image
 *  p
 *    point's coordinates expressed in world coordinates system
 *  q
 *    vector in which the result will be stored
 *  transform
 *    The transformation matrix (gathering model view and projection),
 *    expressed in line major order (OpenGL matrices are column major ordered)
 *  viewport
 *    The viewport: x,y coordinates followed by width and height (OpenGL like viewport)
 */
void fromWorldToImage(const Vec3r &p, Vec3r &q, const real transform[4][4], const int viewport[4]);

/*! Projects from world coordinates to camera coordinates
 *  Returns the point's coordinates expressed in the camera's
 *  coordinates system.
 *  p
 *    point's coordinates expressed in world coordinates system
 *  q
 *    vector in which the result will be stored
 *  model_view_matrix
 *    The model view matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 */
void fromWorldToCamera(const Vec3r &p, Vec3r &q, const real model_view_matrix[4][4]);

/*! Projects from World Coordinates to retina coordinates
 *  Returns the point's coordinates expressed in Retina system.
 *  p
 *    point's coordinates expressed in camera system
 *  q
 *    vector in which the result will be stored
 *  projection_matrix
 *    The projection matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 */
void fromCameraToRetina(const Vec3r &p, Vec3r &q, const real projection_matrix[4][4]);

/*! From retina to image.
 *  Returns the coordinates expressed in Image coordinates system.
 *  p
 *    point's coordinates expressed in retina system
 *  q
 *    vector in which the result will be stored
 *  viewport
 *    The viewport: x,y coordinates followed by width and height (OpenGL like viewport).
 */
void fromRetinaToImage(const Vec3r &p, Vec3r &q, const int viewport[4]);

/*! From image to retina
 *  p
 *    point's coordinates expressed in image system
 *  q
 *    vector in which the result will be stored
 *  viewport
 *    The viewport: x,y coordinates followed by width and height (OpenGL like viewport).
 */
void fromImageToRetina(const Vec3r &p, Vec3r &q, const int viewport[4]);

/*! computes the coordinates of q in the camera coordinates system,
 *  using the known z coordinates of the 3D point.
 *  That means that this method does not inverse any matrices,
 *  it only computes X and Y from x,y and Z)
 *  p
 *    point's coordinates expressed in retina system
 *  q
 *    vector in which the result will be stored
 *  projection_matrix
 *    The projection matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 */
void fromRetinaToCamera(const Vec3r &p, Vec3r &q, real z, const real projection_matrix[4][4]);

/*! Projects from camera coordinates to world coordinates
 *  Returns the point's coordinates expressed in the world's
 *  coordinates system.
 *  p
 *    point's coordinates expressed in the camera coordinates system
 *  q
 *    vector in which the result will be stored
 *  model_view_matrix
 *    The model view matrix expressed in line major order (OpenGL
 *    matrices are column major ordered)
 */
void fromCameraToWorld(const Vec3r &p, Vec3r &q, const real model_view_matrix[4][4]);

}  // end of namespace GeomUtils

} /* namespace Freestyle */

#endif  // __GEOMUTILS_H__
