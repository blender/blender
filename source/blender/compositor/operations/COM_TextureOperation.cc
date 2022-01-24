/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_TextureOperation.h"
#include "COM_WorkScheduler.h"

#include "BKE_image.h"
#include "BKE_node.h"

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
      texture_->nodetree->execdata != nullptr) {
    ntreeTexEndExecTree(texture_->nodetree->execdata);
  }
  NodeOperation::deinit_execution();
}

void TextureBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
  if (BLI_rcti_is_empty(&preferred_area)) {
    int width = rd_->xsch * rd_->size / 100;
    int height = rd_->ysch * rd_->size / 100;
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
  TexResult texres = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, nullptr};
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

  output[3] = texres.talpha ? texres.ta : texres.tin;
  if (retval & TEX_RGB) {
    output[0] = texres.tr;
    output[1] = texres.tg;
    output[2] = texres.tb;
  }
  else {
    output[0] = output[1] = output[2] = output[3];
  }
}

void TextureBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  const int op_width = this->get_width();
  const int op_height = this->get_height();
  const float center_x = op_width / 2;
  const float center_y = op_height / 2;
  TexResult tex_result = {0};
  float vec[3];
  const int thread_id = WorkScheduler::current_thread_id();
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *tex_offset = it.in(0);
    const float *tex_size = it.in(1);
    float u = (it.x - center_x) / op_width * 2;
    float v = (it.y - center_y) / op_height * 2;

    /* When no interpolation/filtering happens in multitex() force nearest interpolation.
     * We do it here because (a) we can't easily say multitex() that we want nearest
     * interpolation and (b) in such configuration multitex() simply floor's the value
     * which often produces artifacts.
     */
    if (texture_ != nullptr && (texture_->imaflag & TEX_INTERPOL) == 0) {
      u += 0.5f / center_x;
      v += 0.5f / center_y;
    }

    vec[0] = tex_size[0] * (u + tex_offset[0]);
    vec[1] = tex_size[1] * (v + tex_offset[1]);
    vec[2] = tex_size[2] * tex_offset[2];

    const int retval = multitex_ext(texture_,
                                    vec,
                                    nullptr,
                                    nullptr,
                                    0,
                                    &tex_result,
                                    thread_id,
                                    pool_,
                                    scene_color_manage_,
                                    false);

    it.out[3] = tex_result.talpha ? tex_result.ta : tex_result.tin;
    if (retval & TEX_RGB) {
      it.out[0] = tex_result.tr;
      it.out[1] = tex_result.tg;
      it.out[2] = tex_result.tb;
    }
    else {
      it.out[0] = it.out[1] = it.out[2] = it.out[3];
    }
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
