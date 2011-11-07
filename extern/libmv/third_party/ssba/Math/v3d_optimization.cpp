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

#include "Math/v3d_optimization.h"

#if defined(V3DLIB_ENABLE_SUITESPARSE)
//# include "COLAMD/Include/colamd.h"
# include "colamd.h"
extern "C"
{
//# include "LDL/Include/ldl.h"
# include "ldl.h"
}
#endif

#include <iostream>
#include <map>

#define USE_BLOCK_REORDERING 1
#define USE_MULTIPLICATIVE_UPDATE 1

using namespace std;

namespace
{

   using namespace V3D;

   inline double
   squaredResidual(VectorArray<double> const& e)
   {
      int const N = e.count();
      int const M = e.size();

      double res = 0.0;

      for (int n = 0; n < N; ++n)
         for (int m = 0; m < M; ++m)
            res += e[n][m] * e[n][m];

      return res;
   } // end squaredResidual()

} // end namespace <>

namespace V3D
{

   int optimizerVerbosenessLevel = 0;

#if defined(V3DLIB_ENABLE_SUITESPARSE)

   void
   SparseLevenbergOptimizer::setupSparseJtJ()
   {
      int const nVaryingA = _nParametersA - _nNonvaryingA;
      int const nVaryingB = _nParametersB - _nNonvaryingB;
      int const nVaryingC = _paramDimensionC - _nNonvaryingC;

      int const bColumnStart = nVaryingA*_paramDimensionA;
      int const cColumnStart = bColumnStart + nVaryingB*_paramDimensionB;
      int const nColumns     = cColumnStart + nVaryingC;

      _jointNonzerosW.clear();
      _jointIndexW.resize(_nMeasurements);
#if 1
      {
         map<pair<int, int>, int> jointNonzeroMap;
         for (size_t k = 0; k < _nMeasurements; ++k)
         {
            int const i = _correspondingParamA[k] - _nNonvaryingA;
            int const j = _correspondingParamB[k] - _nNonvaryingB;
            if (i >= 0 && j >= 0)
            {
               map<pair<int, int>, int>::const_iterator p = jointNonzeroMap.find(make_pair(i, j));
               if (p == jointNonzeroMap.end())
               {
                  jointNonzeroMap.insert(make_pair(make_pair(i, j), _jointNonzerosW.size()));
                  _jointIndexW[k] = _jointNonzerosW.size();
                  _jointNonzerosW.push_back(make_pair(i, j));
               }
               else
               {
                  _jointIndexW[k] = (*p).second;
               } // end if
            } // end if
         } // end for (k)
      } // end scope
#else
      for (size_t k = 0; k < _nMeasurements; ++k)
      {
         int const i = _correspondingParamA[k] - _nNonvaryingA;
         int const j = _correspondingParamB[k] - _nNonvaryingB;
         if (i >= 0 && j >= 0)
         {
            _jointIndexW[k] = _jointNonzerosW.size();
            _jointNonzerosW.push_back(make_pair(i, j));
         }
      } // end for (k)
#endif

#if defined(USE_BLOCK_REORDERING)
      int const bBlockColumnStart = nVaryingA;
      int const cBlockColumnStart = bBlockColumnStart + nVaryingB;

      int const nBlockColumns = cBlockColumnStart + ((nVaryingC > 0) ? 1 : 0);

      //cout << "nBlockColumns = " << nBlockColumns << endl;

      // For the column reordering we treat the columns belonging to one set
      // of parameters as one (logical) column.

      // Determine non-zeros of JtJ (we forget about the non-zero diagonal for now)
      // Only consider nonzeros of Ai^t * Bj induced by the measurements.
      vector<pair<int, int> > nz_blockJtJ(_jointNonzerosW.size());
      for (int k = 0; k < _jointNonzerosW.size(); ++k)
      {
         nz_blockJtJ[k].first  = _jointNonzerosW[k].second + bBlockColumnStart;
         nz_blockJtJ[k].second = _jointNonzerosW[k].first;
      }

      if (nVaryingC > 0)
      {
         // We assume, that the global unknowns are linked to every other variable.
         for (int i = 0; i < nVaryingA; ++i)
            nz_blockJtJ.push_back(make_pair(cBlockColumnStart, i));
         for (int j = 0; j < nVaryingB; ++j)
            nz_blockJtJ.push_back(make_pair(cBlockColumnStart, j + bBlockColumnStart));
      } // end if

      int const nnzBlock = nz_blockJtJ.size();

      vector<int> permBlockJtJ(nBlockColumns + 1);

      if (nnzBlock > 0)
      {
//          cout << "nnzBlock = " << nnzBlock << endl;

         CCS_Matrix<int> blockJtJ(nBlockColumns, nBlockColumns, nz_blockJtJ);

//          cout << " nz_blockJtJ: " << endl;
//          for (size_t k = 0; k < nz_blockJtJ.size(); ++k)
//             cout << " " << nz_blockJtJ[k].first << ":" << nz_blockJtJ[k].second << endl;
//          cout << endl;

         int * colStarts = (int *)blockJtJ.getColumnStarts();
         int * rowIdxs   = (int *)blockJtJ.getRowIndices();

//          cout << "blockJtJ_colStarts = ";
//          for (int k = 0; k <= nBlockColumns; ++k) cout << colStarts[k] << " ";
//          cout << endl;

//          cout << "blockJtJ_rowIdxs = ";
//          for (int k = 0; k < nnzBlock; ++k) cout << rowIdxs[k] << " ";
//          cout << endl;

         int stats[COLAMD_STATS];
         symamd(nBlockColumns, rowIdxs, colStarts, &permBlockJtJ[0], (double *) NULL, stats, &calloc, &free);
         if (optimizerVerbosenessLevel >= 2) symamd_report(stats);
      }
      else
      {
         for (int k = 0; k < permBlockJtJ.size(); ++k) permBlockJtJ[k] = k;
      } // end if

//       cout << "permBlockJtJ = ";
//       for (int k = 0; k < permBlockJtJ.size(); ++k)
//          cout << permBlockJtJ[k] << " ";
//       cout << endl;

      // From the determined symbolic permutation with logical variables, determine the actual ordering
      _perm_JtJ.resize(nVaryingA*_paramDimensionA + nVaryingB*_paramDimensionB + nVaryingC + 1);

      int curDstCol = 0;
      for (int k = 0; k < permBlockJtJ.size()-1; ++k)
      {
         int const srcCol = permBlockJtJ[k];
         if (srcCol < nVaryingA)
         {
            for (int n = 0; n < _paramDimensionA; ++n)
               _perm_JtJ[curDstCol + n] = srcCol*_paramDimensionA + n;
            curDstCol += _paramDimensionA;
         }
         else if (srcCol >= bBlockColumnStart && srcCol < cBlockColumnStart)
         {
            int const bStart = nVaryingA*_paramDimensionA;
            int const j = srcCol - bBlockColumnStart;

            for (int n = 0; n < _paramDimensionB; ++n)
               _perm_JtJ[curDstCol + n] = bStart + j*_paramDimensionB + n;
            curDstCol += _paramDimensionB;
         }
         else if (srcCol == cBlockColumnStart)
         {
            int const cStart = nVaryingA*_paramDimensionA + nVaryingB*_paramDimensionB;

            for (int n = 0; n < nVaryingC; ++n)
               _perm_JtJ[curDstCol + n] = cStart + n;
            curDstCol += nVaryingC;
         }
         else
         {
            cerr << "Should not reach " << __LINE__ << ":" << __LINE__ << "!" << endl;
            assert(false);
         }
      }
#else
      vector<pair<int, int> > nz, nzL;
      this->serializeNonZerosJtJ(nz);

      for (int k = 0; k < nz.size(); ++k)
      {
         // Swap rows and columns, since serializeNonZerosJtJ() generates the
         // upper triangular part but symamd wants the lower triangle.
         nzL.push_back(make_pair(nz[k].second, nz[k].first));
      }

      _perm_JtJ.resize(nColumns+1);

      if (nzL.size() > 0)
      {
         CCS_Matrix<int> symbJtJ(nColumns, nColumns, nzL);

         int * colStarts = (int *)symbJtJ.getColumnStarts();
         int * rowIdxs   = (int *)symbJtJ.getRowIndices();

//       cout << "symbJtJ_colStarts = ";
//       for (int k = 0; k <= nColumns; ++k) cout << colStarts[k] << " ";
//       cout << endl;

//       cout << "symbJtJ_rowIdxs = ";
//       for (int k = 0; k < nzL.size(); ++k) cout << rowIdxs[k] << " ";
//       cout << endl;

         int stats[COLAMD_STATS];
         symamd(nColumns, rowIdxs, colStarts, &_perm_JtJ[0], (double *) NULL, stats, &calloc, &free);
         if (optimizerVerbosenessLevel >= 2) symamd_report(stats);
      }
      else
      {
         for (int k = 0; k < _perm_JtJ.size(); ++k) _perm_JtJ[k] = k;
      } //// end if
#endif
      _perm_JtJ.back() = _perm_JtJ.size() - 1;

//       cout << "_perm_JtJ = ";
//       for (int k = 0; k < _perm_JtJ.size(); ++k) cout << _perm_JtJ[k] << " ";
//       cout << endl;

      // Finally, compute the inverse of the full permutation.
      _invPerm_JtJ.resize(_perm_JtJ.size());
      for (size_t k = 0; k < _perm_JtJ.size(); ++k)
         _invPerm_JtJ[_perm_JtJ[k]] = k;

      vector<pair<int, int> > nz_JtJ;
      this->serializeNonZerosJtJ(nz_JtJ);

      for (int k = 0; k < nz_JtJ.size(); ++k)
      {
         int const i = nz_JtJ[k].first;
         int const j = nz_JtJ[k].second;

         int pi = _invPerm_JtJ[i];
         int pj = _invPerm_JtJ[j];
         // Swap values if in lower triangular part
         if (pi > pj) std::swap(pi, pj);
         nz_JtJ[k].first = pi;
         nz_JtJ[k].second = pj;
      }

      int const nnz   = nz_JtJ.size();

//       cout << "nz_JtJ = ";
//       for (int k = 0; k < nnz; ++k) cout << nz_JtJ[k].first << ":" << nz_JtJ[k].second << " ";
//       cout << endl;

      _JtJ.create(nColumns, nColumns, nz_JtJ);

//       cout << "_colStart_JtJ = ";
//       for (int k = 0; k < _JtJ.num_cols(); ++k) cout << _JtJ.getColumnStarts()[k] << " ";
//       cout << endl;

//       cout << "_rowIdxs_JtJ = ";
//       for (int k = 0; k < nnz; ++k) cout << _JtJ.getRowIndices()[k] << " ";
//       cout << endl;

      vector<int> workFlags(nColumns);

      _JtJ_Lp.resize(nColumns+1);
      _JtJ_Parent.resize(nColumns);
      _JtJ_Lnz.resize(nColumns);

      ldl_symbolic(nColumns, (int *)_JtJ.getColumnStarts(), (int *)_JtJ.getRowIndices(),
                   &_JtJ_Lp[0], &_JtJ_Parent[0], &_JtJ_Lnz[0],
                   &workFlags[0], NULL, NULL);

      if (optimizerVerbosenessLevel >= 1)
         cout << "SparseLevenbergOptimizer: Nonzeros in LDL decomposition: " << _JtJ_Lp[nColumns] << endl;

   } // end SparseLevenbergOptimizer::setupSparseJtJ()

