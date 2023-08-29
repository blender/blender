/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender::deg {

/* Utility class which takes care of calculating average of time series, such as FPS counters. */
template<int MaxSamples> class AveragedTimeSampler {
 public:
  AveragedTimeSampler() : num_samples_(0), next_sample_index_(0) {}

  void add_sample(double value)
  {
    samples_[next_sample_index_] = value;

    /* Move to the next index, keeping wrapping at the end of array into account. */
    ++next_sample_index_;
    if (next_sample_index_ == MaxSamples) {
      next_sample_index_ = 0;
    }

    /* Update number of stored samples. */
    if (num_samples_ != MaxSamples) {
      ++num_samples_;
    }
  }

  double get_averaged() const
  {
    double sum = 0.0;
    for (int i = 0; i < num_samples_; ++i) {
      sum += samples_[i];
    }
    return sum / num_samples_;
  }

 protected:
  double samples_[MaxSamples];

  /* Number of samples which are actually stored in the array. */
  int num_samples_;

  /* Index in the samples_ array under which next sample will be stored. */
  int next_sample_index_;
};

}  // namespace blender::deg
