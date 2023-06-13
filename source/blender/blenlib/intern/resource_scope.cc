/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_resource_scope.hh"

namespace blender {

ResourceScope::ResourceScope() = default;

ResourceScope::~ResourceScope()
{
  /* Free in reversed order. */
  for (int64_t i = resources_.size(); i--;) {
    ResourceData &data = resources_[i];
    data.free(data.data);
  }
}

}  // namespace blender
