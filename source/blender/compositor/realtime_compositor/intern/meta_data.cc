/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_meta_data.hh"

namespace blender::realtime_compositor {

bool MetaData::is_cryptomatte_layer() const
{
  return !this->cryptomatte.manifest.empty() || !this->cryptomatte.hash.empty() ||
         !this->cryptomatte.conversion.empty();
}

}  // namespace blender::realtime_compositor
