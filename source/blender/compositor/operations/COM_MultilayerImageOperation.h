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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_ImageOperation.h"

namespace blender::compositor {

class MultilayerBaseOperation : public BaseImageOperation {
 private:
  int pass_id_;
  int view_;

 protected:
  RenderLayer *render_layer_;
  RenderPass *render_pass_;
  ImBuf *get_im_buf() override;

 public:
  /**
   * Constructor
   */
  MultilayerBaseOperation(RenderLayer *render_layer, RenderPass *render_pass, int view);

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class MultilayerColorOperation : public MultilayerBaseOperation {
 public:
  MultilayerColorOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->add_output_socket(DataType::Color);
  }
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  std::unique_ptr<MetaData> get_meta_data() override;
};

class MultilayerValueOperation : public MultilayerBaseOperation {
 public:
  MultilayerValueOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->add_output_socket(DataType::Value);
  }
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
 public:
  MultilayerVectorOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->add_output_socket(DataType::Vector);
  }
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};

}  // namespace blender::compositor
