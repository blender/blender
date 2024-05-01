/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

bool COM_is_denoise_supported();

class DenoiseBaseOperation : public NodeOperation {
 protected:
  bool output_rendered_;

 protected:
  DenoiseBaseOperation();

 public:
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

class DenoiseOperation : public DenoiseBaseOperation {
 private:
  /**
   * \brief settings of the denoise node.
   */
  const NodeDenoise *settings_;

 public:
  DenoiseOperation();

  void set_denoise_settings(const NodeDenoise *settings)
  {
    settings_ = settings;
  }

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 protected:
  void hash_output_params() override;
  void generate_denoise(MemoryBuffer *output,
                        MemoryBuffer *input_color,
                        MemoryBuffer *input_normal,
                        MemoryBuffer *input_albedo,
                        const NodeDenoise *settings);
};

class DenoisePrefilterOperation : public DenoiseBaseOperation {
 private:
  std::string image_name_;

 public:
  DenoisePrefilterOperation(DataType data_type);

  void set_image_name(StringRef name)
  {
    image_name_ = name;
  }

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 protected:
  void hash_output_params() override;

 private:
  void generate_denoise(MemoryBuffer *output, MemoryBuffer *input);
};

}  // namespace blender::compositor
