/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "COM_InpaintOperation.h"
#include "COM_JumpFloodingAlgorithm.h"
#include "COM_SymmetricSeparableBlurVariableSizeAlgorithm.h"

namespace blender::compositor {

void InpaintSimpleOperation::compute_inpainting_region(
    const MemoryBuffer *input,
    const MemoryBuffer &inpainted_region,
    const MemoryBuffer &distance_to_boundary_buffer,
    MemoryBuffer *output)
{
  const int2 size = int2(this->get_width(), this->get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        float4 color = float4(input->get_elem(x, y));

        if (color.w == 1.0f) {
          copy_v4_v4(output->get_elem(x, y), color);
          continue;
        }

        float distance_to_boundary = *distance_to_boundary_buffer.get_elem(x, y);

        if (distance_to_boundary > max_distance_) {
          copy_v4_v4(output->get_elem(x, y), color);
          continue;
        }

        float4 inpainted_color = float4(inpainted_region.get_elem(x, y));
        float4 final_color = float4(math::interpolate(inpainted_color, color, color.w).xyz(),
                                    1.0f);
        copy_v4_v4(output->get_elem(x, y), final_color);
      }
    }
  });
}

void InpaintSimpleOperation::fill_inpainting_region(const MemoryBuffer *input,
                                                    Span<int2> flooded_boundary,
                                                    MemoryBuffer &filled_region,
                                                    MemoryBuffer &distance_to_boundary_buffer,
                                                    MemoryBuffer &smoothing_radius_buffer)
{
  const int2 size = int2(this->get_width(), this->get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        int2 texel = int2(x, y);
        const size_t index = size_t(y) * size.x + x;

        float4 color = float4(input->get_elem(x, y));

        if (color.w == 1.0f) {
          copy_v4_v4(filled_region.get_elem(x, y), color);
          *smoothing_radius_buffer.get_elem(x, y) = 0.0f;
          *distance_to_boundary_buffer.get_elem(x, y) = 0.0f;
          continue;
        }

        int2 closest_boundary_texel = flooded_boundary[index];
        float distance_to_boundary = math::distance(float2(texel), float2(closest_boundary_texel));
        *distance_to_boundary_buffer.get_elem(x, y) = distance_to_boundary;

        float blur_window_size = math::min(float(max_distance_), distance_to_boundary) /
                                 math::numbers::sqrt2;
        bool skip_smoothing = distance_to_boundary > (max_distance_ * 2.0f);
        float smoothing_radius = skip_smoothing ? 0.0f : blur_window_size;
        *smoothing_radius_buffer.get_elem(x, y) = smoothing_radius;

        float4 boundary_color = float4(
            input->get_elem_clamped(closest_boundary_texel.x, closest_boundary_texel.y));
        float4 final_color = math::interpolate(boundary_color, color, color.w);
        copy_v4_v4(filled_region.get_elem(x, y), final_color);
      }
    }
  });
}

Array<int2> InpaintSimpleOperation::compute_inpainting_boundary(const MemoryBuffer *input)
{
  const int2 size = int2(this->get_width(), this->get_height());
  Array<int2> boundary(size_t(size.x) * size.y);

  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        int2 texel = int2(x, y);

        bool has_transparent_neighbors = false;
        for (int j = -1; j <= 1; j++) {
          for (int i = -1; i <= 1; i++) {
            int2 offset = int2(i, j);

            if (offset != int2(0)) {
              if (float4(input->get_elem_clamped(x + i, y + j)).w < 1.0f) {
                has_transparent_neighbors = true;
                break;
              }
            }
          }
        }

        bool is_opaque = float4(input->get_elem(x, y)).w == 1.0f;
        bool is_boundary_pixel = is_opaque && has_transparent_neighbors;

        int2 jump_flooding_value = initialize_jump_flooding_value(texel, is_boundary_pixel);

        const size_t index = size_t(y) * size.x + x;
        boundary[index] = jump_flooding_value;
      }
    }
  });

  return boundary;
}

/* Identical to  realtime_compositor::InpaintOperation::execute see that function, its
 * sub-functions and shaders for more details. */
void InpaintSimpleOperation::inpaint(const MemoryBuffer *input, MemoryBuffer *output)
{
  const int2 size = int2(this->get_width(), this->get_height());
  Array<int2> inpainting_boundary = compute_inpainting_boundary(input);
  Array<int2> flooded_boundary = jump_flooding(inpainting_boundary, size);

  MemoryBuffer filled_region(DataType::Color, input->get_rect());
  MemoryBuffer distance_to_boundary(DataType::Value, input->get_rect());
  MemoryBuffer smoothing_radius(DataType::Value, input->get_rect());
  fill_inpainting_region(
      input, flooded_boundary, filled_region, distance_to_boundary, smoothing_radius);

  MemoryBuffer smoothed_region(DataType::Color, input->get_rect());
  symmetric_separable_blur_variable_size(
      filled_region, smoothed_region, smoothing_radius, R_FILTER_GAUSS, max_distance_);

  compute_inpainting_region(input, smoothed_region, distance_to_boundary, output);
}

InpaintSimpleOperation::InpaintSimpleOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_image_program_ = nullptr;
  cached_buffer_ = nullptr;
  cached_buffer_ready_ = false;
  flags_.complex = true;
  flags_.is_fullframe_operation = true;
  flags_.can_be_constant = true;
}
void InpaintSimpleOperation::init_execution()
{
  input_image_program_ = this->get_input_socket_reader(0);
  cached_buffer_ = nullptr;
  cached_buffer_ready_ = false;
  this->init_mutex();
}

void *InpaintSimpleOperation::initialize_tile_data(rcti *rect)
{
  if (cached_buffer_ready_) {
    return cached_buffer_;
  }
  lock_mutex();
  if (!cached_buffer_ready_) {
    MemoryBuffer *input = (MemoryBuffer *)input_image_program_->initialize_tile_data(rect);
    cached_buffer_ = new MemoryBuffer(DataType::Color, *rect);
    inpaint(input, cached_buffer_);
    cached_buffer_ready_ = true;
  }

  unlock_mutex();
  return cached_buffer_;
}

void InpaintSimpleOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  copy_v4_v4(output, cached_buffer_->get_elem(x, y));
}

void InpaintSimpleOperation::deinit_execution()
{
  input_image_program_ = nullptr;
  this->deinit_mutex();
  if (cached_buffer_) {
    delete cached_buffer_;
    cached_buffer_ = nullptr;
  }

  cached_buffer_ready_ = false;
}

bool InpaintSimpleOperation::determine_depending_area_of_interest(
    rcti * /*input*/, ReadBufferOperation *read_operation, rcti *output)
{
  if (cached_buffer_ready_) {
    return false;
  }

  rcti new_input;

  new_input.xmax = get_width();
  new_input.xmin = 0;
  new_input.ymax = get_height();
  new_input.ymin = 0;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void InpaintSimpleOperation::get_area_of_interest(const int input_idx,
                                                  const rcti & /*output_area*/,
                                                  rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area = this->get_canvas();
}

void InpaintSimpleOperation::update_memory_buffer(MemoryBuffer *output,
                                                  const rcti & /*area*/,
                                                  Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];

  if (input->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input->get_elem(0, 0));
    return;
  }

  if (!cached_buffer_ready_) {
    inpaint(input, output);
    cached_buffer_ready_ = true;
  }
}

}  // namespace blender::compositor
