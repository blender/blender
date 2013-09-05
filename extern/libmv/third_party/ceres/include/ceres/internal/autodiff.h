// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)
//
// Computation of the Jacobian matrix for vector-valued functions of multiple
// variables, using automatic differentiation based on the implementation of
// dual numbers in jet.h. Before reading the rest of this file, it is adivsable
// to read jet.h's header comment in detail.
//
// The helper wrapper AutoDiff::Differentiate() computes the jacobian of
// functors with templated operator() taking this form:
//
//   struct F {
//     template<typename T>
//     bool operator()(const T *x, const T *y, ..., T *z) {
//       // Compute z[] based on x[], y[], ...
//       // return true if computation succeeded, false otherwise.
//     }
//   };
//
// All inputs and outputs may be vector-valued.
//
// To understand how jets are used to compute the jacobian, a
// picture may help. Consider a vector-valued function, F, returning 3
// dimensions and taking a vector-valued parameter of 4 dimensions:
//
//     y            x
//   [ * ]    F   [ * ]
//   [ * ]  <---  [ * ]
//   [ * ]        [ * ]
//                [ * ]
//
// Similar to the 2-parameter example for f described in jet.h, computing the
// jacobian dy/dx is done by substutiting a suitable jet object for x and all
// intermediate steps of the computation of F. Since x is has 4 dimensions, use
// a Jet<double, 4>.
//
// Before substituting a jet object for x, the dual components are set
// appropriately for each dimension of x:
//
//          y                       x
//   [ * | * * * * ]    f   [ * | 1 0 0 0 ]   x0
//   [ * | * * * * ]  <---  [ * | 0 1 0 0 ]   x1
//   [ * | * * * * ]        [ * | 0 0 1 0 ]   x2
//         ---+---          [ * | 0 0 0 1 ]   x3
//            |                   ^ ^ ^ ^
//          dy/dx                 | | | +----- infinitesimal for x3
//                                | | +------- infinitesimal for x2
//                                | +--------- infinitesimal for x1
//                                +----------- infinitesimal for x0
//
// The reason to set the internal 4x4 submatrix to the identity is that we wish
// to take the derivative of y separately with respect to each dimension of x.
// Each column of the 4x4 identity is therefore for a single component of the
// independent variable x.
//
// Then the jacobian of the mapping, dy/dx, is the 3x4 sub-matrix of the
// extended y vector, indicated in the above diagram.
//
// Functors with multiple parameters
// ---------------------------------
// In practice, it is often convenient to use a function f of two or more
// vector-valued parameters, for example, x[3] and z[6]. Unfortunately, the jet
// framework is designed for a single-parameter vector-valued input. The wrapper
// in this file addresses this issue adding support for functions with one or
// more parameter vectors.
//
// To support multiple parameters, all the parameter vectors are concatenated
// into one and treated as a single parameter vector, except that since the
// functor expects different inputs, we need to construct the jets as if they
// were part of a single parameter vector. The extended jets are passed
// separately for each parameter.
//
// For example, consider a functor F taking two vector parameters, p[2] and
// q[3], and producing an output y[4]:
//
//   struct F {
//     template<typename T>
//     bool operator()(const T *p, const T *q, T *z) {
//       // ...
//     }
//   };
//
// In this case, the necessary jet type is Jet<double, 5>. Here is a
// visualization of the jet objects in this case:
//
//          Dual components for p ----+
//                                    |
//                                   -+-
//           y                 [ * | 1 0 | 0 0 0 ]    --- p[0]
//                             [ * | 0 1 | 0 0 0 ]    --- p[1]
//   [ * | . . | + + + ]         |
//   [ * | . . | + + + ]         v
//   [ * | . . | + + + ]  <--- F(p, q)
//   [ * | . . | + + + ]            ^
//         ^^^   ^^^^^              |
//        dy/dp  dy/dq            [ * | 0 0 | 1 0 0 ] --- q[0]
//                                [ * | 0 0 | 0 1 0 ] --- q[1]
//                                [ * | 0 0 | 0 0 1 ] --- q[2]
//                                            --+--
//                                              |
//          Dual components for q --------------+
//
// where the 4x2 submatrix (marked with ".") and 4x3 submatrix (marked with "+"
// of y in the above diagram are the derivatives of y with respect to p and q
// respectively. This is how autodiff works for functors taking multiple vector
// valued arguments (up to 6).
//
// Jacobian NULL pointers
// ----------------------
// In general, the functions below will accept NULL pointers for all or some of
// the Jacobian parameters, meaning that those Jacobians will not be computed.