   void
   SparseLevenbergOptimizer::serializeNonZerosJtJ(vector<pair<int, int> >& dst) const
   {
      int const nVaryingA = _nParametersA - _nNonvaryingA;
      int const nVaryingB = _nParametersB - _nNonvaryingB;
      int const nVaryingC = _paramDimensionC - _nNonvaryingC;

      int const bColumnStart = nVaryingA*_paramDimensionA;
      int const cColumnStart = bColumnStart + nVaryingB*_paramDimensionB;

      dst.clear();

      // Add the diagonal block matrices (only the upper triangular part).

      // Ui submatrices of JtJ
      for (int i = 0; i < nVaryingA; ++i)
      {
         int const i0 = i * _paramDimensionA;

         for (int c = 0; c < _paramDimensionA; ++c)
            for (int r = 0; r <= c; ++r)
               dst.push_back(make_pair(i0 + r, i0 + c));
      }

      // Vj submatrices of JtJ
      for (int j = 0; j < nVaryingB; ++j)
      {
         int const j0 = j*_paramDimensionB + bColumnStart;

         for (int c = 0; c < _paramDimensionB; ++c)
            for (int r = 0; r <= c; ++r)
               dst.push_back(make_pair(j0 + r, j0 + c));
      }

      // Z submatrix of JtJ
      for (int c = 0; c < nVaryingC; ++c)
         for (int r = 0; r <= c; ++r)
            dst.push_back(make_pair(cColumnStart + r, cColumnStart + c));

      // Add the elements i and j linked by an observation k
      // W submatrix of JtJ
      for (size_t n = 0; n < _jointNonzerosW.size(); ++n)
      {
         int const i0 = _jointNonzerosW[n].first;
         int const j0 = _jointNonzerosW[n].second;
         int const r0 = i0*_paramDimensionA;
         int const c0 = j0*_paramDimensionB + bColumnStart;

         for (int r = 0; r < _paramDimensionA; ++r)
            for (int c = 0; c < _paramDimensionB; ++c)
               dst.push_back(make_pair(r0 + r, c0 + c));
      } // end for (n)

      if (nVaryingC > 0)
      {
         // Finally, add the dense columns linking i (resp. j) with the global parameters.
         // X submatrix of JtJ
         for (int i = 0; i < nVaryingA; ++i)
         {
            int const i0 = i*_paramDimensionA;

            for (int r = 0; r < _paramDimensionA; ++r)
               for (int c = 0; c < nVaryingC; ++c)
                  dst.push_back(make_pair(i0 + r, cColumnStart + c));
         }

         // Y submatrix of JtJ
         for (int j = 0; j < nVaryingB; ++j)
         {
            int const j0 = j*_paramDimensionB + bColumnStart;

            for (int r = 0; r < _paramDimensionB; ++r)
               for (int c = 0; c < nVaryingC; ++c)
                  dst.push_back(make_pair(j0 + r, cColumnStart + c));
         }
      } // end if
   } // end SparseLevenbergOptimizer::serializeNonZerosJtJ()

