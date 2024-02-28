/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TextureOperation.h"
#include "COM_WorkScheduler.h"

#include "BKE_image.h"
#include "BKE_node.hh"
#include "BKE_scene.hh"

#include "NOD_texture.h"

namespace blender::compositor {

TextureBaseOperation::TextureBaseOperation()
{
  this->add_input_socket(DataType::Vector);  // offset
  this->add_input_socket(DataType::Vector);  // size
  texture_ = nullptr;
  rd_ = nullptr;
  pool_ = nullptr;
  scene_color_manage_ = false;
}
TextureOperation::TextureOperation() : TextureBaseOperation()
{
  this->add_output_socket(DataType::Color);
}
TextureAlphaOperation::TextureAlphaOperation() : TextureBaseOperation()
{
  this->add_output_socket(DataType::Value);
}

void TextureBaseOperation::init_execution()
{
  pool_ = BKE_image_pool_new();
  if (texture_ != nullptr && texture_->nodetree != nullptr && texture_->use_nodes) {
    ntreeTexBeginExecTree(texture_->nodetree);
  }
  NodeOperation::init_execution();
}
void TextureBaseOperation::deinit_execution()
{
  BKE_image_pool_free(pool_);
  pool_ = nullptr;
  if (texture_ != nullptr && texture_->use_nodes && texture_->nodetree != nullptr &&
      texture_->nodetree->runtime->execdata != nullptr)
  {
    ntreeTexEndExecTree(texture_->nodetree->runtime->execdata);
  }
  NodeOperation::deinit_execution();
}

void TextureBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
  if (BLI_rcti_is_empty(&preferred_area)) {
    int width, height;
    BKE_render_resolution(rd_, false, &width, &height);
    r_area.xmax = preferred_area.xmin + width;
    r_area.ymax = preferred_area.ymin + height;
  }

  /* Determine inputs. */
  rcti temp = COM_AREA_NONE;
  NodeOperation::determine_canvas(r_area, temp);
}

void TextureBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  const float3 offset = inputs[0]->get_elem(0, 0);
  const float3 scale = inputs[1]->get_elem(0, 0);
  const int2 size = int2(this->get_width(), this->get_height());
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    /* Compute the coordinates in the [-1, 1] range and add 0.5 to evaluate the texture at the
     * center of pixels in case it was interpolated. */
    const float2 pixel_coordinates = ((float2(it.x, it.y) + 0.5f) / float2(size)) * 2.0f - 1.0f;
    /* Note that it is expected that the offset is scaled by the scale. */
    const float3 coordinates = (float3(pixel_coordinates, 0.0f) + offset) * scale;

    TexResult texture_result;
    const int result_type = multitex_ext(texture_,
                                         coordinates,
                                         nullptr,
                                         nullptr,
                                         0,
                                         &texture_result,
                                         WorkScheduler::current_thread_id(),
                                         pool_,
                                         scene_color_manage_,
                                         false);

    float4 color = float4(texture_result.trgba);
    color.w = texture_result.talpha ? color.w : texture_result.tin;
    if (!(result_type & TEX_RGB)) {
      copy_v3_fl(color, color.w);
    }
    copy_v4_v4(it.out, color);
  }
}

void TextureAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  MemoryBuffer texture(DataType::Color, area);
  TextureBaseOperation::update_memory_buffer_partial(&texture, area, inputs);
  output->copy_from(&texture, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
}

}  // namespace blender::compositor
