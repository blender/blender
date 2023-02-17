/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "NOD_geometry.h"
#include "NOD_geometry_exec.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "RNA_access.h"

#include "node_geometry_register.hh"
#include "node_util.h"

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
  AutoAnonymousAttributeID top_id;
  AutoAnonymousAttributeID bottom_id;
  AutoAnonymousAttributeID side_id;
  AutoAnonymousAttributeID uv_map_id;
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
 * Copies the point domain attributes from `in_component` that are in the mask to `out_component`.
 */
void copy_point_attributes_based_on_mask(const GeometryComponent &in_component,
                                         GeometryComponent &result_component,
                                         Span<bool> masks,
                                         bool invert);
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
                            const IndexMask mask,
                            const MutableSpan<int> r_indices,
                            const MutableSpan<float> r_distances_sq,
                            const MutableSpan<float3> r_positions);

int apply_offset_in_cyclic_range(IndexRange range, int start_index, int offset);

std::optional<eCustomDataType> node_data_type_to_custom_data_type(eNodeSocketDatatype type);
std::optional<eCustomDataType> node_socket_to_custom_data_type(const bNodeSocket &socket);

class FieldAtIndexInput final : public bke::GeometryFieldInput {
 private:
  Field<int> index_field_;
  GField value_field_;
  eAttrDomain value_field_domain_;

 public:
  FieldAtIndexInput(Field<int> index_field, GField value_field, eAttrDomain value_field_domain);

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask mask) const final;

  std::optional<eAttrDomain> preferred_domain(const GeometryComponent & /*component*/) const final
  {
    return value_field_domain_;
  }
};

}  // namespace blender::nodes
