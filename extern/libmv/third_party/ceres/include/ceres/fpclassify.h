// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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
// Portable floating point classification. The names are picked such that they
// do not collide with macros. For example, "isnan" in C99 is a macro and hence
// does not respect namespaces.
//
// TODO(keir): Finish porting!

#ifndef CERES_PUBLIC_FPCLASSIFY_H_
#define CERES_PUBLIC_FPCLASSIFY_H_

#if defined(_MSC_VER)
#include <float.h>
#endif

#include <limits>

namespace ceres {

#if defined(_MSC_VER)

inline bool IsFinite  (double x) { return _finite(x) != 0;                   }
inline bool IsInfinite(double x) { return _finite(x) == 0 && _isnan(x) == 0; }
inline bool IsNaN     (double x) { return _isnan(x) != 0;                    }
inline bool IsNormal  (double x) {
  int classification = _fpclass(x);
  return classification == _FPCLASS_NN ||
         classification == _FPCLASS_PN;
}

#elif defined(ANDROID) && defined(_STLPORT_VERSION)

// On Android, when using the STLPort, the C++ isnan and isnormal functions
// are defined as macros.
inline bool IsNaN     (double x) { return isnan(x);    }
inline bool IsNormal  (double x) { return isnormal(x); }
// On Android NDK r6, when using STLPort, the isinf and isfinite functions are
// not available, so reimplement them.
inline bool IsInfinite(double x) {
  return x ==  std::numeric_limits<double>::infinity() ||
         x == -std::numeric_limits<double>::infinity();
}
inline bool IsFinite(double x) {
  return !isnan(x) && !IsInfinite(x);
}

# else

// These definitions are for the normal Unix suspects.
inline bool IsFinite  (double x) { return std::isfinite(x); }
inline bool IsInfinite(double x) { return std::isinf(x);    }
inline bool IsNaN     (double x) { return std::isnan(x);    }
inline bool IsNormal  (double x) { return std::isnormal(x); }

#endif

}  // namespace ceres

#endif  // CERES_PUBLIC_FPCLASSIFY_H_
