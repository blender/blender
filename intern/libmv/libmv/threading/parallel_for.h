// Copyright (c) 2025 libmv authors.
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

// TBB-compatible implementation of parallel_for() functions.
//
// The parallel_for() function applies given functor for every index within the
// given range. The range might be provided in multiple ways.
//
// Supports multiple underlying threading implementations (in the order of
// preference):
// - TBB, requires LIBMV_USE_TBB_THREADS to be defined.
// - OpenMP, requires compiler to use -fopenmp, used when _OPENMP is defined.
// - Single-threaded fall-back.
//
// The function occupies all threads of the default theading pool.

#ifndef LIBMV_THREADING_PARALLEL_FOR_H_
#define LIBMV_THREADING_PARALLEL_FOR_H_

#if defined(LIBMV_USE_TBB_THREADS)
#  include <tbb/parallel_for.h>
#endif

namespace libmv {

// Run the function f() for all indices within [first, last).
template <typename Index, typename Function>
void parallel_for(const Index first, const Index last, const Function& f) {
#if defined(LIBMV_USE_TBB_THREADS)
  tbb::parallel_for(first, last, f);
#else
#  if defined(_OPENMP)
#    pragma omp parallel for schedule(static)
#  endif
  for (Index i = first; i < last; ++i) {
    f(i);
  }
#endif
}

}  // namespace libmv

#endif  // LIBMV_THREADING_PARALLEL_FOR_H_
