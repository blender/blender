/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CryptomatteOperation.h"

namespace blender::compositor {

CryptomatteOperation::CryptomatteOperation(size_t num_inputs)
{
  inputs.resize(num_inputs);
  for (size_t i = 0; i < num_inputs; i++) {
    this->add_input_socket(DataType::Color);
  }
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void CryptomatteOperation::init_execution()
{
  for (size_t i = 0; i < inputs.size(); i++) {
    inputs[i] = this->get_input_socket_reader(i);
  }
}

void CryptomatteOperation::add_object_index(float object_index)
{
  if (object_index != 0.0f) {
    object_index_.append(object_index);
  }
}

void CryptomatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    zero_v4(it.out);
    for (int i = 0; i < it.get_num_inputs(); i++) {
      const float *input = it.in(i);
      if (i == 0) {
        /* Write the front-most object as false color for picking. */
        it.out[0] = input[0];
        uint32_t m3hash;
        ::memcpy(&m3hash, &input[0], sizeof(uint32_t));
        /* Since the red channel is likely to be out of display range,
         * setting green and blue gives more meaningful images. */
        it.out[1] = (float(m3hash << 8) / float(UINT32_MAX));
        it.out[2] = (float(m3hash << 16) / float(UINT32_MAX));
      }
      for (const float hash : object_index_) {
        if (input[0] == hash) {
          it.out[3] += input[1];
        }
        if (input[2] == hash) {
          it.out[3] += input[3];
        }
      }
    }
  }
}

}  // namespace blender::compositor
