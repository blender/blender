/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetSamplerOperation.h"

namespace blender::compositor {

SetSamplerOperation::SetSamplerOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

}  // namespace blender::compositor
