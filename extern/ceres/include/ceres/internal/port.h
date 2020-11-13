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

#ifndef CERES_PUBLIC_INTERNAL_PORT_H_
#define CERES_PUBLIC_INTERNAL_PORT_H_

// This file needs to compile as c code.
#include "ceres/internal/config.h"

#if defined(CERES_USE_OPENMP)
#if defined(CERES_USE_CXX_THREADS) || defined(CERES_NO_THREADS)
#error CERES_USE_OPENMP is mutually exclusive to CERES_USE_CXX_THREADS and CERES_NO_THREADS
#endif
#elif defined(CERES_USE_CXX_THREADS)
#if defined(CERES_USE_OPENMP) || defined(CERES_NO_THREADS)
#error CERES_USE_CXX_THREADS is mutually exclusive to CERES_USE_OPENMP, CERES_USE_CXX_THREADS and CERES_NO_THREADS
#endif
#elif defined(CERES_NO_THREADS)
#if defined(CERES_USE_OPENMP) || defined(CERES_USE_CXX_THREADS)
#error CERES_NO_THREADS is mutually exclusive to CERES_USE_OPENMP and CERES_USE_CXX_THREADS
#endif
#else
#  error One of CERES_USE_OPENMP, CERES_USE_CXX_THREADS or CERES_NO_THREADS must be defined.
#endif

// CERES_NO_SPARSE should be automatically defined by config.h if Ceres was
// compiled without any sparse back-end.  Verify that it has not subsequently
// been inconsistently redefined.
#if defined(CERES_NO_SPARSE)
#if !defined(CERES_NO_SUITESPARSE)
#error CERES_NO_SPARSE requires CERES_NO_SUITESPARSE.
#endif
#if !defined(CERES_NO_CXSPARSE)
#error CERES_NO_SPARSE requires CERES_NO_CXSPARSE
#endif
#if !defined(CERES_NO_ACCELERATE_SPARSE)
#error CERES_NO_SPARSE requires CERES_NO_ACCELERATE_SPARSE
#endif
#if defined(CERES_USE_EIGEN_SPARSE)
#error CERES_NO_SPARSE requires !CERES_USE_EIGEN_SPARSE
#endif
#endif

// A macro to signal which functions and classes are exported when
// building a shared library.
#if defined(_MSC_VER)
#define CERES_API_SHARED_IMPORT __declspec(dllimport)
#define CERES_API_SHARED_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define CERES_API_SHARED_IMPORT __attribute__((visibility("default")))
#define CERES_API_SHARED_EXPORT __attribute__((visibility("default")))
#else
#define CERES_API_SHARED_IMPORT
#define CERES_API_SHARED_EXPORT
#endif

// CERES_BUILDING_SHARED_LIBRARY is only defined locally when Ceres itself is
// compiled as a shared library, it is never exported to users.  In order that
// we do not have to configure config.h separately when building Ceres as either
// a static or dynamic library, we define both CERES_USING_SHARED_LIBRARY and
// CERES_BUILDING_SHARED_LIBRARY when building as a shared library.
#if defined(CERES_USING_SHARED_LIBRARY)
#if defined(CERES_BUILDING_SHARED_LIBRARY)
// Compiling Ceres itself as a shared library.
#define CERES_EXPORT CERES_API_SHARED_EXPORT
#else
// Using Ceres as a shared library.
#define CERES_EXPORT CERES_API_SHARED_IMPORT
#endif
#else
// Ceres was compiled as a static library, export everything.
#define CERES_EXPORT
#endif

// Unit tests reach in and test internal functionality so we need a way to make
// those symbols visible
#ifdef CERES_EXPORT_INTERNAL_SYMBOLS
#define CERES_EXPORT_INTERNAL CERES_EXPORT
#else
#define CERES_EXPORT_INTERNAL
#endif

#endif  // CERES_PUBLIC_INTERNAL_PORT_H_
