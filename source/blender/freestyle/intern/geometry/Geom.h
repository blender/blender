/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GEOM_H__
#define __GEOM_H__

/** \file blender/freestyle/intern/geometry/Geom.h
 *  \ingroup freestyle
 *  \brief Vectors and Matrices (useful type definitions)
 *  \author Sylvain Paris
 *  \author Emmanuel Turquin
 *  \author Stephane Grabli
 *  \date 20/05/2003
 */

#include "VecMat.h"

#include "../system/Precision.h"

namespace Freestyle {

namespace Geometry {

typedef VecMat::Vec2<unsigned>  Vec2u;
typedef VecMat::Vec2<int>       Vec2i;
typedef VecMat::Vec2<float>     Vec2f;
typedef VecMat::Vec2<double>    Vec2d;
typedef VecMat::Vec2<real>      Vec2r;

typedef VecMat::Vec3<unsigned>  Vec3u;
typedef VecMat::Vec3<int>       Vec3i;
typedef VecMat::Vec3<float>     Vec3f;
typedef VecMat::Vec3<double>    Vec3d;
typedef VecMat::Vec3<real>      Vec3r;

typedef VecMat::HVec3<unsigned> HVec3u;
typedef VecMat::HVec3<int>      HVec3i;
typedef VecMat::HVec3<float>    HVec3f;
typedef VecMat::HVec3<double>   HVec3d;
typedef VecMat::HVec3<real>     HVec3r;

typedef VecMat::SquareMatrix<unsigned, 2>   Matrix22u;
typedef VecMat::SquareMatrix<int, 2>        Matrix22i;
typedef VecMat::SquareMatrix<float, 2>      Matrix22f;
typedef VecMat::SquareMatrix<double, 2>     Matrix22d;
typedef VecMat::SquareMatrix<real, 2>       Matrix22r;

typedef VecMat::SquareMatrix<unsigned, 3>   Matrix33u;
typedef VecMat::SquareMatrix<int, 3>        Matrix33i;
typedef VecMat::SquareMatrix<float, 3>      Matrix33f;
typedef VecMat::SquareMatrix<double, 3>     Matrix33d;
typedef VecMat::SquareMatrix<real, 3>       Matrix33r;

typedef VecMat::SquareMatrix<unsigned, 4>   Matrix44u;
typedef VecMat::SquareMatrix<int, 4>        Matrix44i;
typedef VecMat::SquareMatrix<float, 4>      Matrix44f;
typedef VecMat::SquareMatrix<double, 4>     Matrix44d;
typedef VecMat::SquareMatrix<real, 4>       Matrix44r;

} // end of namespace Geometry

} /* namespace Freestyle */

#endif // __GEOM_H__
