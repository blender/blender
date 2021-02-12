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

#include <string.h>

#include "BLI_float3.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "NOD_geometry.h"
#include "NOD_geometry_exec.hh"

#include "node_util.h"

void geo_node_type_base(
    struct bNodeType *ntype, int type, const char *name, short nclass, short flag);
bool geo_node_poll_default(struct bNodeType *ntype, struct bNodeTree *ntree);

namespace blender::nodes {
void update_attribute_input_socket_availabilities(bNode &node,
                                                  const StringRef name,
                                                  const GeometryNodeAttributeInputMode mode,
                                                  const bool name_is_available = true);

CustomDataType attribute_data_type_highest_complexity(Span<CustomDataType>);
AttributeDomain attribute_domain_highest_priority(Span<AttributeDomain> domains);

Array<uint32_t> get_geometry_element_ids_as_uints(const GeometryComponent &component,
                                                  const AttributeDomain domain);

GeometrySet geometry_set_realize_instances(const GeometrySet &geometry_set);

struct AttributeInfo {
  CustomDataType data_type;
  AttributeDomain domain;
};

/**
 * Add information about all the attributes on every component of the type. The resulting info
 * will contain the highest complexity data type and the highest priority domain among every
 * attribute with the given name on all of the input components.
 */
void gather_attribute_info(Map<std::string, AttributeInfo> &attributes,
                           const GeometryComponentType component_type,
                           Span<GeometryInstanceGroup> set_groups,
                           const Set<std::string> &ignored_attributes);

}  // namespace blender::nodes
