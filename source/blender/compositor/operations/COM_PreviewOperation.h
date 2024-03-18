/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_global.h"
#include "BLI_rect.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_color_types.h"
#include "DNA_image_types.h"

namespace blender::compositor {

class PreviewOperation : public MultiThreadedOperation {
 protected:
  ImBuf *output_image_;

  /**
   * \brief holds reference to the SDNA bNode, where this nodes will render the preview image for
   */
  bNodePreview *preview_;
  SocketReader *input_;
  float divider_;
  unsigned int default_width_;
  unsigned int default_height_;

  const ColorManagedViewSettings *view_settings_;
  const ColorManagedDisplaySettings *display_settings_;

 public:
  PreviewOperation(const ColorManagedViewSettings *view_settings,
                   const ColorManagedDisplaySettings *display_settings,
                   unsigned int default_width,
                   unsigned int default_height);
  void verify_preview(bNodeInstanceHash *previews, bNodeInstanceKey key);

  bool is_output_operation(bool /*rendering*/) const override
  {
    return !G.background;
  }
  void init_execution() override;
  void deinit_execution() override;
  eCompositorPriority get_render_priority() const override;

  void execute_region(rcti *rect, unsigned int tile_number) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
