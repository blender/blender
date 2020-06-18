// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
// http://ceres-solver.org/
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
// dual numbers in jet.h. Before reading the rest of this file, it is advisable
// to read jet.h's header comment in detail.
//
// The helper wrapper AutoDifferentiate() computes the jacobian of
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
// jacobian dy/dx is done by substituting a suitable jet object for x and all
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

#include <array>
#include <utility>

#include "ceres/internal/array_selector.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/parameter_dims.h"
#include "ceres/internal/variadic_evaluate.h"
#include "ceres/jet.h"
#include "ceres/types.h"
#include "glog/logging.h"

// If the number of parameters exceeds this values, the corresponding jets are
// placed on the heap. This will reduce performance by a factor of 2-5 on
// current compilers.
#ifndef CERES_AUTODIFF_MAX_PARAMETERS_ON_STACK
#define CERES_AUTODIFF_MAX_PARAMETERS_ON_STACK 50
#endif

#ifndef CERES_AUTODIFF_MAX_RESIDUALS_ON_STACK
#define CERES_AUTODIFF_MAX_RESIDUALS_ON_STACK 20
#endif

namespace ceres {
namespace internal {

// Extends src by a 1st order perturbation for every dimension and puts it in
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
template <int j, int N, int Offset, typename T, typename JetT>
struct Make1stOrderPerturbation {
 public:
  inline static void Apply(const T* src, JetT* dst) {
    if (j == 0) {
      DCHECK(src);
      DCHECK(dst);
    }
    dst[j] = JetT(src[j], j + Offset);
    Make1stOrderPerturbation<j + 1, N, Offset, T, JetT>::Apply(src, dst);
  }
};

template <int N, int Offset, typename T, typename JetT>
struct Make1stOrderPerturbation<N, N, Offset, T, JetT> {
 public:
  static void Apply(const T* /*src*/, JetT* /*dst*/) {}
};

// Calls Make1stOrderPerturbation for every parameter block.
//
// Example:
// If one having three parameter blocks with dimensions (3, 2, 4), the call
// Make1stOrderPerturbations<integer_sequence<3, 2, 4>::Apply(params, x);
// will result in the following calls to Make1stOrderPerturbation:
// Make1stOrderPerturbation<0, 3, 0>::Apply(params[0], x + 0);
// Make1stOrderPerturbation<0, 2, 3>::Apply(params[1], x + 3);
// Make1stOrderPerturbation<0, 4, 5>::Apply(params[2], x + 5);
template <typename Seq, int ParameterIdx = 0, int Offset = 0>
struct Make1stOrderPerturbations;

template <int N, int... Ns, int ParameterIdx, int Offset>
struct Make1stOrderPerturbations<std::integer_sequence<int, N, Ns...>,
                                 ParameterIdx,
                                 Offset> {
  template <typename T, typename JetT>
  inline static void Apply(T const* const* parameters, JetT* x) {
    Make1stOrderPerturbation<0, N, Offset, T, JetT>::Apply(
        parameters[ParameterIdx], x + Offset);
    Make1stOrderPerturbations<std::integer_sequence<int, Ns...>,
                              ParameterIdx + 1,
                              Offset + N>::Apply(parameters, x);
  }
};

// End of 'recursion'. Nothing more to do.
template <int ParameterIdx, int Total>
struct Make1stOrderPerturbations<std::integer_sequence<int>, ParameterIdx, Total> {
  template <typename T, typename JetT>
  static void Apply(T const* const* /* NOT USED */, JetT* /* NOT USED */) {}
};

// Takes the 0th order part of src, assumed to be a Jet type, and puts it in
// dst. This is used to pick out the "vector" part of the extended y.
template <typename JetT, typename T>
inline void Take0thOrderPart(int M, const JetT* src, T dst) {
  DCHECK(src);
  for (int i = 0; i < M; ++i) {
    dst[i] = src[i].a;
  }
}

// Takes N 1st order parts, starting at index N0, and puts them in the M x N
// matrix 'dst'. This is used to pick out the "matrix" parts of the extended y.
template <int N0, int N, typename JetT, typename T>
inline void Take1stOrderPart(const int M, const JetT* src, T* dst) {
  DCHECK(src);
  DCHECK(dst);
  for (int i = 0; i < M; ++i) {
    Eigen::Map<Eigen::Matrix<T, N, 1>>(dst + N * i, N) =
        src[i].v.template segment<N>(N0);
  }
}

// Calls Take1stOrderPart for every parameter block.
//
// Example:
// If one having three parameter blocks with dimensions (3, 2, 4), the call
// Take1stOrderParts<integer_sequence<3, 2, 4>::Apply(num_outputs,
//                                                    output,
//                                                    jacobians);
// will result in the following calls to Take1stOrderPart:
// if (jacobians[0]) {
//   Take1stOrderPart<0, 3>(num_outputs, output, jacobians[0]);
// }
// if (jacobians[1]) {
//   Take1stOrderPart<3, 2>(num_outputs, output, jacobians[1]);
// }
// if (jacobians[2]) {
//   Take1stOrderPart<5, 4>(num_outputs, output, jacobians[2]);
// }
template <typename Seq, int ParameterIdx = 0, int Offset = 0>
struct Take1stOrderParts;

template <int N, int... Ns, int ParameterIdx, int Offset>
struct Take1stOrderParts<std::integer_sequence<int, N, Ns...>,
                         ParameterIdx,
                         Offset> {
  template <typename JetT, typename T>
  inline static void Apply(int num_outputs, JetT* output, T** jacobians) {
    if (jacobians[ParameterIdx]) {
      Take1stOrderPart<Offset, N>(num_outputs, output, jacobians[ParameterIdx]);
    }
    Take1stOrderParts<std::integer_sequence<int, Ns...>,
                      ParameterIdx + 1,
                      Offset + N>::Apply(num_outputs, output, jacobians);
  }
};

// End of 'recursion'. Nothing more to do.
template <int ParameterIdx, int Offset>
struct Take1stOrderParts<std::integer_sequence<int>, ParameterIdx, Offset> {
  template <typename T, typename JetT>
  static void Apply(int /* NOT USED*/,
                    JetT* /* NOT USED*/,
                    T** /* NOT USED */) {}
};

template <int kNumResiduals,
          typename ParameterDims,
          typename Functor,
          typename T>
inline bool AutoDifferentiate(const Functor& functor,
                              T const* const* parameters,
                              int dynamic_num_outputs,
                              T* function_value,
                              T** jacobians) {
  typedef Jet<T, ParameterDims::kNumParameters> JetT;
  using Parameters = typename ParameterDims::Parameters;

  if (kNumResiduals != DYNAMIC) {
    DCHECK_EQ(kNumResiduals, dynamic_num_outputs);
  }

  ArraySelector<JetT,
                ParameterDims::kNumParameters,
                CERES_AUTODIFF_MAX_PARAMETERS_ON_STACK>
      parameters_as_jets(ParameterDims::kNumParameters);

  // Pointers to the beginning of each parameter block
  std::array<JetT*, ParameterDims::kNumParameterBlocks> unpacked_parameters =
      ParameterDims::GetUnpackedParameters(parameters_as_jets.data());

  // If the number of residuals is fixed, we use the template argument as the
  // number of outputs. Otherwise we use the num_outputs parameter. Note: The
  // ?-operator here is compile-time evaluated, therefore num_outputs is also
  // a compile-time constant for functors with fixed residuals.
  const int num_outputs =
      kNumResiduals == DYNAMIC ? dynamic_num_outputs : kNumResiduals;
  DCHECK_GT(num_outputs, 0);

  ArraySelector<JetT, kNumResiduals, CERES_AUTODIFF_MAX_RESIDUALS_ON_STACK>
      residuals_as_jets(num_outputs);

  // Invalidate the output Jets, so that we can detect if the user
  // did not assign values to all of them.
  for (int i = 0; i < num_outputs; ++i) {
    residuals_as_jets[i].a = kImpossibleValue;
    residuals_as_jets[i].v.setConstant(kImpossibleValue);
  }

  Make1stOrderPerturbations<Parameters>::Apply(parameters,
                                               parameters_as_jets.data());

  if (!VariadicEvaluate<ParameterDims>(
          functor, unpacked_parameters.data(), residuals_as_jets.data())) {
    return false;
  }

  Take0thOrderPart(num_outputs, residuals_as_jets.data(), function_value);
  Take1stOrderParts<Parameters>::Apply(
      num_outputs, residuals_as_jets.data(), jacobians);

  return true;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_AUTODIFF_H_
