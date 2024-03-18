/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KuwaharaAnisotropicStructureTensorOperation.h"

#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

namespace blender::compositor {

KuwaharaAnisotropicStructureTensorOperation::KuwaharaAnisotropicStructureTensorOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->flags_.is_fullframe_operation = true;
  this->flags_.can_be_constant = true;
}

void KuwaharaAnisotropicStructureTensorOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
}

void KuwaharaAnisotropicStructureTensorOperation::deinit_execution()
{
  image_reader_ = nullptr;
}

/* Computes the structure tensor of the image using a Dirac delta window function as described in
 * section "3.2 Local Structure Estimation" of the paper:
 *
 *   Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
 *   filtering." 2011.
 *
 * The structure tensor should then be smoothed using a Gaussian function to eliminate high
 * frequency details. */
void KuwaharaAnisotropicStructureTensorOperation::execute_pixel_sampled(float output[4],
                                                                        float x_float,
                                                                        float y_float,
                                                                        PixelSampler /*sampler*/)
{
  using math::max, math::min, math::dot;
  const int x = x_float;
  const int y = y_float;
  const int width = this->get_width();
  const int height = this->get_height();

  /* The weight kernels of the filter optimized for rotational symmetry described in section "3.2.1
   * Gradient Calculation". */
  const float corner_weight = 0.182f;
  const float center_weight = 1.0f - 2.0f * corner_weight;

  float4 input_color;
  float3 x_partial_derivative = float3(0.0f);
  image_reader_->read(input_color, max(0, x - 1), min(height - 1, y + 1), nullptr);
  x_partial_derivative += input_color.xyz() * -corner_weight;
  image_reader_->read(input_color, max(0, x - 1), y, nullptr);
  x_partial_derivative += input_color.xyz() * -center_weight;
  image_reader_->read(input_color, max(0, x - 1), max(0, y - 1), nullptr);
  x_partial_derivative += input_color.xyz() * -corner_weight;
  image_reader_->read(input_color, min(width, x + 1), min(height - 1, y + 1), nullptr);
  x_partial_derivative += input_color.xyz() * corner_weight;
  image_reader_->read(input_color, min(width, x + 1), y, nullptr);
  x_partial_derivative += input_color.xyz() * center_weight;
  image_reader_->read(input_color, min(width, x + 1), max(0, y - 1), nullptr);
  x_partial_derivative += input_color.xyz() * corner_weight;

  float3 y_partial_derivative = float3(0.0f);
  image_reader_->read(input_color, max(0, x - 1), min(height - 1, y + 1), nullptr);
  y_partial_derivative += input_color.xyz() * corner_weight;
  image_reader_->read(input_color, x, min(height - 1, y + 1), nullptr);
  y_partial_derivative += input_color.xyz() * center_weight;
  image_reader_->read(input_color, min(width, x + 1), min(height - 1, y + 1), nullptr);
  y_partial_derivative += input_color.xyz() * corner_weight;
  image_reader_->read(input_color, max(0, x - 1), max(0, y - 1), nullptr);
  y_partial_derivative += input_color.xyz() * -corner_weight;
  image_reader_->read(input_color, x, max(0, y - 1), nullptr);
  y_partial_derivative += input_color.xyz() * -center_weight;
  image_reader_->read(input_color, min(width, x + 1), max(0, y - 1), nullptr);
  y_partial_derivative += input_color.xyz() * -corner_weight;

  /* We encode the structure tensor in a float4 using a column major storage order. */
  float4 structure_tensor = float4(dot(x_partial_derivative, x_partial_derivative),
                                   dot(x_partial_derivative, y_partial_derivative),
                                   dot(x_partial_derivative, y_partial_derivative),
                                   dot(y_partial_derivative, y_partial_derivative));
  copy_v4_v4(output, structure_tensor);
}

/* Computes the structure tensor of the image using a Dirac delta window function as described in
 * section "3.2 Local Structure Estimation" of the paper:
 *
 *   Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
 *   filtering." 2011.
 *
 * The structure tensor should then be smoothed using a Gaussian function to eliminate high
 * frequency details. */
void KuwaharaAnisotropicStructureTensorOperation::update_memory_buffer_partial(
    MemoryBuffer *output, const rcti &area, Span<MemoryBuffer *> inputs)
{
  using math::max, math::min, math::dot;
  MemoryBuffer *image = inputs[0];
  const int width = image->get_width();
  const int height = image->get_height();

  /* The weight kernels of the filter optimized for rotational symmetry described in section
   * "3.2.1 Gradient Calculation". */
  const float corner_weight = 0.182f;
  const float center_weight = 1.0f - 2.0f * corner_weight;

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    float3 input_color;
    float3 x_partial_derivative = float3(0.0f);
    input_color = float3(image->get_elem(max(0, x - 1), min(height - 1, y + 1)));
    x_partial_derivative += input_color * -corner_weight;
    input_color = float3(image->get_elem(max(0, x - 1), y));
    x_partial_derivative += input_color * -center_weight;
    input_color = float3(image->get_elem(max(0, x - 1), max(0, y - 1)));
    x_partial_derivative += input_color * -corner_weight;
    input_color = float3(image->get_elem(min(width - 1, x + 1), min(height - 1, y + 1)));
    x_partial_derivative += input_color * corner_weight;
    input_color = float3(image->get_elem(min(width - 1, x + 1), y));
    x_partial_derivative += input_color * center_weight;
    input_color = float3(image->get_elem(min(width - 1, x + 1), max(0, y - 1)));
    x_partial_derivative += input_color * corner_weight;

    float3 y_partial_derivative = float3(0.0f);
    input_color = float3(image->get_elem(max(0, x - 1), min(height - 1, y + 1)));
    y_partial_derivative += input_color * corner_weight;
    input_color = float3(image->get_elem(x, min(height - 1, y + 1)));
    y_partial_derivative += input_color * center_weight;
    input_color = float3(image->get_elem(min(width - 1, x + 1), min(height - 1, y + 1)));
    y_partial_derivative += input_color * corner_weight;
    input_color = float3(image->get_elem(max(0, x - 1), max(0, y - 1)));
    y_partial_derivative += input_color * -corner_weight;
    input_color = float3(image->get_elem(x, max(0, y - 1)));
    y_partial_derivative += input_color * -center_weight;
    input_color = float3(image->get_elem(min(width - 1, x + 1), max(0, y - 1)));
    y_partial_derivative += input_color * -corner_weight;

    /* We encode the structure tensor in a float4 using a column major storage order. */
    float4 structure_tensor = float4(dot(x_partial_derivative, x_partial_derivative),
                                     dot(x_partial_derivative, y_partial_derivative),
                                     dot(x_partial_derivative, y_partial_derivative),
                                     dot(y_partial_derivative, y_partial_derivative));
    copy_v4_v4(it.out, structure_tensor);
  }
}

}  // namespace blender::compositor
