/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_RenderLayersProg.h"

#include "BLI_math_interp.hh"
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
    case PixelSampler::Nearest:
      math::interpolate_nearest_border_fl(
          input_buffer_, output, width, height, elementsize_, x, y);
      break;
    case PixelSampler::Bilinear:
      math::interpolate_bilinear_border_fl(
          input_buffer_, output, width, height, elementsize_, x, y);
      break;
    case PixelSampler::Bicubic:
      math::interpolate_cubic_bspline_fl(input_buffer_, output, width, height, elementsize_, x, y);
      break;
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
