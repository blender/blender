/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

class SocketProxyOperation : public NodeOperation {
 public:
  SocketProxyOperation(DataType type, bool use_conversion);

  std::unique_ptr<MetaData> get_meta_data() override;
};

}  // namespace blender::compositor
