/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetColorOperation.h"

namespace blender::compositor {

SetColorOperation::SetColorOperation()
{
  this->add_output_socket(DataType::Color);
  flags_.is_set_operation = true;
}

void SetColorOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

}  // namespace blender::compositor
