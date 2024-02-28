/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CompositorOperation.h"

#include "BLI_string.h"

#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_scene.hh"

#include "IMB_imbuf.hh"

#include "RE_pipeline.h"

namespace blender::compositor {

CompositorOperation::CompositorOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);

  this->set_render_data(nullptr);
  output_buffer_ = nullptr;

  use_alpha_input_ = false;
  active_ = false;

  scene_ = nullptr;
  scene_name_[0] = '\0';
  view_name_ = nullptr;

  flags_.use_render_border = true;
}

void CompositorOperation::init_execution()
{
  if (!active_) {
    return;
  }

  /* When initializing the tree during initial load the width and height can be zero. */
  if (this->get_width() * this->get_height() != 0) {
    output_buffer_ = (float *)MEM_callocN(
        sizeof(float[4]) * this->get_width() * this->get_height(), "CompositorOperation");
  }
}

void CompositorOperation::deinit_execution()
{
  if (!active_) {
    return;
  }

  if (!is_braked()) {
    Render *re = RE_GetSceneRender(scene_);
    RenderResult *rr = RE_AcquireResultWrite(re);

    if (rr) {
      RenderView *rv = RE_RenderViewGetByName(rr, view_name_);
      ImBuf *ibuf = RE_RenderViewEnsureImBuf(rr, rv);

      IMB_assign_float_buffer(ibuf, output_buffer_, IB_TAKE_OWNERSHIP);

      rr->have_combined = true;
    }
    else {
      if (output_buffer_) {
        MEM_freeN(output_buffer_);
      }
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_R_RESULT, "Render Result");
    BKE_image_partial_update_mark_full_update(image);
    BLI_thread_lock(LOCK_DRAW_IMAGE);
    BKE_image_signal(G.main, image, nullptr, IMA_SIGNAL_FREE);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }
  else {
    if (output_buffer_) {
      MEM_freeN(output_buffer_);
    }
  }

  output_buffer_ = nullptr;
}

void CompositorOperation::set_scene_name(const char *scene_name)
{
  STRNCPY(scene_name_, scene_name);
}

void CompositorOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  if (!output_buffer_) {
    return;
  }
  MemoryBuffer output_buf(output_buffer_, COM_DATA_TYPE_COLOR_CHANNELS, get_width(), get_height());
  output_buf.copy_from(inputs[0], area);
  if (use_alpha_input_) {
    output_buf.copy_from(inputs[1], area, 0, COM_DATA_TYPE_VALUE_CHANNELS, 3);
  }
}

void CompositorOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  int width, height;
  BKE_render_resolution(rd_, false, &width, &height);

  /* Check actual render resolution with cropping it may differ with cropped border.rendering
   * Fix for #31777 Border Crop gives black (easy). */
  Render *re = RE_GetSceneRender(scene_);
  if (re) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr) {
      width = rr->rectx;
      height = rr->recty;
    }
    RE_ReleaseResult(re);
  }

  rcti local_preferred;
  BLI_rcti_init(&local_preferred, 0, width, 0, height);

  set_determined_canvas_modifier([&](rcti &canvas) { canvas = local_preferred; });
  NodeOperation::determine_canvas(local_preferred, r_area);
}

}  // namespace blender::compositor
