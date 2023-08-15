/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Vectors and Matrices (useful type definitions)
 */

#include "VecMat.h"

#include "../system/Precision.h"

namespace Freestyle {

namespace Geometry {

typedef VecMat::Vec2<uint> Vec2u;
typedef VecMat::Vec2<int> Vec2i;
typedef VecMat::Vec2<float> Vec2f;
typedef VecMat::Vec2<double> Vec2d;
typedef VecMat::Vec2<real> Vec2r;

typedef VecMat::Vec3<uint> Vec3u;
typedef VecMat::Vec3<int> Vec3i;
typedef VecMat::Vec3<float> Vec3f;
typedef VecMat::Vec3<double> Vec3d;
typedef VecMat::Vec3<real> Vec3r;

typedef VecMat::HVec3<uint> HVec3u;
typedef VecMat::HVec3<int> HVec3i;
typedef VecMat::HVec3<float> HVec3f;
typedef VecMat::HVec3<double> HVec3d;
typedef VecMat::HVec3<real> HVec3r;

typedef VecMat::SquareMatrix<uint, 2> Matrix22u;
typedef VecMat::SquareMatrix<int, 2> Matrix22i;
typedef VecMat::SquareMatrix<float, 2> Matrix22f;
typedef VecMat::SquareMatrix<double, 2> Matrix22d;
typedef VecMat::SquareMatrix<real, 2> Matrix22r;

typedef VecMat::SquareMatrix<uint, 3> Matrix33u;
typedef VecMat::SquareMatrix<int, 3> Matrix33i;
typedef VecMat::SquareMatrix<float, 3> Matrix33f;
typedef VecMat::SquareMatrix<double, 3> Matrix33d;
typedef VecMat::SquareMatrix<real, 3> Matrix33r;

typedef VecMat::SquareMatrix<uint, 4> Matrix44u;
typedef VecMat::SquareMatrix<int, 4> Matrix44i;
typedef VecMat::SquareMatrix<float, 4> Matrix44f;
typedef VecMat::SquareMatrix<double, 4> Matrix44d;
typedef VecMat::SquareMatrix<real, 4> Matrix44r;

}  // end of namespace Geometry

} /* namespace Freestyle */