   void
   SparseLevenbergOptimizer::fillSparseJtJ(MatrixArray<double> const& Ui,
                                           MatrixArray<double> const& Vj,
                                           MatrixArray<double> const& Wn,
                                           Matrix<double> const& Z,
                                           Matrix<double> const& X,
                                           Matrix<double> const& Y)
   {
      int const nVaryingA = _nParametersA - _nNonvaryingA;
      int const nVaryingB = _nParametersB - _nNonvaryingB;
      int const nVaryingC = _paramDimensionC - _nNonvaryingC;

      int const bColumnStart = nVaryingA*_paramDimensionA;
      int const cColumnStart = bColumnStart + nVaryingB*_paramDimensionB;

      int const nCols = _JtJ.num_cols();
      int const nnz   = _JtJ.getNonzeroCount();

      // The following has to replicate the procedure as in serializeNonZerosJtJ()

      int serial = 0;

      double * values = _JtJ.getValues();
      int const * destIdxs = _JtJ.getDestIndices();

      // Add the diagonal block matrices (only the upper triangular part).

      // Ui submatrices of JtJ
      for (int i = 0; i < nVaryingA; ++i)
      {
         int const i0 = i * _paramDimensionA;

         for (int c = 0; c < _paramDimensionA; ++c)
            for (int r = 0; r <= c; ++r, ++serial)
               values[destIdxs[serial]] = Ui[i][r][c];
      }

      // Vj submatrices of JtJ
      for (int j = 0; j < nVaryingB; ++j)
      {
         int const j0 = j*_paramDimensionB + bColumnStart;

         for (int c = 0; c < _paramDimensionB; ++c)
            for (int r = 0; r <= c; ++r, ++serial)
               values[destIdxs[serial]] = Vj[j][r][c];
      }

      // Z submatrix of JtJ
      for (int c = 0; c < nVaryingC; ++c)
         for (int r = 0; r <= c; ++r, ++serial)
            values[destIdxs[serial]] = Z[r][c];

      // Add the elements i and j linked by an observation k
      // W submatrix of JtJ
      for (size_t n = 0; n < _jointNonzerosW.size(); ++n)
      {
         for (int r = 0; r < _paramDimensionA; ++r)
            for (int c = 0; c < _paramDimensionB; ++c, ++serial)
               values[destIdxs[serial]] = Wn[n][r][c];
      } // end for (k)

      if (nVaryingC > 0)
      {
         // Finally, add the dense columns linking i (resp. j) with the global parameters.
         // X submatrix of JtJ
         for (int i = 0; i < nVaryingA; ++i)
         {
            int const r0 = i * _paramDimensionA;
            for (int r = 0; r < _paramDimensionA; ++r)
               for (int c = 0; c < nVaryingC; ++c, ++serial)
                  values[destIdxs[serial]] = X[r0+r][c];
         }

         // Y submatrix of JtJ
         for (int j = 0; j < nVaryingB; ++j)
         {
            int const r0 = j * _paramDimensionB;
            for (int r = 0; r < _paramDimensionB; ++r)
               for (int c = 0; c < nVaryingC; ++c, ++serial)
                  values[destIdxs[serial]] = Y[r0+r][c];
         }
      } // end if
   } // end SparseLevenbergOptimizer::fillSparseJtJ()

