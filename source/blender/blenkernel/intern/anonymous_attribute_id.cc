/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"

namespace blender::bke {

std::string AnonymousAttributeID::user_name() const
{
  return this->name();
}

bool AnonymousAttributePropagationInfo::propagate(const AnonymousAttributeID &anonymous_id) const
{
  if (this->propagate_all) {
    return true;
  }
  if (!this->names) {
    return false;
  }
  return this->names->contains_as(anonymous_id.name());
}

}  // namespace blender::bke
