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
// Portable typedefs for various fixed-size integers. Uses template
// metaprogramming instead of fragile compiler defines.

#ifndef CERES_INTERNAL_INTEGRAL_TYPES_H_
#define CERES_INTERNAL_INTEGRAL_TYPES_H_

namespace ceres {
namespace internal {

// Compile time ternary on types.
template<bool kCondition, typename kTrueType, typename kFalseType>
struct Ternary {
  typedef kTrueType type;
};
template<typename kTrueType, typename kFalseType>
struct Ternary<false, kTrueType, kFalseType> {
  typedef kFalseType type;
};

#define CERES_INTSIZE(TYPE) \
    typename Ternary<sizeof(TYPE) * 8 == kBits, TYPE,

template<int kBits>
struct Integer {
  typedef
      CERES_INTSIZE(char)
      CERES_INTSIZE(short)
      CERES_INTSIZE(int)
      CERES_INTSIZE(long int)
      CERES_INTSIZE(long long)
      void>::type >::type >::type >::type >::type
      type;
};

template<int kBits>
struct UnsignedInteger {
  typedef
      CERES_INTSIZE(unsigned char)
      CERES_INTSIZE(unsigned short)
      CERES_INTSIZE(unsigned int)
      CERES_INTSIZE(unsigned long int)
      CERES_INTSIZE(unsigned long long)
      void>::type >::type >::type >::type >::type
      type;
};

#undef CERES_INTSIZE

typedef Integer< 8>::type int8;
typedef Integer<16>::type int16;
typedef Integer<32>::type int32;
typedef Integer<64>::type int64;

typedef UnsignedInteger< 8>::type uint8;
typedef UnsignedInteger<16>::type uint16;
typedef UnsignedInteger<32>::type uint32;
typedef UnsignedInteger<64>::type uint64;

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_INTEGRAL_TYPES_H_
