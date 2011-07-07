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

#ifndef V3D_METRICBUNDLE_H
#define V3D_METRICBUNDLE_H

# if defined(V3DLIB_ENABLE_SUITESPARSE)

#include "Math/v3d_optimization.h"
#include "Math/v3d_linear.h"
#include "Math/v3d_linear_utils.h"
#include "Geometry/v3d_cameramatrix.h"
#include "Geometry/v3d_distortion.h"

namespace V3D
{

   // This structure provides some helper functions common to all metric BAs
   struct MetricBundleOptimizerBase : public SparseLevenbergOptimizer
   {
         typedef SparseLevenbergOptimizer Base;

         MetricBundleOptimizerBase(double inlierThreshold,
                                   vector<CameraMatrix>& cams,
                                   vector<Vector3d >& Xs,
                                   vector<Vector2d > const& measurements,
                                   vector<int> const& corrspondingView,
                                   vector<int> const& corrspondingPoint,
                                   int nAddParamsA, int nParamsC)
            : SparseLevenbergOptimizer(2, cams.size(), 6+nAddParamsA, Xs.size(), 3, nParamsC,
                                       corrspondingView, corrspondingPoint),
              _cams(cams), _Xs(Xs), _measurements(measurements),
              _savedTranslations(cams.size()), _savedRotations(cams.size()),
              _savedXs(Xs.size()),
              _inlierThreshold(inlierThreshold), _cachedParamLength(0.0)
         {
            // Since we assume that BA does not alter the inputs too much,
            // we compute the overall length of the parameter vector in advance
            // and return that value as the result of getParameterLength().
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
            {
               _cachedParamLength += sqrNorm_L2(_cams[i].getTranslation());
               _cachedParamLength += 3.0; // Assume eye(3) for R.
            }
            for (int j = _nNonvaryingB; j < _nParametersB; ++j)
               _cachedParamLength += sqrNorm_L2(_Xs[j]);

            _cachedParamLength = sqrt(_cachedParamLength);
         }

         // Huber robust cost function.
         virtual void fillWeights(VectorArray<double> const& residual, Vector<double>& w)
         {
            for (unsigned int k = 0; k < w.size(); ++k)
            {
               Vector<double> const& r = residual[k];
               double const e = norm_L2(r);
               w[k] = (e < _inlierThreshold) ? 1.0 : sqrt(_inlierThreshold / e);
            } // end for (k)
         }

         virtual double getParameterLength() const
         {
            return _cachedParamLength;
         }

         virtual void updateParametersA(VectorArray<double> const& deltaAi);
         virtual void updateParametersB(VectorArray<double> const& deltaBj);
         virtual void updateParametersC(Vector<double> const& deltaC)
         {
            (void)deltaC;
         }

         virtual void saveAllParameters()
         {
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
            {
               _savedTranslations[i] = _cams[i].getTranslation();
               _savedRotations[i]    = _cams[i].getRotation();
            }
            _savedXs = _Xs;
         }

         virtual void restoreAllParameters()
         {
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
            {
               _cams[i].setTranslation(_savedTranslations[i]);
               _cams[i].setRotation(_savedRotations[i]);
            }
            _Xs = _savedXs;
         }

      protected:
         typedef InlineMatrix<double, 3, 6> Matrix3x6d;

         void poseDerivatives(int i, int j, Vector3d& XX,
                              Matrix3x6d& d_dRT, Matrix3x3d& d_dX) const;

         vector<CameraMatrix>& _cams;
         vector<Vector3d>&     _Xs;

         vector<Vector2d> const& _measurements;

         vector<Vector3d>   _savedTranslations;
         vector<Matrix3x3d> _savedRotations;
         vector<Vector3d>   _savedXs;

         double const _inlierThreshold;
         double       _cachedParamLength;
   }; // end struct MetricBundleOptimizerBase

   struct StdMetricBundleOptimizer : public MetricBundleOptimizerBase
   {
         typedef MetricBundleOptimizerBase Base;

