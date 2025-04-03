// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
// Author: jodebo_beck@gmx.de (Johannes Beck)
//         sergiu.deitsch@gmail.com (Sergiu Deitsch)
//
// Algorithms to be used together with integer_sequence, like computing the sum
// or the exclusive scan (sometimes called exclusive prefix sum) at compile
// time.

#ifndef CERES_PUBLIC_INTERNAL_INTEGER_SEQUENCE_ALGORITHM_H_
#define CERES_PUBLIC_INTERNAL_INTEGER_SEQUENCE_ALGORITHM_H_

#include <utility>

#include "ceres/jet_fwd.h"

namespace ceres::internal {

// Implementation of calculating an exclusive scan (exclusive prefix sum) of an
// integer sequence. Exclusive means that the i-th input element is not included
// in the i-th sum. Calculating the exclusive scan for an input array I results
// in the following output R:
//
// R[0] = 0
// R[1] = I[0];
// R[2] = I[0] + I[1];
// R[3] = I[0] + I[1] + I[2];
// ...
//
// In C++17 std::exclusive_scan does the same operation at runtime (but
// cannot be used to calculate the prefix sum at compile time). See
// https://en.cppreference.com/w/cpp/algorithm/exclusive_scan for a more
// detailed description.
//
// Example for integer_sequence<int, 1, 4, 3> (seq := integer_sequence):
//                   T  , Sum,          Ns...   ,          Rs...
// ExclusiveScanImpl<int,   0, seq<int, 1, 4, 3>, seq<int         >>
// ExclusiveScanImpl<int,   1, seq<int,    4, 3>, seq<int, 0      >>
// ExclusiveScanImpl<int,   5, seq<int,       3>, seq<int, 0, 1   >>
// ExclusiveScanImpl<int,   8, seq<int         >, seq<int, 0, 1, 5>>
//                                                ^^^^^^^^^^^^^^^^^
//                                                resulting sequence
template <typename T, T Sum, typename SeqIn, typename SeqOut>
struct ExclusiveScanImpl;

template <typename T, T Sum, T N, T... Ns, T... Rs>
struct ExclusiveScanImpl<T,
                         Sum,
                         std::integer_sequence<T, N, Ns...>,
                         std::integer_sequence<T, Rs...>> {
  using Type =
      typename ExclusiveScanImpl<T,
                                 Sum + N,
                                 std::integer_sequence<T, Ns...>,
                                 std::integer_sequence<T, Rs..., Sum>>::Type;
};

// End of 'recursion'. The resulting type is SeqOut.
template <typename T, T Sum, typename SeqOut>
struct ExclusiveScanImpl<T, Sum, std::integer_sequence<T>, SeqOut> {
  using Type = SeqOut;
};

// Calculates the exclusive scan of the specified integer sequence. The last
// element (the total) is not included in the resulting sequence so they have
// same length. This means the exclusive scan of integer_sequence<int, 1, 2, 3>
// will be integer_sequence<int, 0, 1, 3>.
template <typename Seq>
class ExclusiveScanT {
  using T = typename Seq::value_type;

 public:
  using Type =
      typename ExclusiveScanImpl<T, T(0), Seq, std::integer_sequence<T>>::Type;
};

// Helper to use exclusive scan without typename.
template <typename Seq>
using ExclusiveScan = typename ExclusiveScanT<Seq>::Type;

// Removes all elements from a integer sequence corresponding to specified
// ValueToRemove.
//
// This type should not be used directly but instead RemoveValue.
template <typename T, T ValueToRemove, typename... Sequence>
struct RemoveValueImpl;

// Final filtered sequence
template <typename T, T ValueToRemove, T... Values>
struct RemoveValueImpl<T,
                       ValueToRemove,
                       std::integer_sequence<T, Values...>,
                       std::integer_sequence<T>> {
  using type = std::integer_sequence<T, Values...>;
};

// Found a matching value
template <typename T, T ValueToRemove, T... Head, T... Tail>
struct RemoveValueImpl<T,
                       ValueToRemove,
                       std::integer_sequence<T, Head...>,
                       std::integer_sequence<T, ValueToRemove, Tail...>>
    : RemoveValueImpl<T,
                      ValueToRemove,
                      std::integer_sequence<T, Head...>,
                      std::integer_sequence<T, Tail...>> {};

// Move one element from the tail to the head
template <typename T, T ValueToRemove, T... Head, T MiddleValue, T... Tail>
struct RemoveValueImpl<T,
                       ValueToRemove,
                       std::integer_sequence<T, Head...>,
                       std::integer_sequence<T, MiddleValue, Tail...>>
    : RemoveValueImpl<T,
                      ValueToRemove,
                      std::integer_sequence<T, Head..., MiddleValue>,
                      std::integer_sequence<T, Tail...>> {};

// Start recursion by splitting the integer sequence into two separate ones
template <typename T, T ValueToRemove, T... Tail>
struct RemoveValueImpl<T, ValueToRemove, std::integer_sequence<T, Tail...>>
    : RemoveValueImpl<T,
                      ValueToRemove,
                      std::integer_sequence<T>,
                      std::integer_sequence<T, Tail...>> {};

// RemoveValue takes an integer Sequence of arbitrary type and removes all
// elements matching ValueToRemove.
//
// In contrast to RemoveValueImpl, this implementation deduces the value type
// eliminating the need to specify it explicitly.
//
// As an example, RemoveValue<std::integer_sequence<int, 1, 2, 3>, 4>::type will
// not transform the type of the original sequence. However,
// RemoveValue<std::integer_sequence<int, 0, 0, 2>, 2>::type will generate a new
// sequence of type std::integer_sequence<int, 0, 0> by removing the value 2.
template <typename Sequence, typename Sequence::value_type ValueToRemove>
struct RemoveValue
    : RemoveValueImpl<typename Sequence::value_type, ValueToRemove, Sequence> {
};

// Convenience template alias for RemoveValue.
template <typename Sequence, typename Sequence::value_type ValueToRemove>
using RemoveValue_t = typename RemoveValue<Sequence, ValueToRemove>::type;

// Returns true if all elements of Values are equal to HeadValue.
//
// Returns true if Values is empty.
template <typename T, T HeadValue, T... Values>
inline constexpr bool AreAllEqual_v = ((HeadValue == Values) && ...);

// Predicate determining whether an integer sequence is either empty or all
// values are equal.
template <typename Sequence>
struct IsEmptyOrAreAllEqual;

// Empty case.
template <typename T>
struct IsEmptyOrAreAllEqual<std::integer_sequence<T>> : std::true_type {};

// General case for sequences containing at least one value.
template <typename T, T HeadValue, T... Values>
struct IsEmptyOrAreAllEqual<std::integer_sequence<T, HeadValue, Values...>>
    : std::integral_constant<bool, AreAllEqual_v<T, HeadValue, Values...>> {};

// Convenience variable template for IsEmptyOrAreAllEqual.
template <class Sequence>
inline constexpr bool IsEmptyOrAreAllEqual_v =
    IsEmptyOrAreAllEqual<Sequence>::value;

}  // namespace ceres::internal

#endif  // CERES_PUBLIC_INTERNAL_INTEGER_SEQUENCE_ALGORITHM_H_
