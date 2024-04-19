/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetVectorOperation.h"

namespace blender::compositor {

SetVectorOperation::SetVectorOperation()
{
  this->add_output_socket(DataType::Vector);
  flags_.is_constant_operation = true;
}

void SetVectorOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = preferred_area;
}

}  // namespace blender::compositor
