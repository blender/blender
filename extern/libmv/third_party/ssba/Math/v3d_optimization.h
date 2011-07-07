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

#ifndef V3D_OPTIMIZATION_H
#define V3D_OPTIMIZATION_H

#include "Math/v3d_linear.h"
#include "Math/v3d_mathutilities.h"

#include <vector>
#include <iostream>

namespace V3D
{

   enum
   {
      LEVENBERG_OPTIMIZER_TIMEOUT = 0,
      LEVENBERG_OPTIMIZER_SMALL_UPDATE = 1,
      LEVENBERG_OPTIMIZER_CONVERGED = 2
   };

   extern int optimizerVerbosenessLevel;

   struct LevenbergOptimizerCommon
   {
         LevenbergOptimizerCommon()
            : status(LEVENBERG_OPTIMIZER_TIMEOUT), currentIteration(0), maxIterations(50),
              tau(1e-3), lambda(1e-3),
              gradientThreshold(1e-10), updateThreshold(1e-10),
              _nu(2.0)
         { }
         virtual ~LevenbergOptimizerCommon() {}

         // See Madsen et al., "Methods for non-linear least squares problems."
         virtual void increaseLambda()
         {
            lambda *= _nu; _nu *= 2.0;
         }

         virtual void decreaseLambda(double const rho)
         {
            double const r = 2*rho - 1.0;
            lambda *= std::max(1.0/3.0, 1 - r*r*r);
            if (lambda < 1e-10) lambda = 1e-10;
            _nu = 2;
         }

         bool applyGradientStoppingCriteria(double maxGradient) const
         {
            return maxGradient < gradientThreshold;
         }

         bool applyUpdateStoppingCriteria(double paramLength, double updateLength) const
         {
            return updateLength < updateThreshold * (paramLength + updateThreshold);
         }

         int    status;
         int    currentIteration, maxIterations;
         double tau, lambda;
         double gradientThreshold, updateThreshold;

      protected:
         double _nu;
   }; // end struct LevenbergOptimizerCommon

# if defined(V3DLIB_ENABLE_SUITESPARSE)

   struct SparseLevenbergOptimizer : public LevenbergOptimizerCommon
   {
         SparseLevenbergOptimizer(int measurementDimension,
                                  int nParametersA, int paramDimensionA,
                                  int nParametersB, int paramDimensionB,
                                  int paramDimensionC,
                                  std::vector<int> const& correspondingParamA,
                                  std::vector<int> const& correspondingParamB)
            : LevenbergOptimizerCommon(),
              _nMeasurements(correspondingParamA.size()),
              _measurementDimension(measurementDimension),
              _nParametersA(nParametersA), _paramDimensionA(paramDimensionA),
              _nParametersB(nParametersB), _paramDimensionB(paramDimensionB),
              _paramDimensionC(paramDimensionC),
              _nNonvaryingA(0), _nNonvaryingB(0), _nNonvaryingC(0),
              _correspondingParamA(correspondingParamA),
              _correspondingParamB(correspondingParamB)
         {
            assert(correspondingParamA.size() == correspondingParamB.size());
         }

         ~SparseLevenbergOptimizer() { }

         void setNonvaryingCounts(int nNonvaryingA, int nNonvaryingB, int nNonvaryingC)
         {
            _nNonvaryingA = nNonvaryingA;
            _nNonvaryingB = nNonvaryingB;
            _nNonvaryingC = nNonvaryingC;
         }

         void getNonvaryingCounts(int& nNonvaryingA, int& nNonvaryingB, int& nNonvaryingC) const
         {
            nNonvaryingA = _nNonvaryingA;
            nNonvaryingB = _nNonvaryingB;
            nNonvaryingC = _nNonvaryingC;
         }

         void minimize();

         virtual void evalResidual(VectorArray<double>& residuals) = 0;

         virtual void fillWeights(VectorArray<double> const& residuals, Vector<double>& w)
         {
            (void)residuals;
            std::fill(w.begin(), w.end(), 1.0);
         }

         void fillAllJacobians(Vector<double> const& w,
                               MatrixArray<double>& Ak,
                               MatrixArray<double>& Bk,
                               MatrixArray<double>& Ck)
         {
            int const nVaryingA = _nParametersA - _nNonvaryingA;
            int const nVaryingB = _nParametersB - _nNonvaryingB;
            int const nVaryingC = _paramDimensionC - _nNonvaryingC;

            for (unsigned k = 0; k < _nMeasurements; ++k)
            {
               int const i = _correspondingParamA[k];
               int const j = _correspondingParamB[k];

               if (i < _nNonvaryingA && j < _nNonvaryingB) continue;

               fillJacobians(Ak[k], Bk[k], Ck[k], i, j, k);
            } // end for (k)

            if (nVaryingA > 0)
            {
               for (unsigned k = 0; k < _nMeasurements; ++k)
                  scaleMatrixIP(w[k], Ak[k]);
            }
            if (nVaryingB > 0)
            {
               for (unsigned k = 0; k < _nMeasurements; ++k)
                  scaleMatrixIP(w[k], Bk[k]);
            }
            if (nVaryingC > 0)
            {
               for (unsigned k = 0; k < _nMeasurements; ++k)
                  scaleMatrixIP(w[k], Ck[k]);
            }
         } // end fillAllJacobians()

