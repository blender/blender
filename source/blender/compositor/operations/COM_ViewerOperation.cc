/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ViewerOperation.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

ViewerOperation::ViewerOperation()
{
  this->set_image(nullptr);
  this->set_image_user(nullptr);
  output_buffer_ = nullptr;
  active_ = false;
  view_settings_ = nullptr;
  display_settings_ = nullptr;
  use_alpha_input_ = false;

  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);

  image_input_ = nullptr;
  alpha_input_ = nullptr;
  rd_ = nullptr;
  view_name_ = nullptr;
  flags_.use_viewer_border = true;
  flags_.is_viewer_operation = true;
}

void ViewerOperation::init_execution()
{
  /* When initializing the tree during initial load the width and height can be zero. */
  image_input_ = get_input_socket_reader(0);
  alpha_input_ = get_input_socket_reader(1);

  if (is_active_viewer_output() && !exec_system_->is_breaked()) {
    init_image();
  }
}

void ViewerOperation::deinit_execution()
{
  image_input_ = nullptr;
  alpha_input_ = nullptr;
  output_buffer_ = nullptr;
}

void ViewerOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  float *buffer = output_buffer_;
  if (!buffer) {
    return;
  }
  const int x1 = rect->xmin;
  const int y1 = rect->ymin;
  const int x2 = rect->xmax;
  const int y2 = rect->ymax;
  const int offsetadd = (this->get_width() - (x2 - x1));
  const int offsetadd4 = offsetadd * 4;
  int offset = (y1 * this->get_width() + x1);
  int offset4 = offset * 4;
  float alpha[4];
  int x;
  int y;
  bool breaked = false;

  for (y = y1; y < y2 && (!breaked); y++) {
    for (x = x1; x < x2; x++) {
      image_input_->read_sampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
      if (use_alpha_input_) {
        alpha_input_->read_sampled(alpha, x, y, PixelSampler::Nearest);
        buffer[offset4 + 3] = alpha[0];
      }

      offset++;
      offset4 += 4;
    }
    if (is_braked()) {
      breaked = true;
    }
    offset += offsetadd;
    offset4 += offsetadd4;
  }
  update_image(rect);
}

void ViewerOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  int scene_render_width, scene_render_height;
  BKE_render_resolution(rd_, false, &scene_render_width, &scene_render_height);

  rcti local_preferred = preferred_area;
  local_preferred.xmax = local_preferred.xmin + scene_render_width;
  local_preferred.ymax = local_preferred.ymin + scene_render_height;

  NodeOperation::determine_canvas(local_preferred, r_area);
}

void ViewerOperation::init_image()
{
  Image *ima = image_;
  ImageUser iuser = *image_user_;
  void *lock;
  ImBuf *ibuf;

  /* make sure the image has the correct number of views */
  if (ima && BKE_scene_multiview_is_render_view_first(rd_, view_name_)) {
    BKE_image_ensure_viewer_views(rd_, ima, image_user_);
  }

  BLI_thread_lock(LOCK_DRAW_IMAGE);

  /* local changes to the original ImageUser */
  iuser.multi_index = BKE_scene_multiview_view_id_get(rd_, view_name_);
  ibuf = BKE_image_acquire_ibuf(ima, &iuser, &lock);

  if (!ibuf) {
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
    return;
  }

  if (ibuf->x != get_width() || ibuf->y != get_height()) {

    imb_freerectImBuf(ibuf);
    imb_freerectfloatImBuf(ibuf);
    ibuf->x = get_width();
    ibuf->y = get_height();
    /* zero size can happen if no image buffers exist to define a sensible resolution */
    if (ibuf->x > 0 && ibuf->y > 0) {
      imb_addrectfloatImBuf(ibuf, 4);
    }

    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  }

  /* now we combine the input with ibuf */
  output_buffer_ = ibuf->float_buffer.data;

  /* needed for display buffer update */
  ibuf_ = ibuf;

  BKE_image_release_ibuf(image_, ibuf_, lock);

  BLI_thread_unlock(LOCK_DRAW_IMAGE);
}

void ViewerOperation::update_image(const rcti *rect)
{
  if (exec_system_->is_breaked()) {
    return;
  }

  image_->offset_x = canvas_.xmin;
  image_->offset_y = canvas_.ymin;
  float *buffer = output_buffer_;
  IMB_partial_display_buffer_update(ibuf_,
                                    buffer,
                                    nullptr,
                                    get_width(),
                                    0,
                                    0,
                                    view_settings_,
                                    display_settings_,
                                    rect->xmin,
                                    rect->ymin,
                                    rect->xmax,
                                    rect->ymax);

  /* This could be improved to use partial updates. For now disabled as the full frame compositor
   * would not use partial frames anymore and the image engine requires more testing. */
  BKE_image_partial_update_mark_full_update(image_);
  this->update_draw();
}

eCompositorPriority ViewerOperation::get_render_priority() const
{
  if (this->is_active_viewer_output()) {
    return eCompositorPriority::High;
  }

  return eCompositorPriority::Low;
}

void ViewerOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  if (!output_buffer_) {
    return;
  }

  MemoryBuffer output_buffer(
      output_buffer_, COM_DATA_TYPE_COLOR_CHANNELS, get_width(), get_height());
  const MemoryBuffer *input_image = inputs[0];
  output_buffer.copy_from(input_image, area);
  if (use_alpha_input_) {
    const MemoryBuffer *input_alpha = inputs[1];
    output_buffer.copy_from(input_alpha, area, 0, COM_DATA_TYPE_VALUE_CHANNELS, 3);
  }

  update_image(&area);
}

void ViewerOperation::clear_display_buffer()
{
  BLI_assert(is_active_viewer_output());
  if (exec_system_->is_breaked()) {
    return;
  }

  init_image();
  if (output_buffer_ == nullptr) {
    return;
  }

  size_t buf_bytes = size_t(ibuf_->y) * ibuf_->x * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float);
  if (buf_bytes > 0) {
    memset(output_buffer_, 0, buf_bytes);
    rcti display_area;
    BLI_rcti_init(&display_area, 0, ibuf_->x, 0, ibuf_->y);
    update_image(&display_area);
  }
}

}  // namespace blender::compositor
