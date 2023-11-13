/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultilayerImageOperation.h"

#include "BLI_string.h"

#include "IMB_imbuf.h"

namespace blender::compositor {

MultilayerBaseOperation::MultilayerBaseOperation(RenderLayer *render_layer,
                                                 RenderPass *render_pass,
                                                 int view)
{
  pass_id_ = BLI_findindex(&render_layer->passes, render_pass);
  view_ = view;
  render_layer_ = render_layer;
  render_pass_ = render_pass;
}

ImBuf *MultilayerBaseOperation::get_im_buf()
{
  /* temporarily changes the view to get the right ImBuf */
  int view = image_user_->view;

  image_user_->view = view_;
  image_user_->pass = pass_id_;

  if (BKE_image_multilayer_index(image_->rr, image_user_)) {
    ImBuf *ibuf = BaseImageOperation::get_im_buf();
    image_user_->view = view;
    return ibuf;
  }

  image_user_->view = view;
  return nullptr;
}

void MultilayerBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> /*inputs*/)
{
  if (buffer_) {
    output->copy_from(buffer_, area);
  }
  else {
    output->clear();
  }
}

std::unique_ptr<MetaData> MultilayerColorOperation::get_meta_data()
{
  BLI_assert(buffer_);
  MetaDataExtractCallbackData callback_data = {nullptr};
  RenderResult *render_result = image_->rr;
  if (render_result && render_result->stamp_data) {
    RenderLayer *render_layer = render_layer_;
    RenderPass *render_pass = render_pass_;
    std::string full_layer_name =
        std::string(render_layer->name,
                    BLI_strnlen(render_layer->name, sizeof(render_layer->name))) +
        "." +
        std::string(render_pass->name, BLI_strnlen(render_pass->name, sizeof(render_pass->name)));
    blender::StringRef cryptomatte_layer_name =
        blender::bke::cryptomatte::BKE_cryptomatte_extract_layer_name(full_layer_name);
    callback_data.set_cryptomatte_keys(cryptomatte_layer_name);

    BKE_stamp_info_callback(&callback_data,
                            render_result->stamp_data,
                            MetaDataExtractCallbackData::extract_cryptomatte_meta_data,
                            false);
  }

  return std::move(callback_data.meta_data);
}

void MultilayerColorOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  if (image_float_buffer_ == nullptr) {
    zero_v4(output);
  }
  else {
    if (number_of_channels_ == 4) {
      switch (sampler) {
        case PixelSampler::Nearest:
          nearest_interpolation_color(buffer_, nullptr, output, x, y);
          break;
        case PixelSampler::Bilinear:
          bilinear_interpolation_color(buffer_, nullptr, output, x, y);
          break;
        case PixelSampler::Bicubic:
          bicubic_interpolation_color(buffer_, nullptr, output, x, y);
          break;
      }
    }
    else {
      int yi = y;
      int xi = x;
      if (xi < 0 || yi < 0 || uint(xi) >= this->get_width() || uint(yi) >= this->get_height()) {
        zero_v4(output);
      }
      else {
        int offset = (yi * this->get_width() + xi) * 3;
        copy_v3_v3(output, &image_float_buffer_[offset]);
      }
    }
  }
}

void MultilayerValueOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler /*sampler*/)
{
  if (image_float_buffer_ == nullptr) {
    output[0] = 0.0f;
  }
  else {
    int yi = y;
    int xi = x;
    if (xi < 0 || yi < 0 || uint(xi) >= this->get_width() || uint(yi) >= this->get_height()) {
      output[0] = 0.0f;
    }
    else {
      float result = image_float_buffer_[yi * this->get_width() + xi];
      output[0] = result;
    }
  }
}

void MultilayerVectorOperation::execute_pixel_sampled(float output[4],
                                                      float x,
                                                      float y,
                                                      PixelSampler /*sampler*/)
{
  if (image_float_buffer_ == nullptr) {
    output[0] = 0.0f;
  }
  else {
    int yi = y;
    int xi = x;
    if (xi < 0 || yi < 0 || uint(xi) >= this->get_width() || uint(yi) >= this->get_height()) {
      output[0] = 0.0f;
    }
    else {
      int offset = (yi * this->get_width() + xi) * 3;
      copy_v3_v3(output, &image_float_buffer_[offset]);
    }
  }
}

}  // namespace blender::compositor
