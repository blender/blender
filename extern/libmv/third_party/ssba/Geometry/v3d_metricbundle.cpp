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

#include "Geometry/v3d_metricbundle.h"

#if defined(V3DLIB_ENABLE_SUITESPARSE)

namespace
{

   typedef V3D::InlineMatrix<double, 2, 4> Matrix2x4d;
   typedef V3D::InlineMatrix<double, 4, 2> Matrix4x2d;
   typedef V3D::InlineMatrix<double, 2, 6> Matrix2x6d;

} // end namespace <>

namespace V3D
{

   void
   MetricBundleOptimizerBase::updateParametersA(VectorArray<double> const& deltaAi)
   {
      Vector3d T, omega;
      Matrix3x3d R0, dR;

      for (int i = _nNonvaryingA; i < _nParametersA; ++i)
      {
         T = _cams[i].getTranslation();
         T[0] += deltaAi[i][0];
         T[1] += deltaAi[i][1];
         T[2] += deltaAi[i][2];
         _cams[i].setTranslation(T);

         // Create incremental rotation using Rodriguez formula.
         R0 = _cams[i].getRotation();
         omega[0] = deltaAi[i][3];
         omega[1] = deltaAi[i][4];
         omega[2] = deltaAi[i][5];
         createRotationMatrixRodriguez(omega, dR);
         _cams[i].setRotation(dR * R0);
      } // end for (i)
   } // end MetricBundleOptimizerBase::updateParametersA()

   void
   MetricBundleOptimizerBase::updateParametersB(VectorArray<double> const& deltaBj)
   {
      for (int j = _nNonvaryingB; j < _nParametersB; ++j)
      {
         _Xs[j][0] += deltaBj[j][0];
         _Xs[j][1] += deltaBj[j][1];
         _Xs[j][2] += deltaBj[j][2];
      }
   } // end MetricBundleOptimizerBase::updateParametersB()

   void
   MetricBundleOptimizerBase::poseDerivatives(int i, int j, Vector3d& XX,
                                              Matrix3x6d& d_dRT, Matrix3x3d& d_dX) const
   {
      XX = _cams[i].transformPointIntoCameraSpace(_Xs[j]);

      // See Frank Dellaerts bundle adjustment tutorial.
      // d(dR * R0 * X + t)/d omega = -[R0 * X]_x
      Matrix3x3d J;
      makeCrossProductMatrix(XX - _cams[i].getTranslation(), J);
      scaleMatrixIP(-1.0, J);

      // Now the transformation from world coords into camera space is xx = Rx + T
      // Hence the derivative of x wrt. T is just the identity matrix.
      makeIdentityMatrix(d_dRT);
      copyMatrixSlice(J, 0, 0, 3, 3, d_dRT, 0, 3);

      // The derivative of Rx+T wrt x is just R.
      copyMatrix(_cams[i].getRotation(), d_dX);
   } // end MetricBundleOptimizerBase::poseDerivatives()


//----------------------------------------------------------------------

   void
   StdMetricBundleOptimizer::fillJacobians(Matrix<double>& Ak,
                                           Matrix<double>& Bk,
                                           Matrix<double>& Ck,
                                           int i, int j, int k)
   {
      Vector3d XX;
      Matrix3x6d d_dRT;
      Matrix3x3d d_dX;
      this->poseDerivatives(i, j, XX, d_dRT, d_dX);

      double const f  = _cams[i].getFocalLength();
      double const ar = _cams[i].getAspectRatio();

      Matrix2x3d dp_dX;
      double const bx = f / (XX[2] * XX[2]);
      double const by = ar * bx;
      dp_dX[0][0] = bx * XX[2]; dp_dX[0][1] = 0;          dp_dX[0][2] = -bx * XX[0];
      dp_dX[1][0] = 0;          dp_dX[1][1] = by * XX[2]; dp_dX[1][2] = -by * XX[1];

      multiply_A_B(dp_dX, d_dRT, Ak);
      multiply_A_B(dp_dX, d_dX, Bk);
   } // end StdMetricBundleOptimizer::fillJacobians()

 //----------------------------------------------------------------------

