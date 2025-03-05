/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_range.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "DNA_node_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_compute_preview.hh"

namespace blender::compositor {

static void compute_preview_cpu(Context &context,
                                const Result &input_result,
                                bke::bNodePreview *preview)
{
  const int2 input_size = input_result.domain().size;
  const int2 preview_size = int2(preview->ibuf->x, preview->ibuf->y);

  ColormanageProcessor *color_processor = IMB_colormanagement_display_processor_new(
      &context.get_scene().view_settings, &context.get_scene().display_settings);

  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int2 coordinates = int2((float2(x, y) / float2(preview_size)) * float2(input_size));
        float4 color = input_result.load_pixel_generic_type(coordinates);
        if (input_result.type() == ResultType::Float) {
          color = float4(float3(color.x), 1.0f);
        }
        IMB_colormanagement_processor_apply_v4(color_processor, color);

        const int64_t index = (y * preview_size.x + x) * 4;
        rgba_float_to_uchar(preview->ibuf->byte_buffer.data + index, color);
      }
    }
  });

  IMB_colormanagement_processor_free(color_processor);
}

static void compute_preview_gpu(Context &context,
                                const Result &input_result,
                                bke::bNodePreview *preview)
{
  const int2 preview_size = int2(preview->ibuf->x, preview->ibuf->y);

  GPUShader *shader = context.get_shader("compositor_compute_preview");
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

  ColormanageProcessor *color_processor = IMB_colormanagement_display_processor_new(
      &context.get_scene().view_settings, &context.get_scene().display_settings);

  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int64_t index = (y * preview_size.x + x) * 4;
        IMB_colormanagement_processor_apply_v4(color_processor, preview_pixels + index);
        rgba_float_to_uchar(preview->ibuf->byte_buffer.data + index, preview_pixels + index);
      }
    }
  });

  MEM_freeN(preview_pixels);
  IMB_colormanagement_processor_free(color_processor);
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

void compute_preview(Context &context, const DNode &node, const Result &input_result)
{
  /* Initialize node tree previews if not already initialized. */
  bNodeTree *root_tree = const_cast<bNodeTree *>(
      &node.context()->derived_tree().root_context().btree());

  const int2 preview_size = compute_preview_size(input_result.domain().size);

  bke::bNodePreview *preview = bke::node_preview_verify(
      root_tree->runtime->previews, node.instance_key(), preview_size.x, preview_size.y, true);

  if (context.use_gpu()) {
    compute_preview_gpu(context, input_result, preview);
  }
  else {
    compute_preview_cpu(context, input_result, preview);
  }
}

}  // namespace blender::compositor