   void
   SparseLevenbergOptimizer::minimize()
   {
      status = LEVENBERG_OPTIMIZER_TIMEOUT;
      bool computeDerivatives = true;

      int const nVaryingA = _nParametersA - _nNonvaryingA;
      int const nVaryingB = _nParametersB - _nNonvaryingB;
      int const nVaryingC = _paramDimensionC - _nNonvaryingC;

      if (nVaryingA == 0 && nVaryingB == 0 && nVaryingC == 0)
      {
         // No degrees of freedom, nothing to optimize.
         status = LEVENBERG_OPTIMIZER_CONVERGED;
         return;
      }

      this->setupSparseJtJ();

      Vector<double> weights(_nMeasurements);

      MatrixArray<double> Ak(_nMeasurements, _measurementDimension, _paramDimensionA);
      MatrixArray<double> Bk(_nMeasurements, _measurementDimension, _paramDimensionB);
      MatrixArray<double> Ck(_nMeasurements, _measurementDimension, _paramDimensionC);

      MatrixArray<double> Ui(nVaryingA, _paramDimensionA, _paramDimensionA);
      MatrixArray<double> Vj(nVaryingB, _paramDimensionB, _paramDimensionB);

      // Wn = Ak^t*Bk
      MatrixArray<double> Wn(_jointNonzerosW.size(), _paramDimensionA, _paramDimensionB);

      Matrix<double> Z(nVaryingC, nVaryingC);

      // X = A^t*C
      Matrix<double> X(nVaryingA*_paramDimensionA, nVaryingC);
      // Y = B^t*C
      Matrix<double> Y(nVaryingB*_paramDimensionB, nVaryingC);

      VectorArray<double> residuals(_nMeasurements, _measurementDimension);
      VectorArray<double> residuals2(_nMeasurements, _measurementDimension);

      VectorArray<double> diagUi(nVaryingA, _paramDimensionA);
      VectorArray<double> diagVj(nVaryingB, _paramDimensionB);
      Vector<double> diagZ(nVaryingC);

      VectorArray<double> At_e(nVaryingA, _paramDimensionA);
      VectorArray<double> Bt_e(nVaryingB, _paramDimensionB);
      Vector<double> Ct_e(nVaryingC);

      Vector<double> Jt_e(nVaryingA*_paramDimensionA + nVaryingB*_paramDimensionB + nVaryingC);

      Vector<double> delta(nVaryingA*_paramDimensionA + nVaryingB*_paramDimensionB + nVaryingC);
      Vector<double> deltaPerm(nVaryingA*_paramDimensionA + nVaryingB*_paramDimensionB + nVaryingC);

      VectorArray<double> deltaAi(_nParametersA, _paramDimensionA);
      VectorArray<double> deltaBj(_nParametersB, _paramDimensionB);
      Vector<double> deltaC(_paramDimensionC);

      double err = 0.0;

      for (currentIteration = 0; currentIteration < maxIterations; ++currentIteration)
      {
         if (optimizerVerbosenessLevel >= 2)
            cout << "SparseLevenbergOptimizer: currentIteration: " << currentIteration << endl;
         if (computeDerivatives)
         {
            this->evalResidual(residuals);
            this->fillWeights(residuals, weights);
            for (int k = 0; k < _nMeasurements; ++k)
               scaleVectorIP(weights[k], residuals[k]);

            err = squaredResidual(residuals);

            if (optimizerVerbosenessLevel >= 1) cout << "SparseLevenbergOptimizer: |residual|^2 = " << err << endl;
            if (optimizerVerbosenessLevel >= 2) cout << "SparseLevenbergOptimizer: lambda = " << lambda << endl;

            for (int k = 0; k < residuals.count(); ++k) scaleVectorIP(-1.0, residuals[k]);

            this->setupJacobianGathering();
            this->fillAllJacobians(weights, Ak, Bk, Ck);

            // Compute the different parts of J^t*e
            if (nVaryingA > 0)
            {
               for (int i = 0; i < nVaryingA; ++i) makeZeroVector(At_e[i]);

               Vector<double> tmp(_paramDimensionA);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const i = _correspondingParamA[k] - _nNonvaryingA;
                  if (i < 0) continue;
                  multiply_At_v(Ak[k], residuals[k], tmp);
                  addVectors(tmp, At_e[i], At_e[i]);
               } // end for (k)
            } // end if

            if (nVaryingB > 0)
            {
               for (int j = 0; j < nVaryingB; ++j) makeZeroVector(Bt_e[j]);

               Vector<double> tmp(_paramDimensionB);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const j = _correspondingParamB[k] - _nNonvaryingB;
                  if (j < 0) continue;
                  multiply_At_v(Bk[k], residuals[k], tmp);
                  addVectors(tmp, Bt_e[j], Bt_e[j]);
               } // end for (k)
            } // end if

            if (nVaryingC > 0)
            {
               makeZeroVector(Ct_e);

               Vector<double> tmp(_paramDimensionC);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  multiply_At_v(Ck[k], residuals[k], tmp);
                  for (int l = 0; l < nVaryingC; ++l) Ct_e[l] += tmp[_nNonvaryingC + l];
               }
            } // end if

