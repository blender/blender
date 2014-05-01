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
// This is a forwarding header containing the public symbols exported from
// Ceres. Anything in the "ceres" namespace is available for use.

#ifndef CERES_PUBLIC_CERES_H_
#define CERES_PUBLIC_CERES_H_

#define CERES_VERSION 1.9.0
#define CERES_ABI_VERSION 1.9.0

#include "ceres/autodiff_cost_function.h"
#include "ceres/autodiff_local_parameterization.h"
#include "ceres/cost_function.h"
#include "ceres/cost_function_to_functor.h"
#include "ceres/covariance.h"
#include "ceres/crs_matrix.h"
#include "ceres/dynamic_autodiff_cost_function.h"
#include "ceres/dynamic_numeric_diff_cost_function.h"
#include "ceres/iteration_callback.h"
#include "ceres/jet.h"
#include "ceres/local_parameterization.h"
#include "ceres/loss_function.h"
#include "ceres/numeric_diff_cost_function.h"
#include "ceres/numeric_diff_functor.h"
#include "ceres/ordered_groups.h"
#include "ceres/problem.h"
#include "ceres/sized_cost_function.h"
#include "ceres/solver.h"
#include "ceres/types.h"

#endif  // CERES_PUBLIC_CERES_H_
