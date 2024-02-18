/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdlib>

#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "COM_DoubleEdgeMaskOperation.h"
#include "COM_JumpFloodingAlgorithm.h"

/* Exact copies of the functions in compositor_double_edge_mask_compute_boundary.glsl and
 * compositor_double_edge_mask_compute_gradient.glsl but adapted for CPU. See those files for more
 * information. */

namespace blender::compositor {

static float load_mask(const float *input, int2 texel, int2 size)
{
  const int2 clamped_texel = math::clamp(texel, int2(0), size - 1);
  return input[size_t(clamped_texel.y) * size.x + clamped_texel.x];
}

static float load_mask(const float *input, int2 texel, int2 size, float fallback)
{
  if (texel.x < 0 || texel.x >= size.x || texel.y < 0 || texel.y >= size.y) {
    return fallback;
  }
  return input[size_t(texel.y) * size.x + texel.x];
}

void DoubleEdgeMaskOperation::compute_boundary(const float *inner_mask,
                                               const float *outer_mask,
                                               MutableSpan<int2> inner_boundary,
                                               MutableSpan<int2> outer_boundary)
{
  const int2 size = int2(this->get_width(), this->get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        int2 texel = int2(x, y);

        bool has_inner_non_masked_neighbors = false;
        bool has_outer_non_masked_neighbors = false;
        for (int j = -1; j <= 1; j++) {
          for (int i = -1; i <= 1; i++) {
            int2 offset = int2(i, j);

            if (offset == int2(0)) {
              continue;
            }

            if (load_mask(inner_mask, texel + offset, size) == 0.0f) {
              has_inner_non_masked_neighbors = true;
            }

            float boundary_fallback = include_edges_of_image_ ? 0.0f : 1.0f;
            if (load_mask(outer_mask, texel + offset, size, boundary_fallback) == 0.0f) {
              has_outer_non_masked_neighbors = true;
            }

            if (has_inner_non_masked_neighbors && has_outer_non_masked_neighbors) {
              break;
            }
          }
        }

        bool is_inner_masked = load_mask(inner_mask, texel, size) > 0.0f;
        bool is_outer_masked = load_mask(outer_mask, texel, size) > 0.0f;

        bool is_inner_boundary = is_inner_masked && has_inner_non_masked_neighbors &&
                                 (is_outer_masked || include_all_inner_edges_);
        bool is_outer_boundary = is_outer_masked && !is_inner_masked &&
                                 has_outer_non_masked_neighbors;

        int2 inner_jump_flooding_value = initialize_jump_flooding_value(texel, is_inner_boundary);
        int2 outer_jump_flooding_value = initialize_jump_flooding_value(texel, is_outer_boundary);

        const size_t output_index = size_t(texel.y) * size.x + texel.x;
        inner_boundary[output_index] = inner_jump_flooding_value;
        outer_boundary[output_index] = outer_jump_flooding_value;
      }
    }
  });
}

void DoubleEdgeMaskOperation::compute_gradient(const float *inner_mask_buffer,
                                               const float *outer_mask_buffer,
                                               MutableSpan<int2> flooded_inner_boundary,
                                               MutableSpan<int2> flooded_outer_boundary,
                                               float *output_mask)
{
  const int2 size = int2(this->get_width(), this->get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        int2 texel = int2(x, y);
        const size_t index = size_t(texel.y) * size.x + texel.x;

        float inner_mask = inner_mask_buffer[index];
        if (inner_mask != 0.0f) {
          output_mask[index] = 1.0f;
          continue;
        }

        float outer_mask = outer_mask_buffer[index];
        if (outer_mask == 0.0f) {
          output_mask[index] = 0.0f;
          continue;
        }

        int2 inner_boundary_texel = flooded_inner_boundary[index];
        int2 outer_boundary_texel = flooded_outer_boundary[index];
        float distance_to_inner = math::distance(float2(texel), float2(inner_boundary_texel));
        float distance_to_outer = math::distance(float2(texel), float2(outer_boundary_texel));

        float gradient = distance_to_outer / (distance_to_outer + distance_to_inner);

        output_mask[index] = gradient;
      }
    }
  });
}