            int pos = 0;
            for (int i = 0; i < nVaryingA; ++i)
               for (int l = 0; l < _paramDimensionA; ++l, ++pos)
                  Jt_e[pos] = At_e[i][l];
            for (int j = 0; j < nVaryingB; ++j)
               for (int l = 0; l < _paramDimensionB; ++l, ++pos)
                  Jt_e[pos] = Bt_e[j][l];
            for (int l = 0; l < nVaryingC; ++l, ++pos)
               Jt_e[pos] = Ct_e[l];

//                cout << "Jt_e = ";
//                for (int k = 0; k < Jt_e.size(); ++k) cout << Jt_e[k] << " ";
//                cout << endl;

            if (this->applyGradientStoppingCriteria(norm_Linf(Jt_e)))
            {
               status = LEVENBERG_OPTIMIZER_CONVERGED;
               goto end;
            }

            // The lhs J^t*J consists of several parts:
            //         [ U     W   X ]
            // J^t*J = [ W^t   V   Y ]
            //         [ X^t  Y^t  Z ],
            // where U, V and W are block-sparse matrices (due to the sparsity of A and B).
            // X, Y and Z contain only a few columns (the number of global parameters).

            if (nVaryingA > 0)
            {
               // Compute Ui
               Matrix<double> U(_paramDimensionA, _paramDimensionA);

               for (int i = 0; i < nVaryingA; ++i) makeZeroMatrix(Ui[i]);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const i = _correspondingParamA[k] - _nNonvaryingA;
                  if (i < 0) continue;
                  multiply_At_A(Ak[k], U);
                  addMatricesIP(U, Ui[i]);
               } // end for (k)
            } // end if

            if (nVaryingB > 0)
            {
               // Compute Vj
               Matrix<double> V(_paramDimensionB, _paramDimensionB);

               for (int j = 0; j < nVaryingB; ++j) makeZeroMatrix(Vj[j]);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const j = _correspondingParamB[k] - _nNonvaryingB;
                  if (j < 0) continue;
                  multiply_At_A(Bk[k], V);
                  addMatricesIP(V, Vj[j]);
               } // end for (k)
            } // end if

            if (nVaryingC > 0)
            {
               Matrix<double> ZZ(_paramDimensionC, _paramDimensionC);
               Matrix<double> Zsum(_paramDimensionC, _paramDimensionC);

               makeZeroMatrix(Zsum);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  multiply_At_A(Ck[k], ZZ);
                  addMatricesIP(ZZ, Zsum);
               } // end for (k)

               // Ignore the non-varying parameters
               for (int i = 0; i < nVaryingC; ++i)
                  for (int j = 0; j < nVaryingC; ++j)
                     Z[i][j] = Zsum[i+_nNonvaryingC][j+_nNonvaryingC];
            } // end if

            if (nVaryingA > 0 && nVaryingB > 0)
            {
               for (int n = 0; n < Wn.count(); ++n) makeZeroMatrix(Wn[n]);

               Matrix<double> W(_paramDimensionA, _paramDimensionB);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const n = _jointIndexW[k];
                  if (n >= 0)
                  {
                     int const i0 = _jointNonzerosW[n].first;
                     int const j0 = _jointNonzerosW[n].second;

                     multiply_At_B(Ak[k], Bk[k], W);
                     addMatricesIP(W, Wn[n]);
                  } // end if
               } // end for (k)
            } // end if

            if (nVaryingA > 0 && nVaryingC > 0)
            {
               Matrix<double> XX(_paramDimensionA, _paramDimensionC);

               makeZeroMatrix(X);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const i = _correspondingParamA[k] - _nNonvaryingA;
                  // Ignore the non-varying parameters
                  if (i < 0) continue;

                  multiply_At_B(Ak[k], Ck[k], XX);

                  for (int r = 0; r < _paramDimensionA; ++r)
                     for (int c = 0; c < nVaryingC; ++c)
                        X[r+i*_paramDimensionA][c] += XX[r][c+_nNonvaryingC];
               } // end for (k)
            } // end if

            if (nVaryingB > 0 && nVaryingC > 0)
            {
               Matrix<double> YY(_paramDimensionB, _paramDimensionC);

               makeZeroMatrix(Y);

               for (int k = 0; k < _nMeasurements; ++k)
               {
                  int const j = _correspondingParamB[k] - _nNonvaryingB;
                  // Ignore the non-varying parameters
                  if (j < 0) continue;

                  multiply_At_B(Bk[k], Ck[k], YY);

                  for (int r = 0; r < _paramDimensionB; ++r)
                     for (int c = 0; c < nVaryingC; ++c)
                        Y[r+j*_paramDimensionB][c] += YY[r][c+_nNonvaryingC];
               } // end for (k)
            } // end if

            if (currentIteration == 0)
            {
               // Initialize lambda as tau*max(JtJ[i][i])
               double maxEl = -1e30;
               if (nVaryingA > 0)
               {
                  for (int i = 0; i < nVaryingA; ++i)
                     for (int l = 0; l < _paramDimensionA; ++l)
                        maxEl = std::max(maxEl, Ui[i][l][l]);
               }
               if (nVaryingB > 0)
               {
                  for (int j = 0; j < nVaryingB; ++j)
                     for (int l = 0; l < _paramDimensionB; ++l)
                        maxEl = std::max(maxEl, Vj[j][l][l]);
               }
               if (nVaryingC > 0)
               {
                  for (int l = 0; l < nVaryingC; ++l)
                     maxEl = std::max(maxEl, Z[l][l]);
               }

               lambda = tau * maxEl;
               if (optimizerVerbosenessLevel >= 2)
                  cout << "SparseLevenbergOptimizer: initial lambda = " << lambda << endl;
            } // end if (currentIteration == 0)
         } // end if (computeDerivatives)

         for (int i = 0; i < nVaryingA; ++i)
         {
            for (int l = 0; l < _paramDimensionA; ++l) diagUi[i][l] = Ui[i][l][l];
         } // end for (i)

         for (int j = 0; j < nVaryingB; ++j)
         {
            for (int l = 0; l < _paramDimensionB; ++l) diagVj[j][l] = Vj[j][l][l];
         } // end for (j)

         for (int l = 0; l < nVaryingC; ++l) diagZ[l] = Z[l][l];

         // Augment the diagonals with lambda (either by the standard additive update or by multiplication).
