// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

/**  ZHEMV  performs the matrix-vector  operation
  *
  *     y := alpha*A*x + beta*y,
  *
  *  where alpha and beta are scalars, x and y are n element vectors and
  *  A is an n by n hermitian matrix.
  */
int EIGEN_BLAS_FUNC(hemv)(char *uplo, int *n, RealScalar *palpha, RealScalar *pa, int *lda, RealScalar *px, int *incx, RealScalar *pbeta, RealScalar *py, int *incy)
{
  Scalar* a = reinterpret_cast<Scalar*>(pa);
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar alpha  = *reinterpret_cast<Scalar*>(palpha);
  Scalar beta   = *reinterpret_cast<Scalar*>(pbeta);

  // check arguments
  int info = 0;
  if(UPLO(*uplo)==INVALID)        info = 1;
  else if(*n<0)                   info = 2;
  else if(*lda<std::max(1,*n))    info = 5;
  else if(*incx==0)               info = 7;
  else if(*incy==0)               info = 10;
  if(info)
    return xerbla_(SCALAR_SUFFIX_UP"HEMV ",&info,6);

  if(*n==0)
    return 1;

  Scalar* actual_x = get_compact_vector(x,*n,*incx);
  Scalar* actual_y = get_compact_vector(y,*n,*incy);

  if(beta!=Scalar(1))
  {
    if(beta==Scalar(0)) vector(actual_y, *n).setZero();
    else                vector(actual_y, *n) *= beta;
  }

  if(alpha!=Scalar(0))
  {
    // TODO performs a direct call to the underlying implementation function
         if(UPLO(*uplo)==UP) vector(actual_y,*n).noalias() += matrix(a,*n,*n,*lda).selfadjointView<Upper>() * (alpha * vector(actual_x,*n));
    else if(UPLO(*uplo)==LO) vector(actual_y,*n).noalias() += matrix(a,*n,*n,*lda).selfadjointView<Lower>() * (alpha * vector(actual_x,*n));
  }

  if(actual_x!=x) delete[] actual_x;
  if(actual_y!=y) delete[] copy_back(actual_y,y,*n,*incy);

  return 1;
}

/**  ZHBMV  performs the matrix-vector  operation
  *
  *     y := alpha*A*x + beta*y,
  *
  *  where alpha and beta are scalars, x and y are n element vectors and
  *  A is an n by n hermitian band matrix, with k super-diagonals.
  */
// int EIGEN_BLAS_FUNC(hbmv)(char *uplo, int *n, int *k, RealScalar *alpha, RealScalar *a, int *lda,
//                           RealScalar *x, int *incx, RealScalar *beta, RealScalar *y, int *incy)
// {
//   return 1;
// }

/**  ZHPMV  performs the matrix-vector operation
  *
  *     y := alpha*A*x + beta*y,
  *
  *  where alpha and beta are scalars, x and y are n element vectors and
  *  A is an n by n hermitian matrix, supplied in packed form.
  */
// int EIGEN_BLAS_FUNC(hpmv)(char *uplo, int *n, RealScalar *alpha, RealScalar *ap, RealScalar *x, int *incx, RealScalar *beta, RealScalar *y, int *incy)
// {
//   return 1;
// }

/**  ZHPR    performs the hermitian rank 1 operation
  *
  *     A := alpha*x*conjg( x' ) + A,
  *
  *  where alpha is a real scalar, x is an n element vector and A is an
  *  n by n hermitian matrix, supplied in packed form.
  */
// int EIGEN_BLAS_FUNC(hpr)(char *uplo, int *n, RealScalar *alpha, RealScalar *x, int *incx, RealScalar *ap)
// {
//   return 1;
// }

/**  ZHPR2  performs the hermitian rank 2 operation
  *
  *     A := alpha*x*conjg( y' ) + conjg( alpha )*y*conjg( x' ) + A,
  *
  *  where alpha is a scalar, x and y are n element vectors and A is an
  *  n by n hermitian matrix, supplied in packed form.
  */
// int EIGEN_BLAS_FUNC(hpr2)(char *uplo, int *n, RealScalar *palpha, RealScalar *x, int *incx, RealScalar *y, int *incy, RealScalar *ap)
// {
//   return 1;
// }

/**  ZHER   performs the hermitian rank 1 operation
  *
  *     A := alpha*x*conjg( x' ) + A,
  *
  *  where alpha is a real scalar, x is an n element vector and A is an
  *  n by n hermitian matrix.
  */
int EIGEN_BLAS_FUNC(her)(char *uplo, int *n, RealScalar *palpha, RealScalar *px, int *incx, RealScalar *pa, int *lda)
{
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* a = reinterpret_cast<Scalar*>(pa);
  RealScalar alpha = *reinterpret_cast<RealScalar*>(palpha);

  int info = 0;
  if(UPLO(*uplo)==INVALID)                                            info = 1;
  else if(*n<0)                                                       info = 2;
  else if(*incx==0)                                                   info = 5;
  else if(*lda<std::max(1,*n))                                        info = 7;
  if(info)
    return xerbla_(SCALAR_SUFFIX_UP"HER  ",&info,6);

  if(alpha==RealScalar(0))
    return 1;

  Scalar* x_cpy = get_compact_vector(x, *n, *incx);

  // TODO perform direct calls to underlying implementation
//   if(UPLO(*uplo)==LO)       matrix(a,*n,*n,*lda).selfadjointView<Lower>().rankUpdate(vector(x_cpy,*n), alpha);
//   else if(UPLO(*uplo)==UP)  matrix(a,*n,*n,*lda).selfadjointView<Upper>().rankUpdate(vector(x_cpy,*n), alpha);

  if(UPLO(*uplo)==LO)
    for(int j=0;j<*n;++j)
      matrix(a,*n,*n,*lda).col(j).tail(*n-j) += alpha * internal::conj(x_cpy[j]) * vector(x_cpy+j,*n-j);
  else
    for(int j=0;j<*n;++j)
      matrix(a,*n,*n,*lda).col(j).head(j+1) += alpha * internal::conj(x_cpy[j]) * vector(x_cpy,j+1);

  matrix(a,*n,*n,*lda).diagonal().imag().setZero();

  if(x_cpy!=x)  delete[] x_cpy;

  return 1;
}

