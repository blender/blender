/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2014, Blender Foundation.
 */

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
