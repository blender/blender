// -*- C++ -*-
/*
Copyright (c) 2008 University of North Carolina at Chapel Hill

This file is part of SSBA (Simple Sparse Bundle Adjustment).

SSBA is free software: you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

SSBA is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License along
with SSBA. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef V3D_CAMERA_MATRIX_H
#define V3D_CAMERA_MATRIX_H

#include "Math/v3d_linear.h"
#include "Geometry/v3d_distortion.h"

namespace V3D
{

   struct CameraMatrix
   {
         CameraMatrix()
         {
            makeIdentityMatrix(_K);
            makeIdentityMatrix(_R);
            makeZeroVector(_T);
            this->updateCachedValues(true, true);
         }

         CameraMatrix(double f, double cx, double cy)
         {
            makeIdentityMatrix(_K);
            _K[0][0] = f;
            _K[1][1] = f;
            _K[0][2] = cx;
            _K[1][2] = cy;
            makeIdentityMatrix(_R);
            makeZeroVector(_T);
            this->updateCachedValues(true, true);
         }

         CameraMatrix(Matrix3x3d const& K,
                      Matrix3x3d const& R,
                      Vector3d const& T)
            : _K(K), _R(R), _T(T)
         {
            this->updateCachedValues(true, true);
         }

         void setIntrinsic(Matrix3x3d const& K) { _K = K; this->updateCachedValues(true, false); }
         void setRotation(Matrix3x3d const& R) { _R = R; this->updateCachedValues(false, true); }
         void setTranslation(Vector3d const& T) { _T = T; this->updateCachedValues(false, true); }

         template <typename Mat>
         void setOrientation(Mat const& RT)
         {
            _R[0][0] = RT[0][0]; _R[0][1] = RT[0][1]; _R[0][2] = RT[0][2];
            _R[1][0] = RT[1][0]; _R[1][1] = RT[1][1]; _R[1][2] = RT[1][2];
            _R[2][0] = RT[2][0]; _R[2][1] = RT[2][1]; _R[2][2] = RT[2][2];
            _T[0]    = RT[0][3]; _T[1]    = RT[1][3]; _T[2]    = RT[2][3];
            this->updateCachedValues(false, true);
         }

         Matrix3x3d const& getIntrinsic()   const { return _K; }
         Matrix3x3d const& getRotation()    const { return _R; }
         Vector3d   const& getTranslation() const { return _T; }

         Matrix3x4d getOrientation() const
         {
            Matrix3x4d RT;
            RT[0][0] = _R[0][0]; RT[0][1] = _R[0][1]; RT[0][2] = _R[0][2];
            RT[1][0] = _R[1][0]; RT[1][1] = _R[1][1]; RT[1][2] = _R[1][2];
            RT[2][0] = _R[2][0]; RT[2][1] = _R[2][1]; RT[2][2] = _R[2][2];
            RT[0][3] = _T[0];    RT[1][3] = _T[1];    RT[2][3] = _T[2];
            return RT;
         }

         Matrix3x4d getProjection() const
         {
            Matrix3x4d const RT = this->getOrientation();
            return _K * RT;
         }

         double getFocalLength() const { return _K[0][0]; }
         double getAspectRatio() const { return _K[1][1] / _K[0][0]; }

         Vector2d getPrincipalPoint() const
         {
            Vector2d pp;
            pp[0] = _K[0][2];
            pp[1] = _K[1][2];
            return pp;
         }

         Vector2d projectPoint(Vector3d const& X) const
         {
            Vector3d q = _K*(_R*X + _T);
            Vector2d res;
            res[0] = q[0]/q[2]; res[1] = q[1]/q[2];
            return res;
         }

         template <typename Distortion>
         Vector2d projectPoint(Distortion const& distortion, Vector3d const& X) const
         {
            Vector3d XX = _R*X + _T;
            Vector2d p;
            p[0] = XX[0] / XX[2];
            p[1] = XX[1] / XX[2];
            p = distortion(p);

            Vector2d res;
            res[0] = _K[0][0] * p[0] + _K[0][1] * p[1] + _K[0][2];
            res[1] =                   _K[1][1] * p[1] + _K[1][2];
            return res;
         }

         Vector3d unprojectPixel(Vector2d const &p, double depth = 1) const
         {
            Vector3d pp;
            pp[0] = p[0]; pp[1] = p[1]; pp[2] = 1.0;
            Vector3d ray = _invK * pp;
            ray[0] *= depth/ray[2];
            ray[1] *= depth/ray[2];
            ray[2] = depth;
            ray = _Rt * ray;
            return _center + ray;
         }

         Vector3d transformPointIntoCameraSpace(Vector3d const& p) const
         {
            return _R*p + _T;
         }

         Vector3d transformPointFromCameraSpace(Vector3d const& p) const
         {
            return _Rt*(p-_T);
         }

         Vector3d transformDirectionFromCameraSpace(Vector3d const& dir) const
         {
            return _Rt*dir;
         }

         Vector3d const& cameraCenter() const
         {
            return _center;
         }

         Vector3d opticalAxis() const
         {
            return this->transformDirectionFromCameraSpace(makeVector3(0.0, 0.0, 1.0));
         }

         Vector3d upVector() const
         {
            return this->transformDirectionFromCameraSpace(makeVector3(0.0, 1.0, 0.0));
         }

         Vector3d rightVector() const
         {
            return this->transformDirectionFromCameraSpace(makeVector3(1.0, 0.0, 0.0));
         }

         Vector3d getRay(Vector2d const& p) const
         {
            Vector3d pp = makeVector3(p[0], p[1], 1.0);
            Vector3d ray = _invK * pp;
            ray = _Rt * ray;
            normalizeVector(ray);
            return ray;
         }

      protected:
         void updateCachedValues(bool intrinsic, bool orientation)
         {
            if (intrinsic) _invK = invertedMatrix(_K);

            if (orientation)
            {
               makeTransposedMatrix(_R, _Rt);
               _center = _Rt * (-1.0 * _T);
            }
         }

         Matrix3x3d _K, _R;
         Vector3d   _T;
         Matrix3x3d _invK, _Rt;
         Vector3d   _center;
   }; // end struct CameraMatrix

} // end namespace V3D

#endif
