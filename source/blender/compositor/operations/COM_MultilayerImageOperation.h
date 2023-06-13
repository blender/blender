/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
