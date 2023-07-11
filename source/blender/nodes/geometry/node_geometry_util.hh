/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "BLI_bounds_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.hh"

#include "NOD_geometry.hh"
#include "NOD_geometry_exec.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "RNA_access.h"

#include "node_geometry_register.hh"
#include "node_util.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/Types.h>
#endif

struct BVHTreeFromMesh;

void geo_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass);
bool geo_node_poll_default(const struct bNodeType *ntype,
                           const struct bNodeTree *ntree,
                           const char **r_disabled_hint);

namespace blender::nodes {

void transform_mesh(Mesh &mesh,
                    const float3 translation,
                    const float3 rotation,
                    const float3 scale);

void transform_geometry_set(GeoNodeExecParams &params,
                            GeometrySet &geometry,
                            const float4x4 &transform,
                            const Depsgraph &depsgraph);

Mesh *create_line_mesh(const float3 start, const float3 delta, int count);

Mesh *create_grid_mesh(
    int verts_x, int verts_y, float size_x, float size_y, const AttributeIDRef &uv_map_id);

struct ConeAttributeOutputs {
  AnonymousAttributeIDPtr top_id;
  AnonymousAttributeIDPtr bottom_id;
  AnonymousAttributeIDPtr side_id;
  AnonymousAttributeIDPtr uv_map_id;
};

Mesh *create_cylinder_or_cone_mesh(float radius_top,
                                   float radius_bottom,
                                   float depth,
                                   int circle_segments,
                                   int side_segments,
                                   int fill_segments,
                                   GeometryNodeMeshCircleFillType fill_type,
                                   ConeAttributeOutputs &attribute_outputs);

/**
 * Calculates the bounds of a radial primitive.
 * The algorithm assumes X-axis symmetry of primitives.
 */
Bounds<float3> calculate_bounds_radial_primitive(float radius_top,
                                                 float radius_bottom,
                                                 int segments,
                                                 float height);

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
void move_values_to_simulation_state(const Span<NodeSimulationItem> node_simulation_items,
                                     const Span<void *> input_values,
                                     bke::sim::SimulationZoneState &r_zone_state);
void move_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     bke::sim::SimulationZoneState &zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &sim_output_node,
                                     Span<void *> r_output_values);
void copy_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     const bke::sim::SimulationZoneState &zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &sim_output_node,
                                     Span<void *> r_output_values);

void copy_with_checked_indices(const GVArray &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               GMutableSpan dst);

}  // namespace blender::nodes
