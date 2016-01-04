// Copyright (c) 2010 libmv authors.
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

#ifndef LIBMV_MULTIVIEW_EUCLIDEAN_RESECTION_H_
#define LIBMV_MULTIVIEW_EUCLIDEAN_RESECTION_H_

#include "libmv/numeric/numeric.h"
#include "libmv/multiview/projection.h"

namespace libmv {
namespace euclidean_resection {

enum ResectionMethod {
  RESECTION_ANSAR_DANIILIDIS,

  // The "EPnP" algorithm by Lepetit et al.
  // http://cvlab.epfl.ch/~lepetit/papers/lepetit_ijcv08.pdf
  RESECTION_EPNP,
  
  // The Procrustes PNP algorithm ("PPnP")
  // http://www.diegm.uniud.it/fusiello/papers/3dimpvt12-b.pdf
  RESECTION_PPNP
};

/**
 * Computes the extrinsic parameters, R and t for a calibrated camera
 * from 4 or more 3D points and their normalized images.
 *
 * \param x_camera  Image points in normalized camera coordinates e.g. x_camera
 *                   = inv(K) * x_image.
 * \param X_world   3D points in the world coordinate system
 * \param R         Solution for the camera rotation matrix
 * \param t         Solution for the camera translation vector
 * \param method    The resection method to use.
 */
bool EuclideanResection(const Mat2X &x_camera,
                        const Mat3X &X_world,
                        Mat3 *R, Vec3 *t,
                        ResectionMethod method = RESECTION_EPNP);

/**
 * Computes the extrinsic parameters, R and t for a calibrated camera
 * from 4 or more 3D points and their images.
 *
 * \param x_image   Image points in non-normalized image coordinates. The
 *                  coordates are laid out one per row. The matrix can be Nx2
 *                  or Nx3 for euclidean or homogenous 2D coordinates.
 * \param X_world   3D points in the world coordinate system
 * \param K         Intrinsic parameters camera matrix
 * \param R         Solution for the camera rotation matrix
 * \param t         Solution for the camera translation vector
 * \param method    Resection method
 */
bool EuclideanResection(const Mat &x_image,
                        const Mat3X &X_world,
                        const Mat3 &K,
                        Mat3 *R, Vec3 *t,
                        ResectionMethod method = RESECTION_EPNP);

/**
 * The absolute orientation algorithm recovers the transformation between a set
 * of 3D points, X and Xp such that:
 *
 *           Xp = R*X + t
 *
 * The recovery of the absolute orientation is implemented after this article:
 * Horn, Hilden, "Closed-form solution of absolute orientation using
 * orthonormal matrices"
 */
void AbsoluteOrientation(const Mat3X &X,
                         const Mat3X &Xp,
                         Mat3 *R,
                         Vec3 *t);

/**
 * Computes the extrinsic parameters, R and t for a calibrated camera from 4 or
 * more 3D points and their images.
 *
 * \param x_camera Image points in normalized camera coordinates, e.g.
 *                 x_camera=inv(K)*x_image
 * \param X_world  3D points in the world coordinate system
 * \param R        Solution for the camera rotation matrix
 * \param t        Solution for the camera translation vector
 *
 * This is the algorithm described in: "Linear Pose Estimation from Points or
 * Lines", by Ansar, A. and Daniilidis, PAMI 2003. vol. 25, no. 5.
 */
void EuclideanResectionAnsarDaniilidis(const Mat2X &x_camera,
                                       const Mat3X &X_world,
                                       Mat3 *R, Vec3 *t);
/**
 * Computes the extrinsic parameters, R and t for a calibrated camera from 4 or
 * more 3D points and their images.
 *
 * \param x_camera Image points in normalized camera coordinates,
 *                 e.g. x_camera = inv(K) * x_image
 * \param X_world 3D points in the world coordinate system
 * \param R       Solution for the camera rotation matrix
 * \param t       Solution for the camera translation vector
 *
 * This is the algorithm described in:
 * "{EP$n$P: An Accurate $O(n)$ Solution to the P$n$P Problem", by V. Lepetit
 * and F. Moreno-Noguer and P. Fua, IJCV 2009. vol. 81, no. 2
 * \note: the non-linear optimization is not implemented here.
 */
bool EuclideanResectionEPnP(const Mat2X &x_camera,
                            const Mat3X &X_world,
                            Mat3 *R, Vec3 *t);

/**
 * Computes the extrinsic parameters, R and t for a calibrated camera from 4 or
 * more 3D points and their images.
 *
 * \param x_camera Image points in normalized camera coordinates,
 *                 e.g. x_camera = inv(K) * x_image
 * \param X_world 3D points in the world coordinate system
 * \param R       Solution for the camera rotation matrix
 * \param t       Solution for the camera translation vector
 *
 * Straight from the paper:
 * http://www.diegm.uniud.it/fusiello/papers/3dimpvt12-b.pdf
 */
bool EuclideanResectionPPnP(const Mat2X &x_camera,
                            const Mat3X &X_world,
                            Mat3 *R, Vec3 *t);

}  // namespace euclidean_resection
}  // namespace libmv


#endif /* LIBMV_MULTIVIEW_EUCLIDEAN_RESECTION_H_ */
