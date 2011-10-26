// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_PARALLELIZER_H
#define EIGEN_PARALLELIZER_H

namespace internal {

/** \internal */
inline void manage_multi_threading(Action action, int* v)
{
  static EIGEN_UNUSED int m_maxThreads = -1;

  if(action==SetAction)
  {
    eigen_internal_assert(v!=0);
    m_maxThreads = *v;
  }
  else if(action==GetAction)
  {
    eigen_internal_assert(v!=0);
    #ifdef EIGEN_HAS_OPENMP
    if(m_maxThreads>0)
      *v = m_maxThreads;
    else
      *v = omp_get_max_threads();
    #else
    *v = 1;
    #endif
  }
  else
  {
    eigen_internal_assert(false);
  }
}

/** \returns the max number of threads reserved for Eigen
  * \sa setNbThreads */
inline int nbThreads()
{
  int ret;
  manage_multi_threading(GetAction, &ret);
  return ret;
}

/** Sets the max number of threads reserved for Eigen
  * \sa nbThreads */
inline void setNbThreads(int v)
{
  manage_multi_threading(SetAction, &v);
}

template<typename Index> struct GemmParallelInfo
{
  GemmParallelInfo() : sync(-1), users(0), rhs_start(0), rhs_length(0) {}

  int volatile sync;
  int volatile users;

  Index rhs_start;
  Index rhs_length;
};

template<bool Condition, typename Functor, typename Index>
void parallelize_gemm(const Functor& func, Index rows, Index cols, bool transpose)
{
#ifndef EIGEN_HAS_OPENMP
  // FIXME the transpose variable is only needed to properly split
  // the matrix product when multithreading is enabled. This is a temporary
  // fix to support row-major destination matrices. This whole
  // parallelizer mechanism has to be redisigned anyway.
  EIGEN_UNUSED_VARIABLE(transpose);
  func(0,rows, 0,cols);
#else

  // Dynamically check whether we should enable or disable OpenMP.
  // The conditions are:
  // - the max number of threads we can create is greater than 1
  // - we are not already in a parallel code
  // - the sizes are large enough

  // 1- are we already in a parallel session?
  // FIXME omp_get_num_threads()>1 only works for openmp, what if the user does not use openmp?
  if((!Condition) || (omp_get_num_threads()>1))
    return func(0,rows, 0,cols);

  Index size = transpose ? cols : rows;

  // 2- compute the maximal number of threads from the size of the product:
  // FIXME this has to be fine tuned
  Index max_threads = std::max<Index>(1,size / 32);

  // 3 - compute the number of threads we are going to use
  Index threads = std::min<Index>(nbThreads(), max_threads);

  if(threads==1)
    return func(0,rows, 0,cols);

  func.initParallelSession();

  if(transpose)
    std::swap(rows,cols);

  Index blockCols = (cols / threads) & ~Index(0x3);
  Index blockRows = (rows / threads) & ~Index(0x7);
  
  GemmParallelInfo<Index>* info = new GemmParallelInfo<Index>[threads];

  #pragma omp parallel for schedule(static,1) num_threads(threads)
  for(Index i=0; i<threads; ++i)
  {
    Index r0 = i*blockRows;
    Index actualBlockRows = (i+1==threads) ? rows-r0 : blockRows;

    Index c0 = i*blockCols;
    Index actualBlockCols = (i+1==threads) ? cols-c0 : blockCols;

    info[i].rhs_start = c0;
    info[i].rhs_length = actualBlockCols;

    if(transpose)
      func(0, cols, r0, actualBlockRows, info);
    else
      func(r0, actualBlockRows, 0,cols, info);
  }

  delete[] info;
#endif
}

} // end namespace internal

#endif // EIGEN_PARALLELIZER_H
