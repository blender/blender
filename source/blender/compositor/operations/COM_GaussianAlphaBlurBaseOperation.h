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

class GaussianAlphaBlurBaseOperation : public BlurBaseOperation {
 protected:
  float *gausstab_;
  float *distbuf_inv_;
  int falloff_; /* Falloff for #distbuf_inv. */
  bool do_subtract_;
  int filtersize_;
  float rad_;
  eDimension dimension_;

 public:
  GaussianAlphaBlurBaseOperation(eDimension dim);

  virtual void init_data() override;
  virtual void init_execution() override;
  virtual void deinit_execution() override;

  void get_area_of_interest(const int input_idx,
                            const rcti &output_area,
                            rcti &r_input_area) final;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) final;

  /**
   * Set subtract for Dilate/Erode functionality
   */
  void set_subtract(bool subtract)
  {
    do_subtract_ = subtract;
  }
  void set_falloff(int falloff)
  {
    falloff_ = falloff;
  }
};

}  // namespace blender::compositor
