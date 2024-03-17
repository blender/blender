/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_PlaneDistortCommonOperation.h"

namespace blender::compositor {

class PlaneCornerPinMaskOperation : public PlaneDistortMaskOperation {
 public:
  PlaneCornerPinMaskOperation();

  void init_data() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

class PlaneCornerPinWarpImageOperation : public PlaneDistortWarpImageOperation {
 public:
  PlaneCornerPinWarpImageOperation();

  void init_data() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
};

}  // namespace blender::compositor
