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

#ifndef __PROJECTIONS_H__
#define __PROJECTIONS_H__

#include <stdio.h>
#include <stdlib.h>

#define CONTAINS_INDEX
#define GRID_DIMENSION 20

#if defined(_WIN32) && !(_MSC_VER >= 1900)
#  define isnan(n) _isnan(n)
#  define LONG __int64
#  define int64_t __int64
#else
#  include <stdint.h>
#endif

/**
 * Structures and classes for computing projections of triangles onto
 * separating axes during scan conversion
 *
 * @author Tao Ju
 */

extern const int vertmap[8][3];
extern const int centmap[3][3][3][2];
extern const int edgemap[12][2];
extern const int facemap[6][4];

/* Axes:
 *  0,  1,  2: cube face normals
 *
 *          3: triangle normal
 *
 *  4,  5,  6,
 *  7,  8,  9,
 * 10, 11, 12: cross of each triangle edge vector with each cube
 *             face normal
 */
#define NUM_AXES 13

/**
 * Structure for the projections inheritable from parent
 */
struct TriangleProjection {
  /// Projections of triangle (min and max)
  int64_t tri_proj[NUM_AXES][2];

  /// Normal of the triangle
  double norm[3];

  /// Index of polygon
  int index;
};

/* This is a projection for the cube against a single projection
   axis, see CubeTriangleIsect.cubeProj */
struct CubeProjection {
  int64_t origin;
  int64_t edges[3];
  int64_t min, max;
};

/**
 * Class for projections of cube / triangle vertices on the separating axes
 */
class CubeTriangleIsect {
 public:
  /// Inheritable portion
  TriangleProjection *inherit;

  /// Projections of the cube vertices
  CubeProjection cubeProj[NUM_AXES];

 public:
  CubeTriangleIsect()
  {
  }

  /**
   * Construction from a cube (axes aligned) and triangle
   */
  CubeTriangleIsect(int64_t cube[2][3], int64_t trig[3][3], int64_t error, int triind);

  /**
   * Construction from a parent CubeTriangleIsect object and the index of
   * the children
   */
  CubeTriangleIsect(CubeTriangleIsect *parent);

  unsigned char getBoxMask();

  /**
   * Shifting a cube to a new origin
   */
  void shift(int off[3]);

  /**
   * Method to test intersection of the triangle and the cube
   */
  int isIntersecting() const;

  int isIntersectingPrimary(int edgeInd) const;

  float getIntersectionPrimary(int edgeInd) const;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:CubeTriangleIsect")
#endif
};

#endif /* __PROJECTIONS_H__ */
