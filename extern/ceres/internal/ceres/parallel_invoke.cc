// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
// Author: vitus@google.com (Michael Vitus)

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <tuple>

#include "ceres/internal/config.h"
#include "ceres/parallel_for.h"
#include "ceres/parallel_vector_ops.h"
#include "glog/logging.h"

namespace ceres::internal {

BlockUntilFinished::BlockUntilFinished(int num_total_jobs)
    : num_total_jobs_finished_(0), num_total_jobs_(num_total_jobs) {}

void BlockUntilFinished::Finished(int num_jobs_finished) {
  if (num_jobs_finished == 0) return;
  std::lock_guard<std::mutex> lock(mutex_);
  num_total_jobs_finished_ += num_jobs_finished;
  CHECK_LE(num_total_jobs_finished_, num_total_jobs_);
  if (num_total_jobs_finished_ == num_total_jobs_) {
    condition_.notify_one();
  }
}

void BlockUntilFinished::Block() {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_.wait(
      lock, [this]() { return num_total_jobs_finished_ == num_total_jobs_; });
}

ParallelInvokeState::ParallelInvokeState(int start,
                                         int end,
                                         int num_work_blocks)
    : start(start),
      end(end),
      num_work_blocks(num_work_blocks),
      base_block_size((end - start) / num_work_blocks),
      num_base_p1_sized_blocks((end - start) % num_work_blocks),
      block_id(0),
      thread_id(0),
      block_until_finished(num_work_blocks) {}

}  // namespace ceres::internal
