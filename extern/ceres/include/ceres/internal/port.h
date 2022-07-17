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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_PUBLIC_INTERNAL_PORT_H_
#define CERES_PUBLIC_INTERNAL_PORT_H_

// A macro to mark a function/variable/class as deprecated.
// We use compiler specific attributes rather than the c++
// attribute because they do not mix well with each other.
#if defined(_MSC_VER)
#define CERES_DEPRECATED_WITH_MSG(message) __declspec(deprecated(message))
#elif defined(__GNUC__)
#define CERES_DEPRECATED_WITH_MSG(message) __attribute__((deprecated(message)))
#else
// In the worst case fall back to c++ attribute.
#define CERES_DEPRECATED_WITH_MSG(message) [[deprecated(message)]]
#endif

#ifndef CERES_GET_FLAG
#define CERES_GET_FLAG(X) X
#endif

// Indicates whether C++17 is currently active
#ifndef CERES_HAS_CPP17
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define CERES_HAS_CPP17
#endif  // __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >=
        // 201703L)
#endif  // !defined(CERES_HAS_CPP17)

// Indicates whether C++20 is currently active
#ifndef CERES_HAS_CPP20
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define CERES_HAS_CPP20
#endif  // __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >=
        // 202002L)
#endif  // !defined(CERES_HAS_CPP20)

// Prevents symbols from being substituted by the corresponding macro definition
// under the same name. For instance, min and max are defined as macros on
// Windows (unless NOMINMAX is defined) which causes compilation errors when
// defining or referencing symbols under the same name.
//
// To be robust in all cases particularly when NOMINMAX cannot be used, use this
// macro to annotate min/max declarations/definitions. Examples:
//
//   int max CERES_PREVENT_MACRO_SUBSTITUTION();
//   min CERES_PREVENT_MACRO_SUBSTITUTION(a, b);
//   max CERES_PREVENT_MACRO_SUBSTITUTION(a, b);
//
// NOTE: In case the symbols for which the substitution must be prevented are
// used within another macro, the substitution must be inhibited using parens as
//
//   (std::numerical_limits<double>::max)()
//
// since the helper macro will not work here. Do not use this technique in
// general case, because it will prevent argument-dependent lookup (ADL).
//
#define CERES_PREVENT_MACRO_SUBSTITUTION  // Yes, it's empty

#endif  // CERES_PUBLIC_INTERNAL_PORT_H_
