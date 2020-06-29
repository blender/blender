/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender {
namespace deg {

// Utility class which takes care of calculating average of time series, such as
// FPS counters.
template<int MaxSamples> class AveragedTimeSampler {
 public:
  AveragedTimeSampler() : num_samples_(0), next_sample_index_(0)
  {
  }

  void add_sample(double value)
  {
    samples_[next_sample_index_] = value;

    // Move to the next index, keeping wrapping at the end of array into account.
    ++next_sample_index_;
    if (next_sample_index_ == MaxSamples) {
      next_sample_index_ = 0;
    }

    // Update number of stored samples.
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

  // Number of samples which are actually stored in the array.
  int num_samples_;

  // Index in the samples_ array under which next sample will be stored.
  int next_sample_index_;
};

}  // namespace deg
}  // namespace blender
