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
  input_size_ = nullptr;
  input_offset_ = nullptr;
  rd_ = nullptr;
  pool_ = nullptr;
  scene_color_manage_ = false;
  flags_.complex = true;
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
  input_offset_ = get_input_socket_reader(0);
  input_size_ = get_input_socket_reader(1);
  pool_ = BKE_image_pool_new();
  if (texture_ != nullptr && texture_->nodetree != nullptr && texture_->use_nodes) {
    ntreeTexBeginExecTree(texture_->nodetree);
  }
  NodeOperation::init_execution();
}
void TextureBaseOperation::deinit_execution()
{
  input_size_ = nullptr;
  input_offset_ = nullptr;
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

  if (execution_model_ == eExecutionModel::FullFrame) {
    /* Determine inputs. */
    rcti temp = COM_AREA_NONE;
    NodeOperation::determine_canvas(r_area, temp);
  }
}

void TextureAlphaOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float color[4];
  TextureBaseOperation::execute_pixel_sampled(color, x, y, sampler);
  output[0] = color[3];
}

void TextureBaseOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  TexResult texres = {0.0f};
  float texture_size[4];
  float texture_offset[4];
  float vec[3];
  int retval;
  const float cx = this->get_width() / 2;
  const float cy = this->get_height() / 2;
  float u = (x - cx) / this->get_width() * 2;
  float v = (y - cy) / this->get_height() * 2;

  /* When no interpolation/filtering happens in multitex() force nearest interpolation.
   * We do it here because (a) we can't easily say multitex() that we want nearest
   * interpolation and (b) in such configuration multitex() simply floor's the value
   * which often produces artifacts.
   */
  if (texture_ != nullptr && (texture_->imaflag & TEX_INTERPOL) == 0) {
    u += 0.5f / cx;
    v += 0.5f / cy;
  }

  input_size_->read_sampled(texture_size, x, y, sampler);
  input_offset_->read_sampled(texture_offset, x, y, sampler);

  vec[0] = texture_size[0] * (u + texture_offset[0]);
  vec[1] = texture_size[1] * (v + texture_offset[1]);
  vec[2] = texture_size[2] * texture_offset[2];

  const int thread_id = WorkScheduler::current_thread_id();
  retval = multitex_ext(
      texture_, vec, nullptr, nullptr, 0, &texres, thread_id, pool_, scene_color_manage_, false);

  output[3] = texres.talpha ? texres.trgba[3] : texres.tin;
  if (retval & TEX_RGB) {
    copy_v3_v3(output, texres.trgba);
  }
  else {
    output[0] = output[1] = output[2] = output[3];
  }
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
