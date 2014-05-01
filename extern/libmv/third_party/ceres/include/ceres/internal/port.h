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

#ifndef CERES_PUBLIC_INTERNAL_PORT_H_
#define CERES_PUBLIC_INTERNAL_PORT_H_

// This file needs to compile as c code.
#ifdef __cplusplus

#include <string>

#include "ceres/internal/config.h"

#if defined(CERES_TR1_MEMORY_HEADER)
#include <tr1/memory>
#else
#include <memory>
#endif

namespace ceres {

// It is unfortunate that this import of the entire standard namespace is
// necessary. The reasons are historical and won't be explained here, but
// suffice to say it is not a mistake and can't be removed without breaking
// things outside of the Ceres optimization package.
using namespace std;

// This is necessary to properly handle the case that there is a different
// "string" implementation in the global namespace.
using std::string;

#if defined(CERES_TR1_SHARED_PTR)
using std::tr1::shared_ptr;
#else
using std::shared_ptr;
#endif

}  // namespace ceres

#endif  // __cplusplus

// A macro to signal which functions and classes are exported when
// building a DLL with MSVC.
//
// Note that the ordering here is important, CERES_BUILDING_SHARED_LIBRARY
// is only defined locally when Ceres is compiled, it is never exported to
// users.  However, in order that we do not have to configure config.h
// separately for building vs installing, if we are using MSVC and building
// a shared library, then both CERES_BUILDING_SHARED_LIBRARY and
// CERES_USING_SHARED_LIBRARY will be defined when Ceres is compiled.
// Hence it is important that the check for CERES_BUILDING_SHARED_LIBRARY
// happens first.
#if defined(_MSC_VER) && defined(CERES_BUILDING_SHARED_LIBRARY)
# define CERES_EXPORT __declspec(dllexport)
#elif defined(_MSC_VER) && defined(CERES_USING_SHARED_LIBRARY)
# define CERES_EXPORT __declspec(dllimport)
#else
# define CERES_EXPORT
#endif

#endif  // CERES_PUBLIC_INTERNAL_PORT_H_
