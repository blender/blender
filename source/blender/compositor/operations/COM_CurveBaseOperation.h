/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

struct CurveMapping;

namespace blender::compositor {

class CurveBaseOperation : public MultiThreadedOperation {
 protected:
  CurveMapping *curve_mapping_;

 public:
  CurveBaseOperation();
  ~CurveBaseOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_curve_mapping(const CurveMapping *mapping);
};

}  // namespace blender::compositor
