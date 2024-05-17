/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ImageOperation.h"

namespace blender::compositor {

class MultilayerBaseOperation : public BaseImageOperation {
 private:
  int pass_id_;

 protected:
  RenderLayer *render_layer_;
  RenderPass *render_pass_;

  /* Returns the image view to use for the current active view. */
  int get_view_index() const;

  ImBuf *get_im_buf() override;

 public:
  /**
   * Constructor
   */
  MultilayerBaseOperation(RenderLayer *render_layer, RenderPass *render_pass);

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class MultilayerColorOperation : public MultilayerBaseOperation {
 public:
  MultilayerColorOperation(RenderLayer *render_layer, RenderPass *render_pass)
      : MultilayerBaseOperation(render_layer, render_pass)
  {
    this->add_output_socket(DataType::Color);
  }
  std::unique_ptr<MetaData> get_meta_data() override;
};

class MultilayerValueOperation : public MultilayerBaseOperation {
 public:
  MultilayerValueOperation(RenderLayer *render_layer, RenderPass *render_pass)
      : MultilayerBaseOperation(render_layer, render_pass)
  {
    this->add_output_socket(DataType::Value);
  }
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
 public:
  MultilayerVectorOperation(RenderLayer *render_layer, RenderPass *render_pass)
      : MultilayerBaseOperation(render_layer, render_pass)
  {
    this->add_output_socket(DataType::Vector);
  }
};

}  // namespace blender::compositor
