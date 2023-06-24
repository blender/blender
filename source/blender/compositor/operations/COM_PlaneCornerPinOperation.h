/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "COM_PlaneDistortCommonOperation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

namespace blender::compositor {

class PlaneCornerPinMaskOperation : public PlaneDistortMaskOperation {
 private:
  /* TODO(manzanilla): to be removed with tiled implementation. */
  bool corners_ready_;

 public:
  PlaneCornerPinMaskOperation();

  void init_data() override;
  void init_execution() override;
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

class PlaneCornerPinWarpImageOperation : public PlaneDistortWarpImageOperation {
 private:
  bool corners_ready_;

 public:
  PlaneCornerPinWarpImageOperation();

  void init_data() override;
  void init_execution() override;
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

}  // namespace blender::compositor
