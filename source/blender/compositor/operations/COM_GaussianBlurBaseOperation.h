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
 * Copyright 2021, Blender Foundation.
 */

#pragma once

#include "COM_BlurBaseOperation.h"

namespace blender::compositor {

class GaussianBlurBaseOperation : public BlurBaseOperation {
 protected:
  float *gausstab_;
#ifdef BLI_HAVE_SSE2
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

}  // namespace blender::compositor
