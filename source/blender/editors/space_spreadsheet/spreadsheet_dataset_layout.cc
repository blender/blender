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

#include <optional>

#include "BLI_span.hh"

#include "BLT_translation.h"

#include "spreadsheet_dataset_layout.hh"

namespace blender::ed::spreadsheet {

#define ATTR_INFO(type, label, icon) \
  std::optional<DatasetAttrDomainLayoutInfo> \
  { \
    std::in_place, type, label, icon \
  }
#define ATTR_INFO_NONE(type) \
  { \
    std::nullopt \
  }

/**
 * Definition for the component->attribute-domain hierarchy.
 * Constructed at compile time.
 *
 * \warning Order of attribute-domains matters! It __must__ match the #AttributeDomain
 * definition and fill gaps with unset optionals (i.e. `std::nullopt`). Would be nice to use
 * array designators for this (which C++ doesn't support).
 */
constexpr DatasetComponentLayoutInfo DATASET_layout_hierarchy[] = {
    {
        GEO_COMPONENT_TYPE_MESH,
        N_("Mesh"),
        ICON_MESH_DATA,
        {
            ATTR_INFO(ATTR_DOMAIN_POINT, N_("Vertex"), ICON_VERTEXSEL),
            ATTR_INFO(ATTR_DOMAIN_EDGE, N_("Edge"), ICON_EDGESEL),
            ATTR_INFO(ATTR_DOMAIN_FACE, N_("Face"), ICON_FACESEL),
            ATTR_INFO(ATTR_DOMAIN_CORNER, N_("Face Corner"), ICON_NODE_CORNER),
        },
    },
    {
        GEO_COMPONENT_TYPE_CURVE,
        N_("Curves"),
        ICON_CURVE_DATA,
        {
            ATTR_INFO(ATTR_DOMAIN_POINT, N_("Control Point"), ICON_CURVE_BEZCIRCLE),
            ATTR_INFO_NONE(ATTR_DOMAIN_EDGE),
            ATTR_INFO_NONE(ATTR_DOMAIN_CORNER),
            ATTR_INFO_NONE(ATTR_DOMAIN_FACE),
            ATTR_INFO(ATTR_DOMAIN_CURVE, N_("Spline"), ICON_CURVE_PATH),
        },
    },
    {
        GEO_COMPONENT_TYPE_POINT_CLOUD,
        N_("Point Cloud"),
        ICON_POINTCLOUD_DATA,
        {
            ATTR_INFO(ATTR_DOMAIN_POINT, N_("Point"), ICON_PARTICLE_POINT),
        },
    },
    {
        GEO_COMPONENT_TYPE_VOLUME,
        N_("Volume Grids"),
        ICON_VOLUME_DATA,
        {},
    },
    {
        GEO_COMPONENT_TYPE_INSTANCES,
        N_("Instances"),
        ICON_EMPTY_AXIS,
        {},
    },
};

#undef ATTR_INFO
#undef ATTR_INFO_LABEL

DatasetLayoutHierarchy dataset_layout_hierarchy()
{
  return DatasetLayoutHierarchy{
      Span{DATASET_layout_hierarchy, ARRAY_SIZE(DATASET_layout_hierarchy)}};
}

#ifndef NDEBUG
/**
 * Debug-only sanity check for correct attribute domain initialization (order/indices must
 * match AttributeDomain). This doesn't check for all possible missuses, but should catch the most
 * likely mistakes.
 */
void dataset_layout_hierarchy_sanity_check(const DatasetLayoutHierarchy &hierarchy)
{
  for (const DatasetComponentLayoutInfo &component : hierarchy.components) {
    for (uint i = 0; i < component.attr_domains.size(); i++) {
      if (component.attr_domains[i]) {
        BLI_assert(component.attr_domains[i]->type == static_cast<AttributeDomain>(i));
      }
    }
  }
}
#endif

}  // namespace blender::ed::spreadsheet