#ifndef CERES_PUBLIC_INTERNAL_AUTODIFF_H_
#define CERES_PUBLIC_INTERNAL_AUTODIFF_H_

#include <stddef.h>

#include "ceres/jet.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/variadic_evaluate.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Extends src by a 1st order pertubation for every dimension and puts it in
// dst. The size of src is N. Since this is also used for perturbations in
// blocked arrays, offset is used to shift which part of the jet the
// perturbation occurs. This is used to set up the extended x augmented by an
// identity matrix. The JetT type should be a Jet type, and T should be a
// numeric type (e.g. double). For example,
//
//             0   1 2   3 4 5   6 7 8
//   dst[0]  [ * | . . | 1 0 0 | . . . ]
//   dst[1]  [ * | . . | 0 1 0 | . . . ]
//   dst[2]  [ * | . . | 0 0 1 | . . . ]
//
// is what would get put in dst if N was 3, offset was 3, and the jet type JetT
// was 8-dimensional.
template <typename JetT, typename T, int N>
inline void Make1stOrderPerturbation(int offset, const T* src, JetT* dst) {
  DCHECK(src);
  DCHECK(dst);
  for (int j = 0; j < N; ++j) {
    dst[j].a = src[j];
    dst[j].v.setZero();
    dst[j].v[offset + j] = 1.0;
  }
}

// Takes the 0th order part of src, assumed to be a Jet type, and puts it in
// dst. This is used to pick out the "vector" part of the extended y.
template <typename JetT, typename T>
inline void Take0thOrderPart(int M, const JetT *src, T dst) {
  DCHECK(src);
  for (int i = 0; i < M; ++i) {
    dst[i] = src[i].a;
  }
}

// Takes N 1st order parts, starting at index N0, and puts them in the M x N
// matrix 'dst'. This is used to pick out the "matrix" parts of the extended y.
template <typename JetT, typename T, int N0, int N>
inline void Take1stOrderPart(const int M, const JetT *src, T *dst) {
  DCHECK(src);
  DCHECK(dst);
  for (int i = 0; i < M; ++i) {
    Eigen::Map<Eigen::Matrix<T, N, 1> >(dst + N * i, N) =
        src[i].v.template segment<N>(N0);
  }
}

// This is in a struct because default template parameters on a
// function are not supported in C++03 (though it is available in
// C++0x). N0 through N5 are the dimension of the input arguments to
// the user supplied functor.
template <typename Functor, typename T,
          int N0 = 0, int N1 = 0, int N2 = 0, int N3 = 0, int N4 = 0,
          int N5 = 0, int N6 = 0, int N7 = 0, int N8 = 0, int N9 = 0>
