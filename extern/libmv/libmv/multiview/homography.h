// Copyright (c) 2011 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_MULTIVIEW_HOMOGRAPHY_H_
#define LIBMV_MULTIVIEW_HOMOGRAPHY_H_

#include "libmv/numeric/numeric.h"

namespace libmv {

/**
 * 2D homography transformation estimation.
 * 
 * This function estimates the homography transformation from a list of 2D
 * correspondences which represents either:
 *
 * - 3D points on a plane, with a general moving camera.
 * - 3D points with a rotating camera (pure rotation).
 * - 3D points + different planar projections
 * 
 * \param x1 The first 2xN or 3xN matrix of euclidean or homogeneous points.
 * \param x2 The second 2xN or 3xN matrix of euclidean or homogeneous points.
 * \param  H The 3x3 homography transformation matrix (8 dof) such that
 *               x2 = H * x1   with       |a b c| 
 *                                    H = |d e f|
 *                                        |g h 1| 
 * \param expected_precision The expected precision in order for instance 
 *                           to accept almost homography matrices.
 *
 * \return True if the transformation estimation has succeeded.
 * \note There must be at least 4 non-colinear points.
 */
bool Homography2DFromCorrespondencesLinear(const Mat &x1,
                                           const Mat &x2,
                                           Mat3 *H,
                                           double expected_precision = 
                                             EigenDouble::dummy_precision());

/**
 * 3D Homography transformation estimation.
 * 
 * This function can be used in order to estimate the homography transformation
 * from a list of 3D correspondences.
 *
 * \param[in] x1 The first 4xN matrix of homogeneous points
 * \param[in] x2 The second 4xN matrix of homogeneous points
 * \param[out] H The 4x4 homography transformation matrix (15 dof) such that
 *               x2 = H * x1   with       |a b c d| 
 *                                    H = |e f g h|
 *                                        |i j k l|
 *                                        |m n o 1| 
 * \param[in] expected_precision The expected precision in order for instance 
 *        to accept almost homography matrices.
 * 
 * \return true if the transformation estimation has succeeded
 * 
 * \note Need at least 5 non coplanar points 
 * \note Points coordinates must be in homogeneous coordinates
 */
bool Homography3DFromCorrespondencesLinear(const Mat &x1,
                                           const Mat &x2,
                                           Mat4 *H,
                                           double expected_precision = 
                                             EigenDouble::dummy_precision());
} // namespace libmv

#endif  // LIBMV_MULTIVIEW_HOMOGRAPHY_H_
