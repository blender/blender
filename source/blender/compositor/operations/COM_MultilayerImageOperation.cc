/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultilayerImageOperation.h"

#include "BLI_string.h"

#include "IMB_interp.hh"

namespace blender::compositor {

ImBuf *MultilayerBaseOperation::get_im_buf()
{
  if (rd_ == nullptr || image_ == nullptr) {
    return nullptr;
  }

  ImBuf *ibuf = BKE_image_acquire_multilayer_view_ibuf(
      *rd_, *image_, image_user_, pass_name_.c_str(), view_name_);
  if (ibuf == nullptr || (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr))
  {
    BKE_image_release_ibuf(image_, ibuf, nullptr);
    return nullptr;
  }
  return ibuf;
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
