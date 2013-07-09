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
// Author: kenton@google.com (Kenton Varda)
//
// ManualConstructor statically-allocates space in which to store some
// object, but does not initialize it.  You can then call the constructor
// and destructor for the object yourself as you see fit.  This is useful
// for memory management optimizations, where you want to initialize and
// destroy an object multiple times but only allocate it once.
//
// (When I say ManualConstructor statically allocates space, I mean that
// the ManualConstructor object itself is forced to be the right size.)

#ifndef CERES_PUBLIC_INTERNAL_MANUAL_CONSTRUCTOR_H_
#define CERES_PUBLIC_INTERNAL_MANUAL_CONSTRUCTOR_H_

#include <new>

namespace ceres {
namespace internal {

// ------- Define CERES_ALIGNED_CHAR_ARRAY --------------------------------

#ifndef CERES_ALIGNED_CHAR_ARRAY

// Because MSVC and older GCCs require that the argument to their alignment
// construct to be a literal constant integer, we use a template instantiated
// at all the possible powers of two.
template<int alignment, int size> struct AlignType { };
template<int size> struct AlignType<0, size> { typedef char result[size]; };

#if !defined(CERES_ALIGN_ATTRIBUTE)
#define CERES_ALIGNED_CHAR_ARRAY you_must_define_CERES_ALIGNED_CHAR_ARRAY_for_your_compiler
#else  // !defined(CERES_ALIGN_ATTRIBUTE)

#define CERES_ALIGN_TYPE_TEMPLATE(X) \
  template<int size> struct AlignType<X, size> { \
    typedef CERES_ALIGN_ATTRIBUTE(X) char result[size]; \
  }

CERES_ALIGN_TYPE_TEMPLATE(1);
CERES_ALIGN_TYPE_TEMPLATE(2);
CERES_ALIGN_TYPE_TEMPLATE(4);
CERES_ALIGN_TYPE_TEMPLATE(8);
CERES_ALIGN_TYPE_TEMPLATE(16);
CERES_ALIGN_TYPE_TEMPLATE(32);
CERES_ALIGN_TYPE_TEMPLATE(64);
CERES_ALIGN_TYPE_TEMPLATE(128);
CERES_ALIGN_TYPE_TEMPLATE(256);
CERES_ALIGN_TYPE_TEMPLATE(512);
CERES_ALIGN_TYPE_TEMPLATE(1024);
CERES_ALIGN_TYPE_TEMPLATE(2048);
CERES_ALIGN_TYPE_TEMPLATE(4096);
CERES_ALIGN_TYPE_TEMPLATE(8192);
// Any larger and MSVC++ will complain.

#undef CERES_ALIGN_TYPE_TEMPLATE

#define CERES_ALIGNED_CHAR_ARRAY(T, Size) \
  typename AlignType<CERES_ALIGN_OF(T), sizeof(T) * Size>::result

#endif  // !defined(CERES_ALIGN_ATTRIBUTE)

#endif  // CERES_ALIGNED_CHAR_ARRAY

template <typename Type>
class ManualConstructor {
 public:
  // No constructor or destructor because one of the most useful uses of
  // this class is as part of a union, and members of a union cannot have
  // constructors or destructors.  And, anyway, the whole point of this
  // class is to bypass these.

  inline Type* get() {
    return reinterpret_cast<Type*>(space_);
  }
  inline const Type* get() const  {
    return reinterpret_cast<const Type*>(space_);
  }

  inline Type* operator->() { return get(); }
  inline const Type* operator->() const { return get(); }

  inline Type& operator*() { return *get(); }
  inline const Type& operator*() const { return *get(); }

  // This is needed to get around the strict aliasing warning GCC generates.
  inline void* space() {
    return reinterpret_cast<void*>(space_);
  }

  // You can pass up to four constructor arguments as arguments of Init().
  inline void Init() {
    new(space()) Type;
  }

  template <typename T1>
  inline void Init(const T1& p1) {
    new(space()) Type(p1);
  }

  template <typename T1, typename T2>
  inline void Init(const T1& p1, const T2& p2) {
    new(space()) Type(p1, p2);
  }

  template <typename T1, typename T2, typename T3>
  inline void Init(const T1& p1, const T2& p2, const T3& p3) {
    new(space()) Type(p1, p2, p3);
  }

  template <typename T1, typename T2, typename T3, typename T4>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4) {
    new(space()) Type(p1, p2, p3, p4);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5) {
    new(space()) Type(p1, p2, p3, p4, p5);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6) {
    new(space()) Type(p1, p2, p3, p4, p5, p6);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6, typename T7>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6, const T7& p7) {
    new(space()) Type(p1, p2, p3, p4, p5, p6, p7);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6, typename T7, typename T8>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6, const T7& p7, const T8& p8) {
    new(space()) Type(p1, p2, p3, p4, p5, p6, p7, p8);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6, typename T7, typename T8, typename T9>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6, const T7& p7, const T8& p8,
                   const T9& p9) {
    new(space()) Type(p1, p2, p3, p4, p5, p6, p7, p8, p9);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6, typename T7, typename T8, typename T9, typename T10>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6, const T7& p7, const T8& p8,
                   const T9& p9, const T10& p10) {
    new(space()) Type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
            typename T6, typename T7, typename T8, typename T9, typename T10,
            typename T11>
  inline void Init(const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const T5& p5, const T6& p6, const T7& p7, const T8& p8,
                   const T9& p9, const T10& p10, const T11& p11) {
    new(space()) Type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
  }

  inline void Destroy() {
    get()->~Type();
  }

 private:
  CERES_ALIGNED_CHAR_ARRAY(Type, 1) space_;
};

#undef CERES_ALIGNED_CHAR_ARRAY

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_MANUAL_CONSTRUCTOR_H_