         StdMetricBundleOptimizer(double inlierThreshold,
                                  vector<CameraMatrix>& cams,
                                  vector<Vector3d >& Xs,
                                  vector<Vector2d > const& measurements,
                                  vector<int> const& corrspondingView,
                                  vector<int> const& corrspondingPoint)
            : MetricBundleOptimizerBase(inlierThreshold, cams, Xs, measurements,
                                        corrspondingView, corrspondingPoint, 0, 0)
         { }

         virtual void evalResidual(VectorArray<double>& e)
         {
            for (unsigned int k = 0; k < e.count(); ++k)
            {
               int const i = _correspondingParamA[k];
               int const j = _correspondingParamB[k];

               Vector2d const q = _cams[i].projectPoint(_Xs[j]);
               e[k][0] = q[0] - _measurements[k][0];
               e[k][1] = q[1] - _measurements[k][1];
            }
         }

         virtual void fillJacobians(Matrix<double>& Ak, Matrix<double>& Bk, Matrix<double>& Ck,
                                    int i, int j, int k);
   }; // end struct StdMetricBundleOptimizer

//----------------------------------------------------------------------

   enum
   {
      FULL_BUNDLE_METRIC = 0,
      FULL_BUNDLE_FOCAL_LENGTH = 1,      // f
      FULL_BUNDLE_FOCAL_LENGTH_PP = 2,   // f, cx, cy
      FULL_BUNDLE_RADIAL = 3,            // f, cx, cy, k1, k2
      FULL_BUNDLE_RADIAL_TANGENTIAL = 4  // f, cx, cy, k1, k2, p1, p2
   };

   struct CommonInternalsMetricBundleOptimizer : public MetricBundleOptimizerBase
   {
         static int globalParamDimensionFromMode(int mode)
         {
            switch (mode)
            {
               case FULL_BUNDLE_METRIC:            return 0;
               case FULL_BUNDLE_FOCAL_LENGTH:      return 1;
               case FULL_BUNDLE_FOCAL_LENGTH_PP:   return 3;
               case FULL_BUNDLE_RADIAL:            return 5;
               case FULL_BUNDLE_RADIAL_TANGENTIAL: return 7;
            }
            return 0;
         }

         typedef MetricBundleOptimizerBase Base;

         CommonInternalsMetricBundleOptimizer(int mode,
                                              double inlierThreshold,
                                              Matrix3x3d& K,
                                              StdDistortionFunction& distortion,
                                              vector<CameraMatrix>& cams,
                                              vector<Vector3d >& Xs,
                                              vector<Vector2d > const& measurements,
                                              vector<int> const& corrspondingView,
                                              vector<int> const& corrspondingPoint)
            : MetricBundleOptimizerBase(inlierThreshold, cams, Xs, measurements,
                                        corrspondingView, corrspondingPoint,
                                        0, globalParamDimensionFromMode(mode)),
              _mode(mode), _K(K), _distortion(distortion)
         {
            _cachedAspectRatio = K[1][1] / K[0][0];
         }

         Vector2d projectPoint(Vector3d const& X, int i) const
         {
            Vector3d const XX = _cams[i].transformPointIntoCameraSpace(X);
            Vector2d p;
            p[0] = XX[0] / XX[2];
            p[1] = XX[1] / XX[2];
            p = _distortion(p);
            Vector2d res;
            res[0] = _K[0][0] * p[0] + _K[0][1] * p[1] + _K[0][2];
            res[1] =                   _K[1][1] * p[1] + _K[1][2];
            return res;
         }

         virtual void evalResidual(VectorArray<double>& e)
         {
            for (unsigned int k = 0; k < e.count(); ++k)
            {
               int const i = _correspondingParamA[k];
               int const j = _correspondingParamB[k];

               Vector2d const q = this->projectPoint(_Xs[j], i);
               e[k][0] = q[0] - _measurements[k][0];
               e[k][1] = q[1] - _measurements[k][1];
            }
         }