void DoubleEdgeMaskOperation::compute_double_edge_mask(const float *inner_mask,
                                                       const float *outer_mask,
                                                       float *output_mask)
{
  const int2 size = int2(this->get_width(), this->get_height());
  Array<int2> inner_boundary(size_t(size.x) * size.y);
  Array<int2> outer_boundary(size_t(size.x) * size.y);
  compute_boundary(inner_mask, outer_mask, inner_boundary, outer_boundary);
  Array<int2> flooded_inner_boundary = jump_flooding(inner_boundary, size);
  Array<int2> flooded_outer_boundary = jump_flooding(outer_boundary, size);
  compute_gradient(
      inner_mask, outer_mask, flooded_inner_boundary, flooded_outer_boundary, output_mask);
}

DoubleEdgeMaskOperation::DoubleEdgeMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_inner_mask_ = nullptr;
  input_outer_mask_ = nullptr;
  include_all_inner_edges_ = false;
  include_edges_of_image_ = false;
  flags_.complex = true;
  flags_.can_be_constant = true;
  is_output_rendered_ = false;
}

bool DoubleEdgeMaskOperation::determine_depending_area_of_interest(
    rcti * /*input*/, ReadBufferOperation *read_operation, rcti *output)
{
  if (cached_instance_ == nullptr) {
    rcti new_input;
    new_input.xmax = this->get_width();
    new_input.xmin = 0;
    new_input.ymax = this->get_height();
    new_input.ymin = 0;
    return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
  }

  return false;
}

void DoubleEdgeMaskOperation::init_execution()
{
  input_inner_mask_ = this->get_input_socket_reader(0);
  input_outer_mask_ = this->get_input_socket_reader(1);
  init_mutex();
  cached_instance_ = nullptr;
}

void *DoubleEdgeMaskOperation::initialize_tile_data(rcti *rect)
{
  if (cached_instance_) {
    return cached_instance_;
  }

  lock_mutex();
  if (cached_instance_ == nullptr) {
    MemoryBuffer *inner_mask = (MemoryBuffer *)input_inner_mask_->initialize_tile_data(rect);
    MemoryBuffer *outer_mask = (MemoryBuffer *)input_outer_mask_->initialize_tile_data(rect);
    float *data = (float *)MEM_mallocN(sizeof(float) * this->get_width() * this->get_height(),
                                       __func__);
    float *imask = inner_mask->get_buffer();
    float *omask = outer_mask->get_buffer();
    compute_double_edge_mask(imask, omask, data);
    cached_instance_ = data;
  }
  unlock_mutex();
  return cached_instance_;
}
void DoubleEdgeMaskOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float *buffer = (float *)data;
  int index = (y * this->get_width() + x);
  output[0] = buffer[index];
}

void DoubleEdgeMaskOperation::deinit_execution()
{
  input_inner_mask_ = nullptr;
  input_outer_mask_ = nullptr;
  deinit_mutex();
  if (cached_instance_) {
    MEM_freeN(cached_instance_);
    cached_instance_ = nullptr;
  }
}

void DoubleEdgeMaskOperation::get_area_of_interest(int /*input_idx*/,
                                                   const rcti & /*output_area*/,
                                                   rcti &r_input_area)
{
  r_input_area = this->get_canvas();
}

void DoubleEdgeMaskOperation::update_memory_buffer(MemoryBuffer *output,
                                                   const rcti & /*area*/,
                                                   Span<MemoryBuffer *> inputs)
{
  if (!is_output_rendered_) {
    /* Ensure full buffers to work with no strides. */
    MemoryBuffer *input_inner_mask = inputs[0];
    MemoryBuffer *inner_mask = input_inner_mask->is_a_single_elem() ? input_inner_mask->inflate() :
                                                                      input_inner_mask;
    MemoryBuffer *input_outer_mask = inputs[1];
    MemoryBuffer *outer_mask = input_outer_mask->is_a_single_elem() ? input_outer_mask->inflate() :
                                                                      input_outer_mask;

    BLI_assert(output->get_width() == this->get_width());
    BLI_assert(output->get_height() == this->get_height());
    compute_double_edge_mask(
        inner_mask->get_buffer(), outer_mask->get_buffer(), output->get_buffer());
    is_output_rendered_ = true;

    if (inner_mask != input_inner_mask) {
      delete inner_mask;
    }
    if (outer_mask != input_outer_mask) {
      delete outer_mask;
    }
  }
}

}  // namespace blender::compositor