struct AutoDiff {
  static bool Differentiate(const Functor& functor,
                            T const *const *parameters,
                            int num_outputs,
                            T *function_value,
                            T **jacobians) {
    // This block breaks the 80 column rule to keep it somewhat readable.
    DCHECK_GT(num_outputs, 0);
    DCHECK((!N1 && !N2 && !N3 && !N4 && !N5 && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && !N2 && !N3 && !N4 && !N5 && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && !N3 && !N4 && !N5 && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && !N4 && !N5 && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && !N5 && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0) && !N6 && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0) && (N6 > 0) && !N7 && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0) && (N6 > 0) && (N7 > 0) && !N8 && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0) && (N6 > 0) && (N7 > 0) && (N8 > 0) && !N9) ||
          ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0) && (N6 > 0) && (N7 > 0) && (N8 > 0) && (N9 > 0)))
        << "Zero block cannot precede a non-zero block. Block sizes are "
        << "(ignore trailing 0s): " << N0 << ", " << N1 << ", " << N2 << ", "
        << N3 << ", " << N4 << ", " << N5 << ", " << N6 << ", " << N7 << ", "
        << N8 << ", " << N9;

    typedef Jet<T, N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7 + N8 + N9> JetT;
    FixedArray<JetT, (256 * 7) / sizeof(JetT)> x(
        N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7 + N8 + N9 + num_outputs);

    // These are the positions of the respective jets in the fixed array x.
    const int jet0  = 0;
    const int jet1  = N0;
    const int jet2  = N0 + N1;
    const int jet3  = N0 + N1 + N2;
    const int jet4  = N0 + N1 + N2 + N3;
    const int jet5  = N0 + N1 + N2 + N3 + N4;
    const int jet6  = N0 + N1 + N2 + N3 + N4 + N5;
    const int jet7  = N0 + N1 + N2 + N3 + N4 + N5 + N6;
    const int jet8  = N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7;
    const int jet9  = N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7 + N8;

    const JetT *unpacked_parameters[10] = {
        x.get() + jet0,
        x.get() + jet1,
        x.get() + jet2,
        x.get() + jet3,
        x.get() + jet4,
        x.get() + jet5,
        x.get() + jet6,
        x.get() + jet7,
        x.get() + jet8,
        x.get() + jet9,
    };

    JetT* output = x.get() + N0 + N1 + N2 + N3 + N4 + N5 + N6 + N7 + N8 + N9;

#define CERES_MAKE_1ST_ORDER_PERTURBATION(i)                            \
    if (N ## i) {                                                       \
      internal::Make1stOrderPerturbation<JetT, T, N ## i>(              \
          jet ## i,                                                     \
          parameters[i],                                                \
          x.get() + jet ## i);                                          \
    }
    CERES_MAKE_1ST_ORDER_PERTURBATION(0);
    CERES_MAKE_1ST_ORDER_PERTURBATION(1);
    CERES_MAKE_1ST_ORDER_PERTURBATION(2);
    CERES_MAKE_1ST_ORDER_PERTURBATION(3);
    CERES_MAKE_1ST_ORDER_PERTURBATION(4);
    CERES_MAKE_1ST_ORDER_PERTURBATION(5);
    CERES_MAKE_1ST_ORDER_PERTURBATION(6);
    CERES_MAKE_1ST_ORDER_PERTURBATION(7);
    CERES_MAKE_1ST_ORDER_PERTURBATION(8);
    CERES_MAKE_1ST_ORDER_PERTURBATION(9);
#undef CERES_MAKE_1ST_ORDER_PERTURBATION

    if (!VariadicEvaluate<Functor, JetT,
                          N0, N1, N2, N3, N4, N5, N6, N7, N8, N9>::Call(
        functor, unpacked_parameters, output)) {
      return false;
    }

    internal::Take0thOrderPart(num_outputs, output, function_value);

#define CERES_TAKE_1ST_ORDER_PERTURBATION(i) \
    if (N ## i) { \
      if (jacobians[i]) { \
        internal::Take1stOrderPart<JetT, T, \
                                   jet ## i, \
                                   N ## i>(num_outputs, \
                                           output, \
                                           jacobians[i]); \
      } \
    }
    CERES_TAKE_1ST_ORDER_PERTURBATION(0);
    CERES_TAKE_1ST_ORDER_PERTURBATION(1);
    CERES_TAKE_1ST_ORDER_PERTURBATION(2);
    CERES_TAKE_1ST_ORDER_PERTURBATION(3);
    CERES_TAKE_1ST_ORDER_PERTURBATION(4);
    CERES_TAKE_1ST_ORDER_PERTURBATION(5);
    CERES_TAKE_1ST_ORDER_PERTURBATION(6);
    CERES_TAKE_1ST_ORDER_PERTURBATION(7);
    CERES_TAKE_1ST_ORDER_PERTURBATION(8);
    CERES_TAKE_1ST_ORDER_PERTURBATION(9);
#undef CERES_TAKE_1ST_ORDER_PERTURBATION
    return true;
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_AUTODIFF_H_
