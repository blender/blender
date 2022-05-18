// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2022 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         sergiu.deitsch@gmail.com (Sergiu Deitsch)
//

#ifndef CERES_PUBLIC_PRODUCT_MANIFOLD_H_
#define CERES_PUBLIC_PRODUCT_MANIFOLD_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <utility>

#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/port.h"
#include "ceres/manifold.h"

namespace ceres {

// Construct a manifold by taking the Cartesian product of a number of other
// manifolds. This is useful, when a parameter block is the Cartesian product
// of two or more manifolds. For example the parameters of a camera consist of
// a rotation and a translation, i.e., SO(3) x R^3.
//
// Example usage:
//
// ProductManifold<QuaternionManifold, EuclideanManifold<3>> se3;
//
// is the manifold for a rigid transformation, where the rotation is
// represented using a quaternion.
//
// Manifolds can be copied and moved to ProductManifold:
//
// SubsetManifold manifold1(5, {2});
// SubsetManifold manifold2(3, {0, 1});
// ProductManifold<SubsetManifold, SubsetManifold> manifold(manifold1,
//                                                          manifold2);
//
// In advanced use cases, manifolds can be dynamically allocated and passed as
// (smart) pointers:
//
// ProductManifold<std::unique_ptr<QuaternionManifold>, EuclideanManifold<3>>
//     se3{std::make_unique<QuaternionManifold>(), EuclideanManifold<3>{}};
//
// In C++17, the template parameters can be left out as they are automatically
// deduced making the initialization much simpler:
//
// ProductManifold se3{QuaternionManifold{}, EuclideanManifold<3>{}};
//
// The manifold implementations must be either default constructible, copyable
// or moveable to be usable in a ProductManifold.
template <typename Manifold0, typename Manifold1, typename... ManifoldN>
class ProductManifold final : public Manifold {
 public:
  // ProductManifold constructor perfect forwards arguments to store manifolds.
  //
  // Either use default construction or if you need to copy or move-construct a
  // manifold instance, you need to pass an instance as an argument for all
  // types given as class template parameters.
  template <typename... Args,
            std::enable_if_t<std::is_constructible<
                std::tuple<Manifold0, Manifold1, ManifoldN...>,
                Args...>::value>* = nullptr>
  explicit ProductManifold(Args&&... manifolds)
      : ProductManifold{std::make_index_sequence<kNumManifolds>{},
                        std::forward<Args>(manifolds)...} {}

  int AmbientSize() const override { return ambient_size_; }
  int TangentSize() const override { return tangent_size_; }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override {
    return PlusImpl(
        x, delta, x_plus_delta, std::make_index_sequence<kNumManifolds>{});
  }

  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override {
    return MinusImpl(
        y, x, y_minus_x, std::make_index_sequence<kNumManifolds>{});
  }

  bool PlusJacobian(const double* x, double* jacobian_ptr) const override {
    MatrixRef jacobian(jacobian_ptr, AmbientSize(), TangentSize());
    jacobian.setZero();
    internal::FixedArray<double> buffer(buffer_size_);

    return PlusJacobianImpl(
        x, jacobian, buffer, std::make_index_sequence<kNumManifolds>{});
  }

  bool MinusJacobian(const double* x, double* jacobian_ptr) const override {
    MatrixRef jacobian(jacobian_ptr, TangentSize(), AmbientSize());
    jacobian.setZero();
    internal::FixedArray<double> buffer(buffer_size_);

    return MinusJacobianImpl(
        x, jacobian, buffer, std::make_index_sequence<kNumManifolds>{});
  }

 private:
  static constexpr std::size_t kNumManifolds = 2 + sizeof...(ManifoldN);

  template <std::size_t... Indices, typename... Args>
  explicit ProductManifold(std::index_sequence<Indices...>, Args&&... manifolds)
      : manifolds_{std::forward<Args>(manifolds)...},
        buffer_size_{(std::max)(
            {(Dereference(std::get<Indices>(manifolds_)).TangentSize() *
              Dereference(std::get<Indices>(manifolds_)).AmbientSize())...})},
        ambient_sizes_{
            Dereference(std::get<Indices>(manifolds_)).AmbientSize()...},
        tangent_sizes_{
            Dereference(std::get<Indices>(manifolds_)).TangentSize()...},
        ambient_offsets_{ExclusiveScan(ambient_sizes_)},
        tangent_offsets_{ExclusiveScan(tangent_sizes_)},
        ambient_size_{
            std::accumulate(ambient_sizes_.begin(), ambient_sizes_.end(), 0)},
        tangent_size_{
            std::accumulate(tangent_sizes_.begin(), tangent_sizes_.end(), 0)} {}

  template <std::size_t Index0, std::size_t... Indices>
  bool PlusImpl(const double* x,
                const double* delta,
                double* x_plus_delta,
                std::index_sequence<Index0, Indices...>) const {
    if (!Dereference(std::get<Index0>(manifolds_))
             .Plus(x + ambient_offsets_[Index0],
                   delta + tangent_offsets_[Index0],
                   x_plus_delta + ambient_offsets_[Index0])) {
      return false;
    }

    return PlusImpl(x, delta, x_plus_delta, std::index_sequence<Indices...>{});
  }

