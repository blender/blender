/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_register.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "node_util.hh"

struct BVHTreeFromMesh;
struct GeometrySet;
namespace blender::nodes {
class GatherAddNodeSearchParams;
class GatherLinkSearchOpParams;
}  // namespace blender::nodes

void geo_node_type_base(blender::bke::bNodeType *ntype, int type, const char *name, short nclass);
bool geo_node_poll_default(const blender::bke::bNodeType *ntype,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint);

namespace blender::nodes {

bool check_tool_context_and_error(GeoNodeExecParams &params);
void search_link_ops_for_tool_node(GatherLinkSearchOpParams &params);
void search_link_ops_for_volume_grid_node(GatherLinkSearchOpParams &params);

void get_closest_in_bvhtree(BVHTreeFromMesh &tree_data,
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

const EnumPropertyItem *domain_experimental_grease_pencil_version3_fn(bContext * /*C*/,
                                                                      PointerRNA * /*ptr*/,
                                                                      PropertyRNA * /*prop*/,
                                                                      bool *r_free);

const EnumPropertyItem *domain_without_corner_experimental_grease_pencil_version3_fn(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free);

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
