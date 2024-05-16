/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultilayerImageOperation.h"

#include "BLI_string.h"

#include "IMB_interp.hh"

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
  image_user_.view = view_;
  image_user_.pass = pass_id_;

  if (BKE_image_multilayer_index(image_->rr, &image_user_)) {
    return BaseImageOperation::get_im_buf();
  }

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

}  // namespace blender::compositor