  static constexpr bool PlusImpl(const double* /*x*/,
                                 const double* /*delta*/,
                                 double* /*x_plus_delta*/,
                                 std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool MinusImpl(const double* y,
                 const double* x,
                 double* y_minus_x,
                 std::index_sequence<Index0, Indices...>) const {
    if (!Dereference(std::get<Index0>(manifolds_))
             .Minus(y + ambient_offsets_[Index0],
                    x + ambient_offsets_[Index0],
                    y_minus_x + tangent_offsets_[Index0])) {
      return false;
    }

    return MinusImpl(y, x, y_minus_x, std::index_sequence<Indices...>{});
  }

  static constexpr bool MinusImpl(const double* /*y*/,
                                  const double* /*x*/,
                                  double* /*y_minus_x*/,
                                  std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool PlusJacobianImpl(const double* x,
                        MatrixRef& jacobian,
                        internal::FixedArray<double>& buffer,
                        std::index_sequence<Index0, Indices...>) const {
    if (!Dereference(std::get<Index0>(manifolds_))
             .PlusJacobian(x + ambient_offsets_[Index0], buffer.data())) {
      return false;
    }

    jacobian.block(ambient_offsets_[Index0],
                   tangent_offsets_[Index0],
                   ambient_sizes_[Index0],
                   tangent_sizes_[Index0]) =
        MatrixRef(
            buffer.data(), ambient_sizes_[Index0], tangent_sizes_[Index0]);

    return PlusJacobianImpl(
        x, jacobian, buffer, std::index_sequence<Indices...>{});
  }

  static constexpr bool PlusJacobianImpl(
      const double* /*x*/,
      MatrixRef& /*jacobian*/,
      internal::FixedArray<double>& /*buffer*/,
      std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool MinusJacobianImpl(const double* x,
                         MatrixRef& jacobian,
                         internal::FixedArray<double>& buffer,
                         std::index_sequence<Index0, Indices...>) const {
    if (!Dereference(std::get<Index0>(manifolds_))
             .MinusJacobian(x + ambient_offsets_[Index0], buffer.data())) {
      return false;
    }

    jacobian.block(tangent_offsets_[Index0],
                   ambient_offsets_[Index0],
                   tangent_sizes_[Index0],
                   ambient_sizes_[Index0]) =
        MatrixRef(
            buffer.data(), tangent_sizes_[Index0], ambient_sizes_[Index0]);

    return MinusJacobianImpl(
        x, jacobian, buffer, std::index_sequence<Indices...>{});
  }

  static constexpr bool MinusJacobianImpl(
      const double* /*x*/,
      MatrixRef& /*jacobian*/,
      internal::FixedArray<double>& /*buffer*/,
      std::index_sequence<>) noexcept {
    return true;
  }

  template <typename T, std::size_t N>
  static std::array<T, N> ExclusiveScan(const std::array<T, N>& values) {
    std::array<T, N> result;
    T init = 0;

    // TODO Replace by std::exclusive_scan once C++17 is available
    for (std::size_t i = 0; i != N; ++i) {
      result[i] = init;
      init += values[i];
    }

    return result;
  }

  // TODO Replace by std::void_t once C++17 is available
  template <typename... Types>
  struct Void {
    using type = void;
  };

  template <typename T, typename E = void>
  struct IsDereferenceable : std::false_type {};

  template <typename T>
  struct IsDereferenceable<T, typename Void<decltype(*std::declval<T>())>::type>
      : std::true_type {};

  template <typename T,
            std::enable_if_t<!IsDereferenceable<T>::value>* = nullptr>
  static constexpr decltype(auto) Dereference(T& value) {
    return value;
  }

  // Support dereferenceable types such as std::unique_ptr, std::shared_ptr, raw
  // pointers etc.
  template <typename T,
            std::enable_if_t<IsDereferenceable<T>::value>* = nullptr>
  static constexpr decltype(auto) Dereference(T& value) {
    return *value;
  }

  template <typename T>
  static constexpr decltype(auto) Dereference(T* p) {
    assert(p != nullptr);
    return *p;
  }

  std::tuple<Manifold0, Manifold1, ManifoldN...> manifolds_;
  int buffer_size_;
  std::array<int, kNumManifolds> ambient_sizes_;
  std::array<int, kNumManifolds> tangent_sizes_;
  std::array<int, kNumManifolds> ambient_offsets_;
  std::array<int, kNumManifolds> tangent_offsets_;
  int ambient_size_;
  int tangent_size_;
};

#ifdef CERES_HAS_CPP17
// C++17 deduction guide that allows the user to avoid explicitly specifying
// the template parameters of ProductManifold. The class can instead be
// instantiated as follows:
//
//   ProductManifold manifold{QuaternionManifold{}, EuclideanManifold<3>{}};
//
template <typename Manifold0, typename Manifold1, typename... Manifolds>
ProductManifold(Manifold0&&, Manifold1&&, Manifolds&&...)
    -> ProductManifold<Manifold0, Manifold1, Manifolds...>;
#endif

}  // namespace ceres

#endif  // CERES_PUBLIC_PRODUCT_MANIFOLD_H_
