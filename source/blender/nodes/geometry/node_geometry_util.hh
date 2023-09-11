/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "BKE_node.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_register.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "node_util.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/Types.h>
#endif

struct BVHTreeFromMesh;
struct GeometrySet;
namespace blender::nodes {
class GatherAddNodeSearchParams;
class GatherLinkSearchOpParams;
}  // namespace blender::nodes

void geo_node_type_base(bNodeType *ntype, int type, const char *name, short nclass);
bool geo_node_poll_default(const bNodeType *ntype,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint);

namespace blender::nodes {

bool check_tool_context_and_error(GeoNodeExecParams &params);
void search_link_ops_for_tool_node(GatherLinkSearchOpParams &params);

void transform_mesh(Mesh &mesh,
                    const float3 translation,
                    const float3 rotation,
                    const float3 scale);

void transform_geometry_set(GeoNodeExecParams &params,
                            GeometrySet &geometry,
                            const float4x4 &transform,
                            const Depsgraph &depsgraph);

/**
 * Returns the parts of the geometry that are on the selection for the given domain. If the domain
 * is not applicable for the component, e.g. face domain for point cloud, nothing happens to that
 * component. If no component can work with the domain, then `error_message` is set to true.
 */
void separate_geometry(GeometrySet &geometry_set,
                       eAttrDomain domain,
                       GeometryNodeDeleteGeometryMode mode,
                       const Field<bool> &selection_field,
                       const AnonymousAttributePropagationInfo &propagation_info,
                       bool &r_is_error);

void get_closest_in_bvhtree(BVHTreeFromMesh &tree_data,
                            const VArray<float3> &positions,
                            const IndexMask &mask,
                            const MutableSpan<int> r_indices,
                            const MutableSpan<float> r_distances_sq,
                            const MutableSpan<float3> r_positions);

int apply_offset_in_cyclic_range(IndexRange range, int start_index, int offset);

std::optional<eCustomDataType> node_data_type_to_custom_data_type(eNodeSocketDatatype type);
std::optional<eCustomDataType> node_socket_to_custom_data_type(const bNodeSocket &socket);

#ifdef WITH_OPENVDB
/**
 * Initializes the VolumeComponent of a GeometrySet with a new Volume from points.
 * The grid class should be either openvdb::GRID_FOG_VOLUME or openvdb::GRID_LEVEL_SET.
 */
void initialize_volume_component_from_points(GeoNodeExecParams &params,
                                             const NodeGeometryPointsToVolume &storage,
                                             GeometrySet &r_geometry_set,
                                             openvdb::GridClass gridClass);
#endif

class EvaluateAtIndexInput final : public bke::GeometryFieldInput {
 private:
  Field<int> index_field_;
  GField value_field_;
  eAttrDomain value_field_domain_;

 public:
  EvaluateAtIndexInput(Field<int> index_field, GField value_field, eAttrDomain value_field_domain);

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;

  std::optional<eAttrDomain> preferred_domain(const GeometryComponent & /*component*/) const final
  {
    return value_field_domain_;
  }
};

std::string socket_identifier_for_simulation_item(const NodeSimulationItem &item);

void socket_declarations_for_simulation_items(Span<NodeSimulationItem> items,
                                              NodeDeclaration &r_declaration);
const CPPType &get_simulation_item_cpp_type(eNodeSocketDatatype socket_type);
const CPPType &get_simulation_item_cpp_type(const NodeSimulationItem &item);

bke::bake::BakeState move_values_to_simulation_state(
    const Span<NodeSimulationItem> node_simulation_items, const Span<void *> input_values);
void move_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     bke::bake::BakeState zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &sim_output_node,
                                     Span<void *> r_output_values);
void copy_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     const bke::bake::BakeStateRef &zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &sim_output_node,
                                     Span<void *> r_output_values);

void copy_with_checked_indices(const GVArray &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               GMutableSpan dst);

void socket_declarations_for_repeat_items(const Span<NodeRepeatItem> items,
                                          NodeDeclaration &r_declaration);

namespace enums {

const EnumPropertyItem *attribute_type_type_with_socket_fn(bContext * /*C*/,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free);

bool generic_attribute_type_supported(const EnumPropertyItem &item);

}  // namespace enums

}  // namespace blender::nodes
