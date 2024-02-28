/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "COM_VariableSizeBokehBlurOperation.h"

namespace blender::compositor {

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color, ResizeMode::Align); /* Do not resize the bokeh image. */
  this->add_input_socket(DataType::Value);                    /* Radius. */
  this->add_input_socket(DataType::Value);                    /* Bounding Box. */
#ifdef COM_DEFOCUS_SEARCH
  /* Inverse search radius optimization structure. */
  this->add_input_socket(DataType::Color, ResizeMode::None);
#endif
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;

  max_blur_ = 32.0f;
  threshold_ = 1.0f;
  do_size_scale_ = false;
}

void VariableSizeBokehBlurOperation::init_execution()
{
  QualityStepHelper::init_execution(COM_QH_INCREASE);
}
struct VariableSizeBokehBlurTileData {
  MemoryBuffer *color;
  MemoryBuffer *bokeh;
  MemoryBuffer *size;
  MemoryBuffer *mask;
  int max_blur_scalar;
};

void VariableSizeBokehBlurOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX:
    case BOUNDING_BOX_INPUT_INDEX:
    case SIZE_INPUT_INDEX: {
      const float max_dim = std::max(get_width(), get_height());
      const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
      const int max_blur_scalar = max_blur_ * scalar;
      r_input_area.xmax = output_area.xmax + max_blur_scalar + 2;
      r_input_area.xmin = output_area.xmin - max_blur_scalar - 2;
      r_input_area.ymax = output_area.ymax + max_blur_scalar + 2;
      r_input_area.ymin = output_area.ymin - max_blur_scalar - 2;
      break;
    }
    case BOKEH_INPUT_INDEX: {
      r_input_area = output_area;
      r_input_area.xmax = r_input_area.xmin + COM_BLUR_BOKEH_PIXELS;
      r_input_area.ymax = r_input_area.ymin + COM_BLUR_BOKEH_PIXELS;
      break;
    }
#ifdef COM_DEFOCUS_SEARCH
    case DEFOCUS_INPUT_INDEX: {
      r_input_area.xmax = (output_area.xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
      r_input_area.xmin = (output_area.xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
      r_input_area.ymax = (output_area.ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
      r_input_area.ymin = (output_area.ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
      break;
    }
#endif
  }
}

void VariableSizeBokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input_buffer = inputs[0];
  MemoryBuffer *bokeh_buffer = inputs[1];
  MemoryBuffer *size_buffer = inputs[2];
  MemoryBuffer *mask_buffer = inputs[3];

  const float max_dim = std::max(get_width(), get_height());
  const float base_size = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  const float maximum_size = size_buffer->get_max_value();
  const int search_radius = math::clamp(int(maximum_size * base_size), 0, max_blur_);
  const int2 bokeh_size = int2(bokeh_buffer->get_width(), bokeh_buffer->get_height());

  BuffersIterator<float> it = output->iterate_with({}, area);
  for (; !it.is_end(); ++it) {
    if (*mask_buffer->get_elem(it.x, it.y) <= 0.0f) {
      copy_v4_v4(it.out, input_buffer->get_elem(it.x, it.y));
      continue;
    }

    const float center_size = math::max(0.0f, *size_buffer->get_elem(it.x, it.y) * base_size);

    float4 accumulated_color = float4(input_buffer->get_elem(it.x, it.y));
    float4 accumulated_weight = float4(1.0f);
    const int step = get_step();
    if (center_size >= threshold_) {
      for (int yi = -search_radius; yi <= search_radius; yi += step) {
        for (int xi = -search_radius; xi <= search_radius; xi += step) {
          if (xi == 0 && yi == 0) {
            continue;
          }
          const float candidate_size = math::max(
              0.0f, *size_buffer->get_elem_clamped(it.x + xi, it.y + yi) * base_size);
          const float size = math::min(center_size, candidate_size);
          if (size < threshold_ || math::max(math::abs(xi), math::abs(yi)) > size) {
            continue;
          }

          const float2 normalized_texel = (float2(xi, yi) + size + 0.5f) / (size * 2.0f + 1.0f);
          const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
          const float4 weight = bokeh_buffer->get_elem(int(weight_texel.x), int(weight_texel.y));
          const float4 color = input_buffer->get_elem_clamped(it.x + xi, it.y + yi);
          accumulated_color += color * weight;
          accumulated_weight += weight;
        }
      }
    }

    const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
    copy_v4_v4(it.out, final_color);

    /* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
    if ((center_size > threshold_) && (center_size < threshold_ * 2.0f)) {
      /* factor from 0-1 */
      float fac = (center_size - threshold_) / threshold_;
      interp_v4_v4v4(it.out, input_buffer->get_elem(it.x, it.y), it.out, fac);
    }
  }
}

#ifdef COM_DEFOCUS_SEARCH
/* #InverseSearchRadiusOperation. */
InverseSearchRadiusOperation::InverseSearchRadiusOperation()
{
  this->add_input_socket(DataType::Value, ResizeMode::Align); /* Radius. */
  this->add_output_socket(DataType::Color);
}

void InverseSearchRadiusOperation::determine_resolution(uint resolution[2],
                                                        uint preferred_resolution[2])
{
  NodeOperation::determine_resolution(resolution, preferred_resolution);
  resolution[0] = resolution[0] / DIVIDER;
  resolution[1] = resolution[1] / DIVIDER;
}

#endif

}  // namespace blender::compositor
