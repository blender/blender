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

#include "integrator/adaptive_sampling.h"

#include "util/math.h"

CCL_NAMESPACE_BEGIN

AdaptiveSampling::AdaptiveSampling()
{
}

int AdaptiveSampling::align_samples(int start_sample, int num_samples) const
{
  if (!use) {
    return num_samples;
  }

  /*
   * The naive implementation goes as following:
   *
   *   int count = 1;
   *   while (!need_filter(start_sample + count - 1) && count < num_samples) {
   *     ++count;
   *   }
   *   return count;
   */

  /* 0-based sample index at which first filtering will happen. */
  const int first_filter_sample = (min_samples + 1) | (adaptive_step - 1);

  /* Allow as many samples as possible until the first filter sample. */
  if (start_sample + num_samples <= first_filter_sample) {
    return num_samples;
  }

  const int next_filter_sample = max(first_filter_sample, start_sample | (adaptive_step - 1));

  const int num_samples_until_filter = next_filter_sample - start_sample + 1;

  return min(num_samples_until_filter, num_samples);
}

bool AdaptiveSampling::need_filter(int sample) const
{
  if (!use) {
    return false;
  }

  if (sample <= min_samples) {
    return false;
  }

  return (sample & (adaptive_step - 1)) == (adaptive_step - 1);
}

CCL_NAMESPACE_END
