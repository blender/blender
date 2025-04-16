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
// Author: sergiu.deitsch@gmail.com (Sergiu Deitsch)
//

#ifndef CERES_PUBLIC_INTERNAL_JET_TRAITS_H_
#define CERES_PUBLIC_INTERNAL_JET_TRAITS_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "ceres/internal/integer_sequence_algorithm.h"
#include "ceres/jet_fwd.h"

namespace ceres {
namespace internal {

// Predicate that determines whether any of the Types is a Jet.
template <typename... Types>
struct AreAnyJet : std::false_type {};

template <typename T, typename... Types>
struct AreAnyJet<T, Types...> : AreAnyJet<Types...> {};

template <typename T, int N, typename... Types>
struct AreAnyJet<Jet<T, N>, Types...> : std::true_type {};

// Convenience variable template for AreAnyJet.
template <typename... Types>
inline constexpr bool AreAnyJet_v = AreAnyJet<Types...>::value;

// Extracts the underlying floating-point from a type T.
template <typename T, typename E = void>
struct UnderlyingScalar {
  using type = T;
};

template <typename T, int N>
struct UnderlyingScalar<Jet<T, N>> : UnderlyingScalar<T> {};

// Convenience template alias for UnderlyingScalar type trait.
template <typename T>
using UnderlyingScalar_t = typename UnderlyingScalar<T>::type;

// Predicate determining whether all Types in the pack are the same.
//
// Specifically, the predicate applies std::is_same recursively to pairs of
// Types in the pack.
template <typename T1, typename... Types>
inline constexpr bool AreAllSame_v = (std::is_same<T1, Types>::value && ...);

// Determines the rank of a type. This allows to ensure that types passed as
// arguments are compatible to each other. The rank of Jet is determined by the
// dimensions of the dual part. The rank of a scalar is always 0.
// Non-specialized types default to a rank of -1.
template <typename T, typename E = void>
struct Rank : std::integral_constant<int, -1> {};

// The rank of a scalar is 0.
template <typename T>
struct Rank<T, std::enable_if_t<std::is_scalar<T>::value>>
    : std::integral_constant<int, 0> {};

// The rank of a Jet is given by its dimensionality.
template <typename T, int N>
struct Rank<Jet<T, N>> : std::integral_constant<int, N> {};

// Convenience variable template for Rank.
template <typename T>
inline constexpr int Rank_v = Rank<T>::value;

// Constructs an integer sequence of ranks for each of the Types in the pack.
template <typename... Types>
using Ranks_t = std::integer_sequence<int, Rank_v<Types>...>;

// Returns the scalar part of a type. This overload acts as an identity.
template <typename T>
constexpr decltype(auto) AsScalar(T&& value) noexcept {
  return std::forward<T>(value);
}

// Recursively unwraps the scalar part of a Jet until a non-Jet scalar type is
// encountered.
template <typename T, int N>
constexpr decltype(auto) AsScalar(const Jet<T, N>& value) noexcept(
    noexcept(AsScalar(value.a))) {
  return AsScalar(value.a);
}

}  // namespace internal

// Type trait ensuring at least one of the types is a Jet,
// the underlying scalar types are the same and Jet dimensions match.
//
// The type trait can be further specialized if necessary.
//
// This trait is a candidate for a concept definition once C++20 features can
// be used.
template <typename... Types>
// clang-format off
struct CompatibleJetOperands : std::integral_constant
<
    bool,
    // At least one of the types is a Jet
    internal::AreAnyJet_v<Types...> &&
    // The underlying floating-point types are exactly the same
    internal::AreAllSame_v<internal::UnderlyingScalar_t<Types>...> &&
    // Non-zero ranks of types are equal
    internal::IsEmptyOrAreAllEqual_v<internal::RemoveValue_t<internal::Ranks_t<Types...>, 0>>
>
// clang-format on
{};

// Single Jet operand is always compatible.
template <typename T, int N>
struct CompatibleJetOperands<Jet<T, N>> : std::true_type {};

// Single non-Jet operand is always incompatible.
template <typename T>
struct CompatibleJetOperands<T> : std::false_type {};

// Empty operands are always incompatible.
template <>
struct CompatibleJetOperands<> : std::false_type {};

// Convenience variable template ensuring at least one of the types is a Jet,
// the underlying scalar types are the same and Jet dimensions match.
//
// This trait is a candidate for a concept definition once C++20 features can
// be used.
template <typename... Types>
inline constexpr bool CompatibleJetOperands_v =
    CompatibleJetOperands<Types...>::value;

// Type trait ensuring at least one of the types is a Jet,
// the underlying scalar types are compatible among each other and Jet
// dimensions match.
//
// The type trait can be further specialized if necessary.
//
// This trait is a candidate for a concept definition once C++20 features can
// be used.
template <typename... Types>
// clang-format off
struct PromotableJetOperands : std::integral_constant
<
    bool,
    // Types can be compatible among each other
    internal::AreAnyJet_v<Types...> &&
    // Non-zero ranks of types are equal
    internal::IsEmptyOrAreAllEqual_v<internal::RemoveValue_t<internal::Ranks_t<Types...>, 0>>
>
// clang-format on
{};

// Convenience variable template ensuring at least one of the types is a Jet,
// the underlying scalar types are compatible among each other and Jet
// dimensions match.
//
// This trait is a candidate for a concept definition once C++20 features can
// be used.
template <typename... Types>
inline constexpr bool PromotableJetOperands_v =
    PromotableJetOperands<Types...>::value;

}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_JET_TRAITS_H_
