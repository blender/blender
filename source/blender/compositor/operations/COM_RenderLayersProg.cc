/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_RenderLayersProg.h"

#include "BLI_string.h"

#include "BKE_image.h"

namespace blender::compositor {

/* ******** Render Layers Base Prog ******** */

RenderLayersProg::RenderLayersProg(const char *pass_name, DataType type, int elementsize)
    : pass_name_(pass_name)
{
  this->set_scene(nullptr);
  input_buffer_ = nullptr;
  elementsize_ = elementsize;
  rd_ = nullptr;
  layer_buffer_ = nullptr;

  this->add_output_socket(type);
}

void RenderLayersProg::init_execution()
{
  Scene *scene = this->get_scene();
  Render *re = (scene) ? RE_GetSceneRender(scene) : nullptr;
  RenderResult *rr = nullptr;

  if (re) {
    rr = RE_AcquireResultRead(re);
  }

  if (rr) {
    ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, get_layer_id());
    if (view_layer) {

      RenderLayer *rl = RE_GetRenderLayer(rr, view_layer->name);
      if (rl) {
        input_buffer_ = RE_RenderLayerGetPass(rl, pass_name_.c_str(), view_name_);
        if (input_buffer_) {
          layer_buffer_ = new MemoryBuffer(input_buffer_, elementsize_, get_width(), get_height());
        }
      }
    }
  }
  if (re) {
    RE_ReleaseResult(re);
    re = nullptr;
  }
}

void RenderLayersProg::do_interpolation(float output[4], float x, float y, PixelSampler sampler)
{
  uint offset;
  int width = this->get_width(), height = this->get_height();

  int ix = x, iy = y;
  if (ix < 0 || iy < 0 || ix >= width || iy >= height) {
    if (elementsize_ == 1) {
      output[0] = 0.0f;
    }
    else if (elementsize_ == 3) {
      zero_v3(output);
    }
    else {
      zero_v4(output);
    }
    return;
  }

  switch (sampler) {
    case PixelSampler::Nearest: {
      offset = (iy * width + ix) * elementsize_;

      if (elementsize_ == 1) {
        output[0] = input_buffer_[offset];
      }
      else if (elementsize_ == 3) {
        copy_v3_v3(output, &input_buffer_[offset]);
      }
      else {
        copy_v4_v4(output, &input_buffer_[offset]);
      }
      break;
    }

    case PixelSampler::Bilinear:
      BLI_bilinear_interpolation_fl(input_buffer_, output, width, height, elementsize_, x, y);
      break;

    case PixelSampler::Bicubic:
      BLI_bicubic_interpolation_fl(input_buffer_, output, width, height, elementsize_, x, y);
      break;
  }
}

void RenderLayersProg::execute_pixel_sampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
#if 0
  const RenderData *rd = rd_;

  int dx = 0, dy = 0;

  if (rd->mode & R_BORDER && rd->mode & R_CROP) {
    /* see comment in execute_region describing coordinate mapping,
     * here it simply goes other way around
     */
    int full_width, full_height;
    BKE_render_resolution(rd, false, &full_width, &full_height);

    dx = rd->border.xmin * full_width - (full_width - this->get_width()) / 2.0f;
    dy = rd->border.ymin * full_height - (full_height - this->get_height()) / 2.0f;
  }

  int ix = x - dx;
  int iy = y - dy;
#endif

#ifndef NDEBUG
  {
    const DataType data_type = this->get_output_socket()->get_data_type();
    int actual_element_size = elementsize_;
    int expected_element_size;
    if (data_type == DataType::Value) {
      expected_element_size = 1;
    }
    else if (data_type == DataType::Vector) {
      expected_element_size = 3;
    }
    else if (data_type == DataType::Color) {
      expected_element_size = 4;
    }
    else {
      expected_element_size = 0;
      BLI_assert_msg(0, "Something horribly wrong just happened");
    }
    BLI_assert(expected_element_size == actual_element_size);
  }
#endif

  if (input_buffer_ == nullptr) {
    int elemsize = elementsize_;
    if (elemsize == 1) {
      output[0] = 0.0f;
    }
    else if (elemsize == 3) {
      zero_v3(output);
    }
    else {
      BLI_assert(elemsize == 4);
      zero_v4(output);
    }
  }
  else {
    do_interpolation(output, x, y, sampler);
  }
}

void RenderLayersProg::deinit_execution()
{
  input_buffer_ = nullptr;
  if (layer_buffer_) {
    delete layer_buffer_;
    layer_buffer_ = nullptr;
  }
}

