/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_BlurBaseOperation.h"

namespace blender::compositor {

class GaussianBlurBaseOperation : public BlurBaseOperation {
 protected:
  float *gausstab_;
#if BLI_HAVE_SSE2
  __m128 *gausstab_sse_;
#endif
  int filtersize_;
  float rad_;
  eDimension dimension_;

 public:
  GaussianBlurBaseOperation(eDimension dim);

  virtual void init_data() override;
  virtual void init_execution() override;
  virtual void deinit_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class GaussianXBlurOperation : public GaussianBlurBaseOperation {
 public:
  GaussianXBlurOperation() : GaussianBlurBaseOperation(eDimension::X) {}
};

class GaussianYBlurOperation : public GaussianBlurBaseOperation {
 public:
  GaussianYBlurOperation() : GaussianBlurBaseOperation(eDimension::Y) {}
};

}  // namespace blender::compositor
