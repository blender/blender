/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "BKE_attribute_filter.hh"

namespace blender::bke {

/**
 * Checks if the attribute name has the `.a_` prefix which indicates that it is an anonymous
 * attribute. I.e. it is just internally used by Blender and the name should not be exposed to the
 * user.
 *
 * Use #hash_to_anonymous_attribute_name to generate names for anonymous attributes.
 */
inline bool attribute_name_is_anonymous(const StringRef name)
{
  return name.startswith(".a_");
}

class ProcessAllAttributeExceptAnonymous : public AttributeFilter {
 public:
  Result filter(const StringRef name) const override
  {
    if (attribute_name_is_anonymous(name)) {
      return AttributeFilter::Result::AllowSkip;
    }
    return AttributeFilter::Result::Process;
  }
};

}  // namespace blender::bke