   void
   CommonInternalsMetricBundleOptimizer::fillJacobians(Matrix<double>& Ak,
                                                       Matrix<double>& Bk,
                                                       Matrix<double>& Ck,
                                                       int i, int j, int k)
   {
      double const focalLength = _K[0][0];

      Vector3d XX;
      Matrix3x6d dXX_dRT;
      Matrix3x3d dXX_dX;
      this->poseDerivatives(i, j, XX, dXX_dRT, dXX_dX);

      Vector2d xu; // undistorted image point
      xu[0] = XX[0] / XX[2];
      xu[1] = XX[1] / XX[2];

      Vector2d const xd = _distortion(xu); // distorted image point

      Matrix2x2d dp_dxd;
      dp_dxd[0][0] = focalLength; dp_dxd[0][1] = 0;
      dp_dxd[1][0] = 0;           dp_dxd[1][1] = _cachedAspectRatio * focalLength;

      {
         // First, lets do the derivative wrt the structure and motion parameters.
         Matrix2x3d dxu_dXX;
         dxu_dXX[0][0] = 1.0f / XX[2]; dxu_dXX[0][1] = 0;            dxu_dXX[0][2] = -XX[0]/(XX[2]*XX[2]);
         dxu_dXX[1][0] = 0;            dxu_dXX[1][1] = 1.0f / XX[2]; dxu_dXX[1][2] = -XX[1]/(XX[2]*XX[2]);

         Matrix2x2d dxd_dxu = _distortion.derivativeWrtUndistortedPoint(xu);

         Matrix2x2d dp_dxu = dp_dxd * dxd_dxu;
         Matrix2x3d dp_dXX = dp_dxu * dxu_dXX;

         multiply_A_B(dp_dXX, dXX_dRT, Ak);
         multiply_A_B(dp_dXX, dXX_dX, Bk);
      } // end scope

      switch (_mode)
      {
         case FULL_BUNDLE_RADIAL_TANGENTIAL:
         {
            Matrix2x2d dxd_dp1p2 = _distortion.derivativeWrtTangentialParameters(xu);
            Matrix2x2d d_dp1p2 = dp_dxd * dxd_dp1p2;
            copyMatrixSlice(d_dp1p2, 0, 0, 2, 2, Ck, 0, 5);
            // No break here!
         }
         case FULL_BUNDLE_RADIAL:
         {
            Matrix2x2d dxd_dk1k2 = _distortion.derivativeWrtRadialParameters(xu);
            Matrix2x2d d_dk1k2 = dp_dxd * dxd_dk1k2;
            copyMatrixSlice(d_dk1k2, 0, 0, 2, 2, Ck, 0, 3);
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH_PP:
         {
            Ck[0][1] = 1; Ck[0][2] = 0;
            Ck[1][1] = 0; Ck[1][2] = 1;
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH:
         {
            Ck[0][0] = xd[0];
            Ck[1][0] = xd[1];
         }
         case FULL_BUNDLE_METRIC:
         {
         }
      } // end switch
   } // end CommonInternalsMetricBundleOptimizer::fillJacobians()

   void
   CommonInternalsMetricBundleOptimizer::updateParametersC(Vector<double> const& deltaC)
   {
      switch (_mode)
      {
         case FULL_BUNDLE_RADIAL_TANGENTIAL:
         {
            _distortion.p1 += deltaC[5];
            _distortion.p2 += deltaC[6];
            // No break here!
         }
         case FULL_BUNDLE_RADIAL:
         {
            _distortion.k1 += deltaC[3];
            _distortion.k2 += deltaC[4];
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH_PP:
         {
            _K[0][2] += deltaC[1];
            _K[1][2] += deltaC[2];
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH:
         {
            _K[0][0] += deltaC[0];
            _K[1][1] = _cachedAspectRatio * _K[0][0];
         }
         case FULL_BUNDLE_METRIC:
         {
         }
      } // end switch
   } // end CommonInternalsMetricBundleOptimizer::updateParametersC()

 //----------------------------------------------------------------------

   void
   VaryingInternalsMetricBundleOptimizer::fillJacobians(Matrix<double>& Ak,
                                                        Matrix<double>& Bk,
                                                        Matrix<double>& Ck,
                                                        int i, int j, int k)
   {
      Vector3d XX;
      Matrix3x6d dXX_dRT;
      Matrix3x3d dXX_dX;
      this->poseDerivatives(i, j, XX, dXX_dRT, dXX_dX);

      Vector2d xu; // undistorted image point
      xu[0] = XX[0] / XX[2];
      xu[1] = XX[1] / XX[2];

      Vector2d const xd = _distortions[i](xu); // distorted image point

      double const focalLength = _cams[i].getFocalLength();
      double const aspectRatio = _cams[i].getAspectRatio();

      Matrix2x2d dp_dxd;
      dp_dxd[0][0] = focalLength; dp_dxd[0][1] = 0;
      dp_dxd[1][0] = 0;           dp_dxd[1][1] = aspectRatio * focalLength;

      {
         // First, lets do the derivative wrt the structure and motion parameters.
         Matrix2x3d dxu_dXX;
         dxu_dXX[0][0] = 1.0f / XX[2]; dxu_dXX[0][1] = 0;            dxu_dXX[0][2] = -XX[0]/(XX[2]*XX[2]);
         dxu_dXX[1][0] = 0;            dxu_dXX[1][1] = 1.0f / XX[2]; dxu_dXX[1][2] = -XX[1]/(XX[2]*XX[2]);

         Matrix2x2d dxd_dxu = _distortions[i].derivativeWrtUndistortedPoint(xu);

         Matrix2x2d dp_dxu = dp_dxd * dxd_dxu;
         Matrix2x3d dp_dXX = dp_dxu * dxu_dXX;

         Matrix2x6d dp_dRT;

         multiply_A_B(dp_dXX, dXX_dRT, dp_dRT);
         copyMatrixSlice(dp_dRT, 0, 0, 2, 6, Ak, 0, 0);
         multiply_A_B(dp_dXX, dXX_dX, Bk);
      } // end scope

      switch (_mode)
      {
         case FULL_BUNDLE_RADIAL_TANGENTIAL:
         {
            Matrix2x2d dxd_dp1p2 = _distortions[i].derivativeWrtTangentialParameters(xu);
            Matrix2x2d d_dp1p2 = dp_dxd * dxd_dp1p2;
            copyMatrixSlice(d_dp1p2, 0, 0, 2, 2, Ak, 0, 11);
            // No break here!
         }
         case FULL_BUNDLE_RADIAL:
         {
            Matrix2x2d dxd_dk1k2 = _distortions[i].derivativeWrtRadialParameters(xu);
            Matrix2x2d d_dk1k2 = dp_dxd * dxd_dk1k2;
            copyMatrixSlice(d_dk1k2, 0, 0, 2, 2, Ak, 0, 9);
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH_PP:
         {
            Ak[0][7] = 1; Ak[0][8] = 0;
            Ak[1][7] = 0; Ak[1][8] = 1;
            // No break here!
         }
         case FULL_BUNDLE_FOCAL_LENGTH:
         {
            Ak[0][6] = xd[0];
            Ak[1][6] = xd[1];
         }
         case FULL_BUNDLE_METRIC:
         {
         }
      } // end switch
   } // end VaryingInternalsMetricBundleOptimizer::fillJacobians()

   void
   VaryingInternalsMetricBundleOptimizer::updateParametersA(VectorArray<double> const& deltaAi)
   {
      Vector3d T, omega;
      Matrix3x3d R0, dR, K;

      for (int i = _nNonvaryingA; i < _nParametersA; ++i)
      {
         Vector<double> const& deltaA = deltaAi[i];

         T = _cams[i].getTranslation();
         T[0] += deltaA[0];
         T[1] += deltaA[1];
         T[2] += deltaA[2];
         _cams[i].setTranslation(T);

         // Create incremental rotation using Rodriguez formula.
         R0 = _cams[i].getRotation();
         omega[0] = deltaA[3];
         omega[1] = deltaA[4];
         omega[2] = deltaA[5];
         createRotationMatrixRodriguez(omega, dR);
         _cams[i].setRotation(dR * R0);

         K = _cams[i].getIntrinsic();

         switch (_mode)
         {
            case FULL_BUNDLE_RADIAL_TANGENTIAL:
            {
               _distortions[i].p1 += deltaA[11];
               _distortions[i].p2 += deltaA[12];
               // No break here!
            }
            case FULL_BUNDLE_RADIAL:
            {
               _distortions[i].k1 += deltaA[9];
               _distortions[i].k2 += deltaA[10];
               // No break here!
            }
            case FULL_BUNDLE_FOCAL_LENGTH_PP:
            {
               K[0][2] += deltaA[7];
               K[1][2] += deltaA[8];
               // No break here!
            }
            case FULL_BUNDLE_FOCAL_LENGTH:
            {
               double const ar = K[1][1] / K[0][0];
               K[0][0] += deltaA[6];
               K[1][1] = ar * K[0][0];
            }
            case FULL_BUNDLE_METRIC:
            {
            }
         } // end switch
         _cams[i].setIntrinsic(K);
      } // end for (i)
   } // end VaryingInternalsMetricBundleOptimizer::updateParametersC()

} // end namespace V3D

#endif // defined(V3DLIB_ENABLE_SUITESPARSE)
