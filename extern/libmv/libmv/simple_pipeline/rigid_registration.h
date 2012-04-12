// Copyright (c) 2012 libmv authors.
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

#ifndef LIBMV_SIMPLE_PIPELINE_RIGID_REGISTRATION_H_
#define LIBMV_SIMPLE_PIPELINE_RIGID_REGISTRATION_H_

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

/*!
    Searched for an affine transformation of rigid 3D object defined by it's
    vertices positions from it's current state called points to it's desired
    state called reference points.

    Returns rotation matrix, per-component scale vector and translation which
    transforms points to the mot close state to reference_points.

    Return value is an average squared distance between reference state
    and transformed one.

    Minimzation of distance between point pairs is used to register such a
    rigid transformation and algorithm assumes that pairs of points are
    defined by point's index in a vector, so points with the same index
    belongs to the same pair.
 */
double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R,
                         Vec3 &S,
                         Vec3 &t);

/*!
 * Same as RigidRegistration but provides registration of rotation and translation only
 */
double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R,
                         Vec3 &t);

/*!
 * Same as RigidRegistration but provides registration of rotation only
 */
double RigidRegistration(const vector<Vec3> &reference_points,
                         const vector<Vec3> &points,
                         Mat3 &R);

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_RIGID_REGISTRATION_H_