#if !defined(USE_MULTIPLICATIVE_UPDATE)
         for (int i = 0; i < nVaryingA; ++i)
            for (unsigned l = 0; l < _paramDimensionA; ++l)
               Ui[i][l][l] += lambda;

         for (int j = 0; j < nVaryingB; ++j)
            for (unsigned l = 0; l < _paramDimensionB; ++l)
               Vj[j][l][l] += lambda;

         for (unsigned l = 0; l < nVaryingC; ++l)
            Z[l][l] += lambda;
#else
         for (int i = 0; i < nVaryingA; ++i)
            for (unsigned l = 0; l < _paramDimensionA; ++l)
               Ui[i][l][l] = std::max(Ui[i][l][l] * (1.0 + lambda), 1e-15);

         for (int j = 0; j < nVaryingB; ++j)
            for (unsigned l = 0; l < _paramDimensionB; ++l)
               Vj[j][l][l] = std::max(Vj[j][l][l] * (1.0 + lambda), 1e-15);

         for (unsigned l = 0; l < nVaryingC; ++l)
            Z[l][l] = std::max(Z[l][l] * (1.0 + lambda), 1e-15);
#endif

         this->fillSparseJtJ(Ui, Vj, Wn, Z, X, Y);

         bool success = true;
         double rho = 0.0;
         {
            int const nCols = _JtJ_Parent.size();
            int const nnz   = _JtJ.getNonzeroCount();
            int const lnz   = _JtJ_Lp.back();

            vector<int> Li(lnz);
            vector<double> Lx(lnz);
            vector<double> D(nCols), Y(nCols);
            vector<int> workPattern(nCols), workFlag(nCols);

            int * colStarts = (int *)_JtJ.getColumnStarts();
            int * rowIdxs   = (int *)_JtJ.getRowIndices();
            double * values = _JtJ.getValues();

            int const d = ldl_numeric(nCols, colStarts, rowIdxs, values,
                                      &_JtJ_Lp[0], &_JtJ_Parent[0], &_JtJ_Lnz[0],
                                      &Li[0], &Lx[0], &D[0],
                                      &Y[0], &workPattern[0], &workFlag[0],
                                      NULL, NULL);

            if (d == nCols)
            {
               ldl_perm(nCols, &deltaPerm[0], &Jt_e[0], &_perm_JtJ[0]);
               ldl_lsolve(nCols, &deltaPerm[0], &_JtJ_Lp[0], &Li[0], &Lx[0]);
               ldl_dsolve(nCols, &deltaPerm[0], &D[0]);
               ldl_ltsolve(nCols, &deltaPerm[0], &_JtJ_Lp[0], &Li[0], &Lx[0]);
               ldl_permt(nCols, &delta[0], &deltaPerm[0], &_perm_JtJ[0]);
            }
            else
            {
               if (optimizerVerbosenessLevel >= 2)
                  cout << "SparseLevenbergOptimizer: LDL decomposition failed. Increasing lambda." << endl;
               success = false;
            }
         }

         if (success)
         {
            double const deltaSqrLength = sqrNorm_L2(delta);

            if (optimizerVerbosenessLevel >= 2)
               cout << "SparseLevenbergOptimizer: ||delta||^2 = " << deltaSqrLength << endl;

            double const paramLength = this->getParameterLength();
            if (this->applyUpdateStoppingCriteria(paramLength, sqrt(deltaSqrLength)))
            {
               status = LEVENBERG_OPTIMIZER_SMALL_UPDATE;
               goto end;
            }

            // Copy the updates from delta to the respective arrays
            int pos = 0;

            for (int i = 0; i < _nNonvaryingA; ++i) makeZeroVector(deltaAi[i]);
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
               for (int l = 0; l < _paramDimensionA; ++l, ++pos)
                  deltaAi[i][l] = delta[pos];

            for (int j = 0; j < _nNonvaryingB; ++j) makeZeroVector(deltaBj[j]);
            for (int j = _nNonvaryingB; j < _nParametersB; ++j)
               for (int l = 0; l < _paramDimensionB; ++l, ++pos)
                  deltaBj[j][l] = delta[pos];

            makeZeroVector(deltaC);
            for (int l = _nNonvaryingC; l < _paramDimensionC; ++l, ++pos)
               deltaC[l] = delta[pos];

            saveAllParameters();
            if (nVaryingA > 0) updateParametersA(deltaAi);
            if (nVaryingB > 0) updateParametersB(deltaBj);
            if (nVaryingC > 0) updateParametersC(deltaC);

            this->evalResidual(residuals2);
            for (int k = 0; k < _nMeasurements; ++k)
               scaleVectorIP(weights[k], residuals2[k]);

            double const newErr = squaredResidual(residuals2);
            rho = err - newErr;
            if (optimizerVerbosenessLevel >= 2)
               cout << "SparseLevenbergOptimizer: |new residual|^2 = " << newErr << endl;

#if !defined(USE_MULTIPLICATIVE_UPDATE)
            double const denom1 = lambda * deltaSqrLength;
#else
            double denom1 = 0.0f;
            for (int i = _nNonvaryingA; i < _nParametersA; ++i)
               for (int l = 0; l < _paramDimensionA; ++l)
                  denom1 += deltaAi[i][l] * deltaAi[i][l] * diagUi[i-_nNonvaryingA][l];

            for (int j = _nNonvaryingB; j < _nParametersB; ++j)
               for (int l = 0; l < _paramDimensionB; ++l)
                  denom1 += deltaBj[j][l] * deltaBj[j][l] * diagVj[j-_nNonvaryingB][l];

            for (int l = _nNonvaryingC; l < _paramDimensionC; ++l)
               denom1 += deltaC[l] * deltaC[l] * diagZ[l-_nNonvaryingC];

            denom1 *= lambda;
#endif
            double const denom2 = innerProduct(delta, Jt_e);
            rho = rho / (denom1 + denom2);
            if (optimizerVerbosenessLevel >= 2)
               cout << "SparseLevenbergOptimizer: rho = " << rho
                    << " denom1 = " << denom1 << " denom2 = " << denom2 << endl;
         } // end if (success)

         if (success && rho > 0)
         {
            if (optimizerVerbosenessLevel >= 2)
               cout << "SparseLevenbergOptimizer: Improved solution - decreasing lambda." << endl;
            // Improvement in the new solution
            decreaseLambda(rho);
            computeDerivatives = true;
         }
         else
         {
            if (optimizerVerbosenessLevel >= 2)
               cout << "SparseLevenbergOptimizer: Inferior solution - increasing lambda." << endl;
            restoreAllParameters();
            increaseLambda();
            computeDerivatives = false;

            // Restore diagonal elements in Ui, Vj and Z.
            for (int i = 0; i < nVaryingA; ++i)
            {
               for (int l = 0; l < _paramDimensionA; ++l) Ui[i][l][l] = diagUi[i][l];
            } // end for (i)

            for (int j = 0; j < nVaryingB; ++j)
            {
               for (int l = 0; l < _paramDimensionB; ++l) Vj[j][l][l] = diagVj[j][l];
            } // end for (j)

            for (int l = 0; l < nVaryingC; ++l) Z[l][l] = diagZ[l];
         } // end if
      } // end for

     end:;
      if (optimizerVerbosenessLevel >= 2)
         cout << "Leaving SparseLevenbergOptimizer::minimize()." << endl;
   } // end SparseLevenbergOptimizer::minimize()

#endif // defined(V3DLIB_ENABLE_SUITESPARSE)

} // end namespace V3D
