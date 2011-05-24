// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#include "common.h"
#include <Eigen/Eigenvalues>

// computes an LU factorization of a general M-by-N matrix A using partial pivoting with row interchanges
EIGEN_LAPACK_FUNC(syev,(char *jobz, char *uplo, int* n, Scalar* a, int *lda, Scalar* w, Scalar* /*work*/, int* lwork, int *info))
{
  // TODO exploit the work buffer
  bool query_size = *lwork==-1;
  
  *info = 0;
        if(*jobz!='N' && *jobz!='V')                    *info = -1;
  else  if(UPLO(*uplo)==INVALID)                        *info = -2;
  else  if(*n<0)                                        *info = -3;
  else  if(*lda<std::max(1,*n))                         *info = -5;
  else  if((!query_size) && *lwork<std::max(1,3**n-1))  *info = -8;
  
//   if(*info==0)
//   {
//     int nb = ILAENV( 1, 'SSYTRD', UPLO, N, -1, -1, -1 )
//          LWKOPT = MAX( 1, ( NB+2 )*N )
//          WORK( 1 ) = LWKOPT
// *
//          IF( LWORK.LT.MAX( 1, 3*N-1 ) .AND. .NOT.LQUERY )
//      $      INFO = -8
//       END IF
// *
//       IF( INFO.NE.0 ) THEN
//          CALL XERBLA( 'SSYEV ', -INFO )
//          RETURN
//       ELSE IF( LQUERY ) THEN
//          RETURN
//       END IF
  
  if(*info!=0)
  {
    int e = -*info;
    return xerbla_(SCALAR_SUFFIX_UP"SYEV ", &e, 6);
  }
  
  if(query_size)
  {
    *lwork = 0;
    return 0;
  }
  
  if(*n==0)
    return 0;
  
  PlainMatrixType mat(*n,*n);
  if(UPLO(*uplo)==UP) mat = matrix(a,*n,*n,*lda).adjoint();
  else                mat = matrix(a,*n,*n,*lda);
  
  bool computeVectors = *jobz=='V' || *jobz=='v';
  SelfAdjointEigenSolver<PlainMatrixType> eig(mat,computeVectors?ComputeEigenvectors:EigenvaluesOnly);
  
  if(eig.info()==NoConvergence)
  {
    vector(w,*n).setZero();
    if(computeVectors)
      matrix(a,*n,*n,*lda).setIdentity();
    //*info = 1;
    return 0;
  }
  
  vector(w,*n) = eig.eigenvalues();
  if(computeVectors)
    matrix(a,*n,*n,*lda) = eig.eigenvectors();
  
  return 0;
}
