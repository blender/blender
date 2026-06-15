/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_range.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "DNA_node_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "BKE_type_conversions.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_compute_preview.hh"

namespace blender::compositor {

static void compute_preview_cpu(Context &context, const Result &input, ImBuf *output)
{
  const int2 input_size = input.domain().data_size;
  const int2 preview_size = int2(output->x, output->y);

  Result input_as_color = context.create_result(ResultType::Color);
  if (input.type() == ResultType::Color) {
    input_as_color.share_data(input);
  }
  else {
    input_as_color.allocate_texture(input.domain());
    const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
    conversions.convert_to_initialized_n(input.cpu_data(), input_as_color.cpu_data_for_write());
  }

  ColormanageProcessor color_processor = ColormanageProcessor::display_processor_new(
      &context.get_scene().view_settings, &context.get_scene().display_settings);

  uchar *data_dst = output->byte_data_for_write();
  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int2 coordinates = int2((float2(x, y) / float2(preview_size)) * float2(input_size));

        Color color = input_as_color.load_pixel<Color>(coordinates);
        color_processor.apply_v4(color);

        const int64_t index = (y * preview_size.x + x) * 4;
        rgba_float_to_uchar(data_dst + index, color);
      }
    }
  });

  input_as_color.release();
}

static void compute_preview_gpu(Context &context, const Result &input_result, ImBuf *output)
{
  const int2 preview_size = int2(output->x, output->y);

  gpu::Shader *shader = context.get_shader("compositor_compute_preview");
  GPU_shader_bind(shader);

  if (input_result.type() == ResultType::Float) {
    GPU_texture_swizzle_set(input_result, "rrr1");
  }

  input_result.bind_as_texture(shader, "input_tx");

  Result preview_result = context.create_result(ResultType::Color);
  preview_result.allocate_texture(Domain(preview_size));
  preview_result.bind_as_image(shader, "preview_img");

  compute_dispatch_threads_at_least(shader, preview_size);

  input_result.unbind_as_texture();
  preview_result.unbind_as_image();
  GPU_shader_unbind();

  /* Restore original swizzle mask set above. */
  if (input_result.type() == ResultType::Float) {
    GPU_texture_swizzle_set(input_result, "rgba");
  }

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
  float *preview_pixels = static_cast<float *>(
      GPU_texture_read(preview_result, GPU_DATA_FLOAT, 0));
  preview_result.release();

  ColormanageProcessor color_processor = ColormanageProcessor::display_processor_new(
      &context.get_scene().view_settings, &context.get_scene().display_settings);

  uchar *data_dst = output->byte_data_for_write();
  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int64_t index = (y * preview_size.x + x) * 4;
        color_processor.apply_v4(preview_pixels + index);
        rgba_float_to_uchar(data_dst + index, preview_pixels + index);
      }
    }
  });

  MEM_delete(preview_pixels);
}

/* Given the size of a result, compute a lower resolution size for a preview. The greater dimension
 * will be assigned an arbitrarily chosen size of 128, while the other dimension will get the size
 * that maintains the same aspect ratio. */
static int2 compute_preview_size(int2 size)
{
  const int greater_dimension_size = 128;
  if (size.x > size.y) {
    return int2(greater_dimension_size, int(greater_dimension_size * (float(size.y) / size.x)));
  }
  return int2(int(greater_dimension_size * (float(size.x) / size.y)), greater_dimension_size);
}

ImBuf *compute_preview(Context &context, const Result &input)
{
  const int2 preview_size = compute_preview_size(input.domain().data_size);
  ImBuf *image_buffer = IMB_allocImBuf(UNPACK2(preview_size), ImBufFlags::ByteData);
  if (context.use_gpu()) {
    compute_preview_gpu(context, input, image_buffer);
  }
  else {
    compute_preview_cpu(context, input, image_buffer);
  }

  return image_buffer;
}

}  // namespace blender::compositor