         virtual void fillJacobians(Matrix<double>& Ak, Matrix<double>& Bk, Matrix<double>& Ck,
                                    int i, int j, int k);

         virtual void updateParametersC(Vector<double> const& deltaC);

         virtual void saveAllParameters()
         {
            Base::saveAllParameters();
            _savedK = _K;
            _savedDistortion = _distortion;
         }

         virtual void restoreAllParameters()
         {
            Base::restoreAllParameters();
            _K = _savedK;
            _distortion = _savedDistortion;
         }

      protected:
         int                    _mode;
         Matrix3x3d&            _K;
         StdDistortionFunction& _distortion;

         Matrix3x3d            _savedK;
         StdDistortionFunction _savedDistortion;
         double                _cachedAspectRatio;
   }; // end struct CommonInternalsMetricBundleOptimizer

//----------------------------------------------------------------------

   struct VaryingInternalsMetricBundleOptimizer : public MetricBundleOptimizerBase
   {
         static int extParamDimensionFromMode(int mode)
         {
            switch (mode)
            {
               case FULL_BUNDLE_METRIC:            return 0;
               case FULL_BUNDLE_FOCAL_LENGTH:      return 1;
               case FULL_BUNDLE_FOCAL_LENGTH_PP:   return 3;
               case FULL_BUNDLE_RADIAL:            return 5;
               case FULL_BUNDLE_RADIAL_TANGENTIAL: return 7;
            }
            return 0;
         }

         typedef MetricBundleOptimizerBase Base;

         VaryingInternalsMetricBundleOptimizer(int mode,
                                               double inlierThreshold,
                                               std::vector<StdDistortionFunction>& distortions,
                                               vector<CameraMatrix>& cams,
                                               vector<Vector3d >& Xs,
                                               vector<Vector2d > const& measurements,
                                               vector<int> const& corrspondingView,
                                               vector<int> const& corrspondingPoint)
            : MetricBundleOptimizerBase(inlierThreshold, cams, Xs, measurements,
                                        corrspondingView, corrspondingPoint,
                                        extParamDimensionFromMode(mode), 0),
              _mode(mode), _distortions(distortions),
              _savedKs(cams.size()), _savedDistortions(cams.size())
         { }

         Vector2d projectPoint(Vector3d const& X, int i) const
         {
            return _cams[i].projectPoint(_distortions[i], X);
         }

         virtual void evalResidual(VectorArray<double>& e)
         {
            for (unsigned int k = 0; k < e.count(); ++k)
            {
               int const i = _correspondingParamA[k];
               int const j = _correspondingParamB[k];

               Vector2d const q = this->projectPoint(_Xs[j], i);
               e[k][0] = q[0] - _measurements[k][0];
               e[k][1] = q[1] - _measurements[k][1];
            }
         }

         virtual void fillJacobians(Matrix<double>& Ak, Matrix<double>& Bk, Matrix<double>& Ck,
                                    int i, int j, int k);

         virtual void updateParametersA(VectorArray<double> const& deltaAi);

         virtual void saveAllParameters()
         {
            Base::saveAllParameters();
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
               _savedKs[i] = _cams[i].getIntrinsic();
            std::copy(_distortions.begin(), _distortions.end(), _savedDistortions.begin());
         }

         virtual void restoreAllParameters()
         {
            Base::restoreAllParameters();
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
               _cams[i].setIntrinsic(_savedKs[i]);
            std::copy(_savedDistortions.begin(), _savedDistortions.end(), _distortions.begin());
         }

      protected:
         int                                 _mode;
         std::vector<StdDistortionFunction>& _distortions;

         std::vector<Matrix3x3d>            _savedKs;
         std::vector<StdDistortionFunction> _savedDistortions;
   }; // end struct VaryingInternalsMetricBundleOptimizer

} // end namespace V3D

# endif

#endif
