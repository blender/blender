// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_EXECUTION_SUMMARY_H_
#define CERES_INTERNAL_EXECUTION_SUMMARY_H_

#include <map>
#include <string>

#include "ceres/internal/port.h"
#include "ceres/wall_time.h"
#include "ceres/mutex.h"

namespace ceres {
namespace internal {

// Struct used by various objects to report statistics and other
// information about their execution. e.g., ExecutionSummary::times
// can be used for reporting times associated with various activities.
class ExecutionSummary {
 public:
  void IncrementTimeBy(const string& name, const double value) {
    CeresMutexLock l(&times_mutex_);
    times_[name] += value;
  }

  void IncrementCall(const string& name) {
    CeresMutexLock l(&calls_mutex_);
    calls_[name] += 1;
  }

  const map<string, double>& times() const { return times_; }
  const map<string, int>& calls() const { return calls_; }

 private:
  Mutex times_mutex_;
  map<string, double> times_;

  Mutex calls_mutex_;
  map<string, int> calls_;
};

class ScopedExecutionTimer {
 public:
  ScopedExecutionTimer(const string& name, ExecutionSummary* summary)
      : start_time_(WallTimeInSeconds()),
        name_(name),
        summary_(summary) {}

  ~ScopedExecutionTimer() {
    summary_->IncrementTimeBy(name_, WallTimeInSeconds() - start_time_);
  }

 private:
  const double start_time_;
  const string name_;
  ExecutionSummary* summary_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_EXECUTION_SUMMARY_H_
