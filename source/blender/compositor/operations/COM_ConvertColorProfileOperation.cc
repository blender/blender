/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvertColorProfileOperation.h"

#include "IMB_imbuf.hh"

namespace blender::compositor {

ConvertColorProfileOperation::ConvertColorProfileOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  predivided_ = false;
}

}  // namespace blender::compositor
