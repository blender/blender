/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PreviewOperation.h"

#include "BKE_node.hh"
#include "IMB_colormanagement.h"

namespace blender::compositor {

PreviewOperation::PreviewOperation(const ColorManagedViewSettings *view_settings,
                                   const ColorManagedDisplaySettings *display_settings,
                                   const uint default_width,
                                   const uint default_height)

{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  preview_ = nullptr;
  output_buffer_ = nullptr;
  input_ = nullptr;
  divider_ = 1.0f;
  view_settings_ = view_settings;
  display_settings_ = display_settings;
  default_width_ = default_width;
  default_height_ = default_height;
  flags_.use_viewer_border = true;
  flags_.is_preview_operation = true;
}

void PreviewOperation::verify_preview(bNodeInstanceHash *previews, bNodeInstanceKey key)
{
  /* Size (0, 0) ensures the preview rect is not allocated in advance,
   * this is set later in init_execution once the resolution is determined.
   */
  preview_ = blender::bke::node_preview_verify(previews, key, 0, 0, true);
}

void PreviewOperation::init_execution()
{
  input_ = get_input_socket_reader(0);

  if (this->get_width() == uint(preview_->xsize) && this->get_height() == uint(preview_->ysize)) {
    output_buffer_ = preview_->rect;
  }

  if (output_buffer_ == nullptr) {
    output_buffer_ = (uchar *)MEM_callocN(sizeof(uchar) * 4 * get_width() * get_height(),
                                          "PreviewOperation");
    if (preview_->rect) {
      MEM_freeN(preview_->rect);
    }
    preview_->xsize = get_width();
    preview_->ysize = get_height();
    preview_->rect = output_buffer_;
  }
}

void PreviewOperation::deinit_execution()
{
  output_buffer_ = nullptr;
  input_ = nullptr;
}

void PreviewOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  int offset;
  float color[4];
  struct ColormanageProcessor *cm_processor;

  cm_processor = IMB_colormanagement_display_processor_new(view_settings_, display_settings_);

  for (int y = rect->ymin; y < rect->ymax; y++) {
    offset = (y * get_width() + rect->xmin) * 4;
    for (int x = rect->xmin; x < rect->xmax; x++) {
      float rx = floor(x / divider_);
      float ry = floor(y / divider_);

      color[0] = 0.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
      color[3] = 1.0f;
      input_->read_sampled(color, rx, ry, PixelSampler::Nearest);
      IMB_colormanagement_processor_apply_v4(cm_processor, color);
      rgba_float_to_uchar(output_buffer_ + offset, color);
      offset += 4;
    }
  }

  IMB_colormanagement_processor_free(cm_processor);
}
bool PreviewOperation::determine_depending_area_of_interest(rcti *input,
                                                            ReadBufferOperation *read_operation,
                                                            rcti *output)
{
  rcti new_input;

  new_input.xmin = input->xmin / divider_;
  new_input.xmax = input->xmax / divider_;
  new_input.ymin = input->ymin / divider_;
  new_input.ymax = input->ymax / divider_;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}
void PreviewOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  /* Use default preview resolution as preferred ensuring it has size so that
   * generated inputs (which don't have resolution on their own) are displayed */
  BLI_assert(default_width_ > 0 && default_height_ > 0);
  rcti local_preferred;
  BLI_rcti_init(&local_preferred, 0, default_width_, 0, default_height_);
  NodeOperation::determine_canvas(local_preferred, r_area);

  /* If resolution is 0 there are two possible scenarios:
   * - Either node is not connected at all
   * - Or it is connected to an input which has no resolution.
   *
   * In the former case we rely on the execution system to not evaluate this node.
   *
   * The latter case would only happen if an input doesn't set any resolution ignoring output
   * preferred resolution. In such case preview size will be 0 too.
   */
  int width = BLI_rcti_size_x(&r_area);
  int height = BLI_rcti_size_y(&r_area);
  divider_ = 0.0f;
  if (width > 0 && height > 0) {
    if (width > height) {
      divider_ = float(COM_PREVIEW_SIZE) / (width);
    }
    else {
      divider_ = float(COM_PREVIEW_SIZE) / (height);
    }
  }
  width = width * divider_;
  height = height * divider_;

  BLI_rcti_init(&r_area, r_area.xmin, r_area.xmin + width, r_area.ymin, r_area.ymin + height);
}

eCompositorPriority PreviewOperation::get_render_priority() const
{
  return eCompositorPriority::Low;
}

void PreviewOperation::get_area_of_interest(const int input_idx,
                                            const rcti &output_area,
                                            rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);

  r_input_area.xmin = output_area.xmin / divider_;
  r_input_area.xmax = output_area.xmax / divider_;
  r_input_area.ymin = output_area.ymin / divider_;
  r_input_area.ymax = output_area.ymax / divider_;
}

void PreviewOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];
  struct ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_new(
      view_settings_, display_settings_);

  rcti buffer_area;
  BLI_rcti_init(&buffer_area, 0, this->get_width(), 0, this->get_height());
  BuffersIteratorBuilder<uchar> it_builder(
      output_buffer_, buffer_area, area, COM_data_type_num_channels(DataType::Color));

  for (BuffersIterator<uchar> it = it_builder.build(); !it.is_end(); ++it) {
    const float rx = it.x / divider_;
    const float ry = it.y / divider_;

    float color[4];
    input->read_elem_checked(rx, ry, color);
    IMB_colormanagement_processor_apply_v4(cm_processor, color);
    rgba_float_to_uchar(it.out, color);
  }

  IMB_colormanagement_processor_free(cm_processor);
}

}  // namespace blender::compositor