void RenderLayersProg::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  Scene *sce = this->get_scene();
  Render *re = (sce) ? RE_GetSceneRender(sce) : nullptr;
  RenderResult *rr = nullptr;

  r_area = COM_AREA_NONE;

  if (re) {
    rr = RE_AcquireResultRead(re);
  }

  if (rr) {
    ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&sce->view_layers, get_layer_id());
    if (view_layer) {
      RenderLayer *rl = RE_GetRenderLayer(rr, view_layer->name);
      if (rl) {
        BLI_rcti_init(&r_area, 0, rl->rectx, 0, rl->recty);
      }
    }
  }

  if (re) {
    RE_ReleaseResult(re);
  }
}

std::unique_ptr<MetaData> RenderLayersProg::get_meta_data()
{
  Scene *scene = this->get_scene();
  Render *re = (scene) ? RE_GetSceneRender(scene) : nullptr;
  RenderResult *render_result = nullptr;
  MetaDataExtractCallbackData callback_data = {nullptr};

  if (re) {
    render_result = RE_AcquireResultRead(re);
  }

  if (render_result && render_result->stamp_data) {
    ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, get_layer_id());
    if (view_layer) {
      std::string full_layer_name = std::string(
                                        view_layer->name,
                                        BLI_strnlen(view_layer->name, sizeof(view_layer->name))) +
                                    "." + pass_name_;
      blender::StringRef cryptomatte_layer_name =
          blender::bke::cryptomatte::BKE_cryptomatte_extract_layer_name(full_layer_name);
      callback_data.set_cryptomatte_keys(cryptomatte_layer_name);

      BKE_stamp_info_callback(&callback_data,
                              render_result->stamp_data,
                              MetaDataExtractCallbackData::extract_cryptomatte_meta_data,
                              false);
    }
  }

  if (re) {
    RE_ReleaseResult(re);
    re = nullptr;
  }

  return std::move(callback_data.meta_data);
}

void RenderLayersProg::update_memory_buffer_partial(MemoryBuffer *output,
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> /*inputs*/)
{
  BLI_assert(output->get_num_channels() >= elementsize_);
  if (layer_buffer_) {
    output->copy_from(layer_buffer_, area, 0, elementsize_, 0);
  }
  else {
    std::unique_ptr<float[]> zero_elem = std::make_unique<float[]>(elementsize_);
    output->fill(area, 0, zero_elem.get(), elementsize_);
  }
}

/* ******** Render Layers AO Operation ******** */

void RenderLayersAOOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float *input_buffer = this->get_input_buffer();
  if (input_buffer == nullptr) {
    zero_v3(output);
  }
  else {
    do_interpolation(output, x, y, sampler);
  }
  output[3] = 1.0f;
}

void RenderLayersAOOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> /*inputs*/)
{
  BLI_assert(output->get_num_channels() == COM_DATA_TYPE_COLOR_CHANNELS);
  BLI_assert(elementsize_ == COM_DATA_TYPE_COLOR_CHANNELS);
  if (layer_buffer_) {
    output->copy_from(layer_buffer_, area, 0, COM_DATA_TYPE_VECTOR_CHANNELS, 0);
  }
  else {
    output->fill(area, 0, COM_VECTOR_ZERO, COM_DATA_TYPE_VECTOR_CHANNELS);
  }
  output->fill(area, 3, COM_VALUE_ONE, COM_DATA_TYPE_VALUE_CHANNELS);
}

/* ******** Render Layers Alpha Operation ******** */

void RenderLayersAlphaProg::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float *input_buffer = this->get_input_buffer();

  if (input_buffer == nullptr) {
    output[0] = 0.0f;
  }
  else {
    float temp[4];
    do_interpolation(temp, x, y, sampler);
    output[0] = temp[3];
  }
}

void RenderLayersAlphaProg::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> /*inputs*/)
{
  BLI_assert(output->get_num_channels() == COM_DATA_TYPE_VALUE_CHANNELS);
  BLI_assert(elementsize_ == COM_DATA_TYPE_COLOR_CHANNELS);
  if (layer_buffer_) {
    output->copy_from(layer_buffer_, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
  }
  else {
    output->fill(area, COM_VALUE_ZERO);
  }
}

/* ******** Render Layers Depth Operation ******** */

void RenderLayersDepthProg::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler /*sampler*/)
{
  int ix = x;
  int iy = y;
  float *input_buffer = this->get_input_buffer();

  if (input_buffer == nullptr || ix < 0 || iy < 0 || ix >= int(this->get_width()) ||
      iy >= int(this->get_height()))
  {
    output[0] = 10e10f;
  }
  else {
    uint offset = (iy * this->get_width() + ix);
    output[0] = input_buffer[offset];
  }
}

void RenderLayersDepthProg::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> /*inputs*/)
{
  BLI_assert(output->get_num_channels() == COM_DATA_TYPE_VALUE_CHANNELS);
  BLI_assert(elementsize_ == COM_DATA_TYPE_VALUE_CHANNELS);
  if (layer_buffer_) {
    output->copy_from(layer_buffer_, area);
  }
  else {
    const float default_depth = 10e10f;
    output->fill(area, &default_depth);
  }
}

}  // namespace blender::compositor
