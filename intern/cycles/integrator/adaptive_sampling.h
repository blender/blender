/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

class AdaptiveSampling {
 public:
  AdaptiveSampling();

  /* Align number of samples so that they align with the adaptive filtering.
   *
   * Returns the new value for the `num_samples` so that after rendering so many samples on top
   * of `start_sample` filtering is required.
   *
   * The alignment happens in a way that allows to render as many samples as possible without
   * missing any filtering point. This means that the result is "clamped" by the nearest sample
   * at which filtering is needed. This is part of mechanism which ensures that all devices will
   * perform same exact filtering and adaptive sampling, regardless of their performance.
   *
   * `start_sample` is the 0-based index of sample.
   *
   * NOTE: The start sample is included into the number of samples to render. This means that
   * if the number of samples is 1, then the path tracer will render samples [align_samples],
   * if the number of samples is 2, then the path tracer will render samples [align_samples,
   * align_samples + 1] and so on. */
  int align_samples(int start_sample, int num_samples) const;

  /* Check whether adaptive sampling filter should happen at this sample.
   * Returns false if the adaptive sampling is not use.
   *
   * `sample` is the 0-based index of sample. */
  bool need_filter(int sample) const;

  bool use = false;
  int adaptive_step = 0;
  int min_samples = 0;
  float threshold = 0.0f;
};

CCL_NAMESPACE_END
