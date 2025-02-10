/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "MEM_guardedalloc.h"  // IWYU pragma: export

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"  // IWYU pragma: export
#include "BKE_node_socket_value.hh"  // IWYU pragma: export

#include "NOD_geometry_exec.hh"                 // IWYU pragma: export
#include "NOD_register.hh"                      // IWYU pragma: export
#include "NOD_socket_declarations.hh"           // IWYU pragma: export
#include "NOD_socket_declarations_geometry.hh"  // IWYU pragma: export

#include "node_util.hh"  // IWYU pragma: export

namespace blender {
namespace bke {
struct BVHTreeFromMesh;
}
namespace nodes {
class GatherAddNodeSearchParams;
class GatherLinkSearchOpParams;
}  // namespace nodes
}  // namespace blender

void geo_node_type_base(blender::bke::bNodeType *ntype,
                        std::string idname,
                        std::optional<int16_t> legacy_type = std::nullopt);
bool geo_node_poll_default(const blender::bke::bNodeType *ntype,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint);

namespace blender::nodes {

bool check_tool_context_and_error(GeoNodeExecParams &params);
void search_link_ops_for_tool_node(GatherLinkSearchOpParams &params);
void search_link_ops_for_volume_grid_node(GatherLinkSearchOpParams &params);
void search_link_ops_for_import_node(GatherLinkSearchOpParams &params);

void get_closest_in_bvhtree(bke::BVHTreeFromMesh &tree_data,
                            const VArray<float3> &positions,
                            const IndexMask &mask,
                            MutableSpan<int> r_indices,
                            MutableSpan<float> r_distances_sq,
                            MutableSpan<float3> r_positions);

int apply_offset_in_cyclic_range(IndexRange range, int start_index, int offset);

void mix_baked_data_item(eNodeSocketDatatype socket_type,
                         void *prev,
                         const void *next,
                         const float factor);

namespace enums {

const EnumPropertyItem *attribute_type_type_with_socket_fn(bContext * /*C*/,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free);

bool generic_attribute_type_supported(const EnumPropertyItem &item);

}  // namespace enums

bool custom_data_type_supports_grids(eCustomDataType data_type);
const EnumPropertyItem *grid_custom_data_type_items_filter_fn(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_free);
const EnumPropertyItem *grid_socket_type_items_filter_fn(bContext *C,
                                                         PointerRNA *ptr,
                                                         PropertyRNA *prop,
                                                         bool *r_free);

void node_geo_exec_with_missing_openvdb(GeoNodeExecParams &params);

void draw_data_blocks(const bContext *C, uiLayout *layout, PointerRNA &bake_rna);

}  // namespace blender::nodes
