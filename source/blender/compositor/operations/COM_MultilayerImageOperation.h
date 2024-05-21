/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "COM_ImageOperation.h"

namespace blender::compositor {

class MultilayerBaseOperation : public BaseImageOperation {
 protected:
  /* NOTE: The layer name is only used for meta-data. The image user's layer index defines which
   * layer will be actually accessed for the image buffer. */
  std::string layer_name_;
  std::string pass_name_;

  ImBuf *get_im_buf() override;

 public:
  MultilayerBaseOperation() = default;

  void set_layer_name(std::string layer_name)
  {
    layer_name_ = std::move(layer_name);
  }
  void set_pass_name(std::string pass_name)
  {
    pass_name_ = std::move(pass_name);
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class MultilayerColorOperation : public MultilayerBaseOperation {
 public:
  MultilayerColorOperation()
  {
    this->add_output_socket(DataType::Color);
  }
  std::unique_ptr<MetaData> get_meta_data() override;
};

class MultilayerValueOperation : public MultilayerBaseOperation {
 public:
  MultilayerValueOperation()
  {
    this->add_output_socket(DataType::Value);
  }
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
 public:
  MultilayerVectorOperation()
  {
    this->add_output_socket(DataType::Vector);
  }
};

}  // namespace blender::compositor
