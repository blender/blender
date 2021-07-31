// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_CASTS_H_
#define CERES_INTERNAL_CASTS_H_

#include <cassert>
#include <cstddef>  // For NULL.

namespace ceres {

// Identity metafunction.
template <class T>
struct identity_ {
  typedef T type;
};

// Use implicit_cast as a safe version of static_cast or const_cast
// for implicit conversions. For example:
// - Upcasting in a type hierarchy.
// - Performing arithmetic conversions (int32 to int64, int to double, etc.).
// - Adding const or volatile qualifiers.
//
// In general, implicit_cast can be used to convert this code
//   To to = from;
//   DoSomething(to);
// to this
//   DoSomething(implicit_cast<To>(from));
//
// base::identity_ is used to make a non-deduced context, which
// forces all callers to explicitly specify the template argument.
template<typename To>
inline To implicit_cast(typename identity_<To>::type to) {
  return to;
}

// This version of implicit_cast is used when two template arguments
// are specified. It's obsolete and should not be used.
template<typename To, typename From>
inline To implicit_cast(typename identity_<From>::type const &f) {
  return f;
}

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.

template<typename To, typename From>     // use like this: down_cast<T*>(foo);
inline To down_cast(From* f) {                   // so we only accept pointers
  // Ensures that To is a sub-type of From *.  This test is here only
  // for compile-time type checking, and has no overhead in an
  // optimized build at run-time, as it will be optimized away
  // completely.

  // TODO(csilvers): This should use COMPILE_ASSERT.
  if (false) {
    implicit_cast<From*, To>(NULL);
  }

  // uses RTTI in dbg and fastbuild. asserts are disabled in opt builds.
  assert(f == NULL || dynamic_cast<To>(f) != NULL);  // NOLINT
  return static_cast<To>(f);
}

}  // namespace ceres

#endif  // CERES_INTERNAL_CASTS_H_