         virtual void setupJacobianGathering() { }

         virtual void fillJacobians(Matrix<double>& Ak, Matrix<double>& Bk, Matrix<double>& Ck,
                                    int i, int j, int k) = 0;

         virtual double getParameterLength() const = 0;

         virtual void updateParametersA(VectorArray<double> const& deltaAi) = 0;
         virtual void updateParametersB(VectorArray<double> const& deltaBj) = 0;
         virtual void updateParametersC(Vector<double> const& deltaC) = 0;
         virtual void saveAllParameters() = 0;
         virtual void restoreAllParameters() = 0;

         int currentIteration, maxIterations;

      protected:
         void serializeNonZerosJtJ(std::vector<std::pair<int, int> >& dst) const;
         void setupSparseJtJ();
         void fillSparseJtJ(MatrixArray<double> const& Ui, MatrixArray<double> const& Vj, MatrixArray<double> const& Wk,
                            Matrix<double> const& Z, Matrix<double> const& X, Matrix<double> const& Y);

         int const _nMeasurements, _measurementDimension;
         int const _nParametersA, _paramDimensionA;
         int const _nParametersB, _paramDimensionB;
         int const _paramDimensionC;

         int _nNonvaryingA, _nNonvaryingB, _nNonvaryingC;

         std::vector<int> const& _correspondingParamA;
         std::vector<int> const& _correspondingParamB;

         std::vector<pair<int, int> > _jointNonzerosW;
         std::vector<int>             _jointIndexW;

         std::vector<int> _JtJ_Lp, _JtJ_Parent, _JtJ_Lnz;
         std::vector<int> _perm_JtJ, _invPerm_JtJ;

         CCS_Matrix<double> _JtJ;
   }; // end struct SparseLevenbergOptimizer

   struct StdSparseLevenbergOptimizer : public SparseLevenbergOptimizer
   {
         StdSparseLevenbergOptimizer(int measurementDimension,
                                     int nParametersA, int paramDimensionA,
                                     int nParametersB, int paramDimensionB,
                                     int paramDimensionC,
                                     std::vector<int> const& correspondingParamA,
                                     std::vector<int> const& correspondingParamB)
            : SparseLevenbergOptimizer(measurementDimension, nParametersA, paramDimensionA,
                                       nParametersB, paramDimensionB, paramDimensionC,
                                       correspondingParamA, correspondingParamB),
              curParametersA(nParametersA, paramDimensionA), savedParametersA(nParametersA, paramDimensionA),
              curParametersB(nParametersB, paramDimensionB), savedParametersB(nParametersB, paramDimensionB),
              curParametersC(paramDimensionC), savedParametersC(paramDimensionC)
         { }

         virtual double getParameterLength() const
         {
            double res = 0.0;
            for (int i = 0; i < _nParametersA; ++i) res += sqrNorm_L2(curParametersA[i]);
            for (int j = 0; j < _nParametersB; ++j) res += sqrNorm_L2(curParametersB[j]);
            res += sqrNorm_L2(curParametersC);
            return sqrt(res);
         }

         virtual void updateParametersA(VectorArray<double> const& deltaAi)
         {
            for (int i = 0; i < _nParametersA; ++i) addVectors(deltaAi[i], curParametersA[i], curParametersA[i]);
         }

         virtual void updateParametersB(VectorArray<double> const& deltaBj)
         {
            for (int j = 0; j < _nParametersB; ++j) addVectors(deltaBj[j], curParametersB[j], curParametersB[j]);
         }

         virtual void updateParametersC(Vector<double> const& deltaC)
         {
            addVectors(deltaC, curParametersC, curParametersC);
         }

         virtual void saveAllParameters()
         {
            for (int i = 0; i < _nParametersA; ++i) savedParametersA[i] = curParametersA[i];
            for (int j = 0; j < _nParametersB; ++j) savedParametersB[j] = curParametersB[j];
            savedParametersC = curParametersC;
         }

         virtual void restoreAllParameters()
         {
            for (int i = 0; i < _nParametersA; ++i) curParametersA[i] = savedParametersA[i];
            for (int j = 0; j < _nParametersB; ++j) curParametersB[j] = savedParametersB[j];
            curParametersC = savedParametersC;
         }

         VectorArray<double> curParametersA, savedParametersA;
         VectorArray<double> curParametersB, savedParametersB;
         Vector<double>      curParametersC, savedParametersC;
   }; // end struct StdSparseLevenbergOptimizer

# endif

} // end namespace V3D

#endif
