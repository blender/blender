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

#ifndef CERES_PUBLIC_INTERNAL_EULER_ANGLES_H_
#define CERES_PUBLIC_INTERNAL_EULER_ANGLES_H_

#include <type_traits>

namespace ceres {
namespace internal {

// The EulerSystem struct represents an Euler Angle Convention in compile time.
// It acts like a trait structure and is also used as a tag for dispatching
// Euler angle conversion function templates
//
// Internally, it implements the convention laid out in "Euler angle
// conversion", Ken Shoemake, Graphics Gems IV, where a choice of axis for the
// first rotation (out of 3) and 3 binary choices compactly specify all 24
// rotation conventions
//
//  - InnerAxis: Axis for the first rotation. This is specified by struct tags
//  axis::X, axis::Y, and axis::Z
//
//  - Parity: Defines the parity of the axis permutation. The axis sequence has
//  Even parity if the second axis of rotation is 'greater-than' the first axis
//  of rotation according to the order X<Y<Z<X, otherwise it has Odd parity.
//  This is specified by struct tags Even and Odd
//
//  - AngleConvention: Defines whether Proper Euler Angles (originally defined
//  by Euler, which has the last axis repeated, i.e. ZYZ, ZXZ, etc), or
//  Tait-Bryan Angles (introduced by the nautical and aerospace fields, i.e.
//  using ZYX for roll-pitch-yaw) are used. This is specified by struct Tags
//  ProperEuler and TaitBryan.
//
//  - FrameConvention: Defines whether the three rotations are be in a global
//  frame of reference (extrinsic) or in a body centred frame of reference
//  (intrinsic). This is specified by struct tags Extrinsic and Intrinsic

namespace axis {
struct X : std::integral_constant<int, 0> {};
struct Y : std::integral_constant<int, 1> {};
struct Z : std::integral_constant<int, 2> {};
}  // namespace axis

struct Even;
struct Odd;

struct ProperEuler;
struct TaitBryan;

struct Extrinsic;
struct Intrinsic;

template <typename InnerAxisType,
          typename ParityType,
          typename AngleConventionType,
          typename FrameConventionType>
struct EulerSystem {
  static constexpr bool kIsParityOdd = std::is_same_v<ParityType, Odd>;
  static constexpr bool kIsProperEuler =
      std::is_same_v<AngleConventionType, ProperEuler>;
  static constexpr bool kIsIntrinsic =
      std::is_same_v<FrameConventionType, Intrinsic>;

  static constexpr int kAxes[3] = {
      InnerAxisType::value,
      (InnerAxisType::value + 1 + static_cast<int>(kIsParityOdd)) % 3,
      (InnerAxisType::value + 2 - static_cast<int>(kIsParityOdd)) % 3};
};

}  // namespace internal

// Define human readable aliases to the type of the tags
using ExtrinsicXYZ = internal::EulerSystem<internal::axis::X,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicXYX = internal::EulerSystem<internal::axis::X,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
using ExtrinsicXZY = internal::EulerSystem<internal::axis::X,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicXZX = internal::EulerSystem<internal::axis::X,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
using ExtrinsicYZX = internal::EulerSystem<internal::axis::Y,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicYZY = internal::EulerSystem<internal::axis::Y,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
using ExtrinsicYXZ = internal::EulerSystem<internal::axis::Y,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicYXY = internal::EulerSystem<internal::axis::Y,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
using ExtrinsicZXY = internal::EulerSystem<internal::axis::Z,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicZXZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
using ExtrinsicZYX = internal::EulerSystem<internal::axis::Z,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Extrinsic>;
using ExtrinsicZYZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Extrinsic>;
/* Rotating axes */
using IntrinsicZYX = internal::EulerSystem<internal::axis::X,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicXYX = internal::EulerSystem<internal::axis::X,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;
using IntrinsicYZX = internal::EulerSystem<internal::axis::X,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicXZX = internal::EulerSystem<internal::axis::X,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;
using IntrinsicXZY = internal::EulerSystem<internal::axis::Y,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicYZY = internal::EulerSystem<internal::axis::Y,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;
using IntrinsicZXY = internal::EulerSystem<internal::axis::Y,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicYXY = internal::EulerSystem<internal::axis::Y,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;
using IntrinsicYXZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Even,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicZXZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Even,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;
using IntrinsicXYZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Odd,
                                           internal::TaitBryan,
                                           internal::Intrinsic>;
using IntrinsicZYZ = internal::EulerSystem<internal::axis::Z,
                                           internal::Odd,
                                           internal::ProperEuler,
                                           internal::Intrinsic>;

}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_EULER_ANGLES_H_
