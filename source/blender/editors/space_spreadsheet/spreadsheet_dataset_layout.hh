/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <array>
#include <optional>

/* Enum definitions... */
#include "BKE_attribute.h"
#include "BKE_geometry_set.h"

#include "BLI_span.hh"

/* More enum definitions... */
#include "UI_resources.h"

#pragma once

namespace blender::ed::spreadsheet {

struct DatasetAttrDomainLayoutInfo {
  AttributeDomain type;
  const char *label;
  BIFIconID icon;

  constexpr DatasetAttrDomainLayoutInfo(AttributeDomain type, const char *label, BIFIconID icon)
      : type(type), label(label), icon(icon)
  {
  }
};

struct DatasetComponentLayoutInfo {
  GeometryComponentType type;
  const char *label;
  BIFIconID icon;
  /** Array of attribute-domains. Has to be fixed size based on #AttributeDomain enum, but not all
   * values need displaying for all parent components. Hence the optional use. */
  using AttrDomainArray = std::array<std::optional<DatasetAttrDomainLayoutInfo>, ATTR_DOMAIN_NUM>;
  const AttrDomainArray attr_domains;
};

struct DatasetLayoutHierarchy {
  /** The components for display (with layout info like icon and label). Each component stores
   * the attribute domains it wants to display (also with layout info like icon and label). */
  const Span<DatasetComponentLayoutInfo> components;
};

DatasetLayoutHierarchy dataset_layout_hierarchy();

#ifndef NDEBUG
void dataset_layout_hierarchy_sanity_check(const DatasetLayoutHierarchy &hierarchy);
#endif

}  // namespace blender::ed::spreadsheet
