/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/testing.h"

#include "integrator/adaptive_sampling.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

TEST(AdaptiveSampling, schedule_samples)
{
  AdaptiveSampling adaptive_sampling;
  adaptive_sampling.use = true;
  adaptive_sampling.min_samples = 0;
  adaptive_sampling.adaptive_step = 4;

  for (int sample = 2; sample < 32; ++sample) {
    for (int num_samples = 8; num_samples < 32; ++num_samples) {
      const int num_samples_aligned = adaptive_sampling.align_samples(sample, num_samples);
      /* NOTE: `sample + num_samples_aligned` is the number of samples after rendering, so need
       * to convert this to the 0-based index of the last sample. */
      EXPECT_TRUE(adaptive_sampling.need_filter(sample + num_samples_aligned - 1));
    }
  }
}

TEST(AdaptiveSampling, align_samples)
{
  AdaptiveSampling adaptive_sampling;
  adaptive_sampling.use = true;
  adaptive_sampling.min_samples = 11 /* rounded of sqrt(128) */;
  adaptive_sampling.adaptive_step = 4;

  /* Filtering will happen at the following samples:
   * 15, 19, 23, 27, 31, 35, 39, 43 */

  /* Requested sample and number of samples will result in number of samples lower than
   * `min_samples`. */
  EXPECT_EQ(adaptive_sampling.align_samples(0, 4), 4);
  EXPECT_EQ(adaptive_sampling.align_samples(0, 7), 7);

  /* Request number of samples higher than the minimum samples before filter, but prior to the
   * first sample at which filtering will happen. */
  EXPECT_EQ(adaptive_sampling.align_samples(0, 15), 15);

  /* When rendering many samples from the very beginning, limit number of samples by the first
   * sample at which filtering is to happen. */
  EXPECT_EQ(adaptive_sampling.align_samples(0, 16), 16);
  EXPECT_EQ(adaptive_sampling.align_samples(0, 17), 16);
  EXPECT_EQ(adaptive_sampling.align_samples(0, 20), 16);
  EXPECT_EQ(adaptive_sampling.align_samples(0, 60), 16);

  /* Similar to above, but start sample is not 0. */
  EXPECT_EQ(adaptive_sampling.align_samples(9, 8), 7);
  EXPECT_EQ(adaptive_sampling.align_samples(9, 20), 7);
  EXPECT_EQ(adaptive_sampling.align_samples(9, 60), 7);

  /* Start sample is past the minimum required samples, but prior to the first filter sample. */
  EXPECT_EQ(adaptive_sampling.align_samples(12, 6), 4);
  EXPECT_EQ(adaptive_sampling.align_samples(12, 20), 4);
  EXPECT_EQ(adaptive_sampling.align_samples(12, 60), 4);

  /* Start sample is the sample which is to be filtered. */
  EXPECT_EQ(adaptive_sampling.align_samples(15, 4), 1);
  EXPECT_EQ(adaptive_sampling.align_samples(15, 6), 1);
  EXPECT_EQ(adaptive_sampling.align_samples(15, 10), 1);
  EXPECT_EQ(adaptive_sampling.align_samples(58, 2), 2);

  /* Start sample is past the sample which is to be filtered. */
  EXPECT_EQ(adaptive_sampling.align_samples(16, 3), 3);
  EXPECT_EQ(adaptive_sampling.align_samples(16, 4), 4);
  EXPECT_EQ(adaptive_sampling.align_samples(16, 5), 4);
  EXPECT_EQ(adaptive_sampling.align_samples(16, 10), 4);

  /* Should never exceed requested number of samples. */
  EXPECT_EQ(adaptive_sampling.align_samples(15, 2), 1);
  EXPECT_EQ(adaptive_sampling.align_samples(16, 2), 2);
  EXPECT_EQ(adaptive_sampling.align_samples(17, 2), 2);
  EXPECT_EQ(adaptive_sampling.align_samples(18, 2), 2);
}

TEST(AdaptiveSampling, need_filter)
{
  AdaptiveSampling adaptive_sampling;
  adaptive_sampling.use = true;
  adaptive_sampling.min_samples = 11 /* rounded of sqrt(128) */;
  adaptive_sampling.adaptive_step = 4;

  const vector<int> expected_samples_to_filter = {
      {15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59}};

  vector<int> actual_samples_to_filter;
  for (int sample = 0; sample < 60; ++sample) {
    if (adaptive_sampling.need_filter(sample)) {
      actual_samples_to_filter.push_back(sample);
    }
  }

  EXPECT_EQ(actual_samples_to_filter, expected_samples_to_filter);
}

CCL_NAMESPACE_END
