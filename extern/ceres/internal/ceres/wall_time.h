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
// Author: strandmark@google.com (Petter Strandmark)

#ifndef CERES_INTERNAL_WALL_TIME_H_
#define CERES_INTERNAL_WALL_TIME_H_

#include <map>
#include <string>
#include "ceres/internal/port.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Returns time, in seconds, from some arbitrary starting point. If
// OpenMP is available then the high precision openmp_get_wtime()
// function is used. Otherwise on unixes, gettimeofday is used. The
// granularity is in seconds on windows systems.
double WallTimeInSeconds();

// Log a series of events, recording for each event the time elapsed
// since the last event and since the creation of the object.
//
// The information is output to VLOG(3) upon destruction. A
// name::Total event is added as the final event right before
// destruction.
//
// Example usage:
//
//  void Foo() {
//    EventLogger event_logger("Foo");
//    Bar1();
//    event_logger.AddEvent("Bar1")
//    Bar2();
//    event_logger.AddEvent("Bar2")
//    Bar3();
//  }
//
// Will produce output that looks like
//
//  Foo
//      Bar1:  time1  time1
//      Bar2:  time2  time1 + time2;
//     Total:  time3  time1 + time2 + time3;
class EventLogger {
 public:
  explicit EventLogger(const std::string& logger_name);
  ~EventLogger();
  void AddEvent(const std::string& event_name);

 private:
  double start_time_;
  double last_event_time_;
  std::string events_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_WALL_TIME_H_
