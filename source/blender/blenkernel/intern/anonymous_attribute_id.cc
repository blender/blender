/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"

namespace blender::bke {

bool AnonymousAttributePropagationInfo::propagate(const StringRef attribute) const
{
  if (this->propagate_all) {
    return true;
  }
  if (!this->names) {
    return false;
  }
  return this->names->contains_as(attribute);
}

}  // namespace blender::bke