/**  ZHER2  performs the hermitian rank 2 operation
  *
  *     A := alpha*x*conjg( y' ) + conjg( alpha )*y*conjg( x' ) + A,
  *
  *  where alpha is a scalar, x and y are n element vectors and A is an n
  *  by n hermitian matrix.
  */
int EIGEN_BLAS_FUNC(her2)(char *uplo, int *n, RealScalar *palpha, RealScalar *px, int *incx, RealScalar *py, int *incy, RealScalar *pa, int *lda)
{
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar* a = reinterpret_cast<Scalar*>(pa);
  Scalar alpha = *reinterpret_cast<Scalar*>(palpha);

  int info = 0;
  if(UPLO(*uplo)==INVALID)                                            info = 1;
  else if(*n<0)                                                       info = 2;
  else if(*incx==0)                                                   info = 5;
  else if(*incy==0)                                                   info = 7;
  else if(*lda<std::max(1,*n))                                        info = 9;
  if(info)
    return xerbla_(SCALAR_SUFFIX_UP"HER2 ",&info,6);

  if(alpha==Scalar(0))
    return 1;

  Scalar* x_cpy = get_compact_vector(x, *n, *incx);
  Scalar* y_cpy = get_compact_vector(y, *n, *incy);

  // TODO perform direct calls to underlying implementation
  if(UPLO(*uplo)==LO)       matrix(a,*n,*n,*lda).selfadjointView<Lower>().rankUpdate(vector(x_cpy,*n),vector(y_cpy,*n),alpha);
  else if(UPLO(*uplo)==UP)  matrix(a,*n,*n,*lda).selfadjointView<Upper>().rankUpdate(vector(x_cpy,*n),vector(y_cpy,*n),alpha);

  matrix(a,*n,*n,*lda).diagonal().imag().setZero();

  if(x_cpy!=x)  delete[] x_cpy;
  if(y_cpy!=y)  delete[] y_cpy;

  return 1;
}

/**  ZGERU  performs the rank 1 operation
  *
  *     A := alpha*x*y' + A,
  *
  *  where alpha is a scalar, x is an m element vector, y is an n element
  *  vector and A is an m by n matrix.
  */
int EIGEN_BLAS_FUNC(geru)(int *m, int *n, RealScalar *palpha, RealScalar *px, int *incx, RealScalar *py, int *incy, RealScalar *pa, int *lda)
{
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar* a = reinterpret_cast<Scalar*>(pa);
  Scalar alpha = *reinterpret_cast<Scalar*>(palpha);

  int info = 0;
       if(*m<0)                                                       info = 1;
  else if(*n<0)                                                       info = 2;
  else if(*incx==0)                                                   info = 5;
  else if(*incy==0)                                                   info = 7;
  else if(*lda<std::max(1,*m))                                        info = 9;
  if(info)
    return xerbla_(SCALAR_SUFFIX_UP"GERU ",&info,6);

  if(alpha==Scalar(0))
    return 1;

  Scalar* x_cpy = get_compact_vector(x,*m,*incx);
  Scalar* y_cpy = get_compact_vector(y,*n,*incy);

  // TODO perform direct calls to underlying implementation
  matrix(a,*m,*n,*lda) += alpha * vector(x_cpy,*m) * vector(y_cpy,*n).transpose();

  if(x_cpy!=x)  delete[] x_cpy;
  if(y_cpy!=y)  delete[] y_cpy;

  return 1;
}

/**  ZGERC  performs the rank 1 operation
  *
  *     A := alpha*x*conjg( y' ) + A,
  *
  *  where alpha is a scalar, x is an m element vector, y is an n element
  *  vector and A is an m by n matrix.
  */
int EIGEN_BLAS_FUNC(gerc)(int *m, int *n, RealScalar *palpha, RealScalar *px, int *incx, RealScalar *py, int *incy, RealScalar *pa, int *lda)
{
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar* a = reinterpret_cast<Scalar*>(pa);
  Scalar alpha = *reinterpret_cast<Scalar*>(palpha);

  int info = 0;
       if(*m<0)                                                       info = 1;
  else if(*n<0)                                                       info = 2;
  else if(*incx==0)                                                   info = 5;
  else if(*incy==0)                                                   info = 7;
  else if(*lda<std::max(1,*m))                                        info = 9;
  if(info)
    return xerbla_(SCALAR_SUFFIX_UP"GERC ",&info,6);

  if(alpha==Scalar(0))
    return 1;

  Scalar* x_cpy = get_compact_vector(x,*m,*incx);
  Scalar* y_cpy = get_compact_vector(y,*n,*incy);

  // TODO perform direct calls to underlying implementation
  matrix(a,*m,*n,*lda) += alpha * vector(x_cpy,*m) * vector(y_cpy,*n).adjoint();

  if(x_cpy!=x)  delete[] x_cpy;
  if(y_cpy!=y)  delete[] y_cpy;

  return 1;
}
