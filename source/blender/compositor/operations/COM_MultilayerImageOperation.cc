/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultilayerImageOperation.h"

#include "BLI_string.h"

#include "IMB_interp.hh"

namespace blender::compositor {

int MultilayerBaseOperation::get_view_index() const
{
  if (BLI_listbase_count_at_most(&image_->rr->views, 2) <= 1) {
    return 0;
  }

  const int view_image = image_user_.view;
  const bool is_allview = (view_image == 0); /* if view selected == All (0) */

  if (is_allview) {
    /* Heuristic to match image name with scene names check if the view name exists in the image.
     */
    const int view = BLI_findstringindex(
        &image_->rr->views, view_name_, offsetof(RenderView, name));
    if (view == -1) {
      return 0;
    }
    return view;
  }

  return view_image - 1;
}

ImBuf *MultilayerBaseOperation::get_im_buf()
{
  if (image_ == nullptr) {
    return nullptr;
  }

  const RenderLayer *render_layer = static_cast<const RenderLayer *>(
      BLI_findlink(&image_->rr->layers, image_user_.layer));

  image_user_.view = get_view_index();
  image_user_.pass = BLI_findstringindex(
      &render_layer->passes, pass_name_.c_str(), offsetof(RenderPass, name));

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
  /* TODO: Make access to the render result thread-safe. */
  RenderResult *render_result = image_->rr;
  if (render_result && render_result->stamp_data) {
    const RenderLayer *render_layer = static_cast<const RenderLayer *>(
        BLI_findlink(&image_->rr->layers, image_user_.layer));
    std::string full_layer_name = std::string(render_layer->name) + "." + pass_name_;
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
