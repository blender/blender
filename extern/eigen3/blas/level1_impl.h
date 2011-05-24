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

int EIGEN_BLAS_FUNC(axpy)(int *n, RealScalar *palpha, RealScalar *px, int *incx, RealScalar *py, int *incy)
{
  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar alpha  = *reinterpret_cast<Scalar*>(palpha);

  if(*n<=0) return 0;

  if(*incx==1 && *incy==1)    vector(y,*n) += alpha * vector(x,*n);
  else if(*incx>0 && *incy>0) vector(y,*n,*incy) += alpha * vector(x,*n,*incx);
  else if(*incx>0 && *incy<0) vector(y,*n,-*incy).reverse() += alpha * vector(x,*n,*incx);
  else if(*incx<0 && *incy>0) vector(y,*n,*incy) += alpha * vector(x,*n,-*incx).reverse();
  else if(*incx<0 && *incy<0) vector(y,*n,-*incy).reverse() += alpha * vector(x,*n,-*incx).reverse();

  return 0;
}

int EIGEN_BLAS_FUNC(copy)(int *n, RealScalar *px, int *incx, RealScalar *py, int *incy)
{
  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);

  // be carefull, *incx==0 is allowed !!
  if(*incx==1 && *incy==1)
    vector(y,*n) = vector(x,*n);
  else
  {
    if(*incx<0) x = x - (*n-1)*(*incx);
    if(*incy<0) y = y - (*n-1)*(*incy);
    for(int i=0;i<*n;++i)
    {
      *y = *x;
      x += *incx;
      y += *incy;
    }
  }

  return 0;
}

int EIGEN_CAT(EIGEN_CAT(i,SCALAR_SUFFIX),amax_)(int *n, RealScalar *px, int *incx)
{
  if(*n<=0) return 0;
  Scalar* x = reinterpret_cast<Scalar*>(px);

  DenseIndex ret;
  if(*incx==1)  vector(x,*n).cwiseAbs().maxCoeff(&ret);
  else          vector(x,*n,std::abs(*incx)).cwiseAbs().maxCoeff(&ret);
  return ret+1;
}

int EIGEN_CAT(EIGEN_CAT(i,SCALAR_SUFFIX),amin_)(int *n, RealScalar *px, int *incx)
{
  if(*n<=0) return 0;
  Scalar* x = reinterpret_cast<Scalar*>(px);
  
  DenseIndex ret;
  if(*incx==1)  vector(x,*n).cwiseAbs().minCoeff(&ret);
  else          vector(x,*n,std::abs(*incx)).cwiseAbs().minCoeff(&ret);
  return ret+1;
}

int EIGEN_BLAS_FUNC(rotg)(RealScalar *pa, RealScalar *pb, RealScalar *pc, RealScalar *ps)
{
  Scalar& a = *reinterpret_cast<Scalar*>(pa);
  Scalar& b = *reinterpret_cast<Scalar*>(pb);
  RealScalar* c = pc;
  Scalar* s = reinterpret_cast<Scalar*>(ps);

  #if !ISCOMPLEX
  Scalar r,z;
  Scalar aa = internal::abs(a);
  Scalar ab = internal::abs(b);
  if((aa+ab)==Scalar(0))
  {
    *c = 1;
    *s = 0;
    r = 0;
    z = 0;
  }
  else
  {
    r = internal::sqrt(a*a + b*b);
    Scalar amax = aa>ab ? a : b;
    r = amax>0 ? r : -r;
    *c = a/r;
    *s = b/r;
    z = 1;
    if (aa > ab) z = *s;
    if (ab > aa && *c!=RealScalar(0))
      z = Scalar(1)/ *c;
  }
  *pa = r;
  *pb = z;
  #else
  Scalar alpha;
  RealScalar norm,scale;
  if(internal::abs(a)==RealScalar(0))
  {
    *c = RealScalar(0);
    *s = Scalar(1);
    a = b;
  }
  else
  {
    scale = internal::abs(a) + internal::abs(b);
    norm = scale*internal::sqrt((internal::abs2(a/scale))+ (internal::abs2(b/scale)));
    alpha = a/internal::abs(a);
    *c = internal::abs(a)/norm;
    *s = alpha*internal::conj(b)/norm;
    a = alpha*norm;
  }
  #endif

//   JacobiRotation<Scalar> r;
//   r.makeGivens(a,b);
//   *c = r.c();
//   *s = r.s();

  return 0;
}

int EIGEN_BLAS_FUNC(scal)(int *n, RealScalar *palpha, RealScalar *px, int *incx)
{
  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar alpha = *reinterpret_cast<Scalar*>(palpha);

  if(*incx==1)  vector(x,*n) *= alpha;
  else          vector(x,*n,std::abs(*incx)) *= alpha;

  return 0;
}

int EIGEN_BLAS_FUNC(swap)(int *n, RealScalar *px, int *incx, RealScalar *py, int *incy)
{
  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);

  if(*incx==1 && *incy==1)    vector(y,*n).swap(vector(x,*n));
  else if(*incx>0 && *incy>0) vector(y,*n,*incy).swap(vector(x,*n,*incx));
  else if(*incx>0 && *incy<0) vector(y,*n,-*incy).reverse().swap(vector(x,*n,*incx));
  else if(*incx<0 && *incy>0) vector(y,*n,*incy).swap(vector(x,*n,-*incx).reverse());
  else if(*incx<0 && *incy<0) vector(y,*n,-*incy).reverse().swap(vector(x,*n,-*incx).reverse());

  return 1;
}

