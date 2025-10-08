/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_math_vector.hh"

#include "BKE_bvhutils.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

void get_closest_in_bvhtree(bke::BVHTreeFromMesh &tree_data,
                            const VArray<float3> &positions,
                            const IndexMask &mask,
                            const MutableSpan<int> r_indices,
                            const MutableSpan<float> r_distances_sq,
                            const MutableSpan<float3> r_positions)
{
  BLI_assert(positions.size() >= r_indices.size());
  BLI_assert(positions.size() >= r_distances_sq.size());
  BLI_assert(positions.size() >= r_positions.size());

  mask.foreach_index([&](const int i) {
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    const float3 position = positions[i];
    BLI_bvhtree_find_nearest(
        tree_data.tree, position, &nearest, tree_data.nearest_callback, &tree_data);
    if (!r_indices.is_empty()) {
      r_indices[i] = nearest.index;
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = nearest.dist_sq;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = nearest.co;
    }
  });
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_sample_nearest_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::PointCloud})
      .description("Mesh or point cloud to find the nearest point on");
  b.add_input<decl::Vector>("Sample Position").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_output<decl::Int>("Index").dependent_field({1});
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
  node->custom2 = int(AttrDomain::Point);
}

static void get_closest_pointcloud_points(const bke::BVHTreeFromPointCloud &tree_data,
                                          const VArray<float3> &positions,
                                          const IndexMask &mask,
                                          MutableSpan<int> r_indices,
                                          MutableSpan<float> r_distances_sq)
{
  BLI_assert(positions.size() >= r_indices.size());
  if (tree_data.tree == nullptr) {
    r_indices.fill(0);
    r_distances_sq.fill(0.0f);
    return;
  }

  mask.foreach_index([&](const int i) {
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    const float3 position = positions[i];
    BLI_bvhtree_find_nearest(tree_data.tree,
                             position,
                             &nearest,
                             tree_data.nearest_callback,
                             &const_cast<bke::BVHTreeFromPointCloud &>(tree_data));
    r_indices[i] = nearest.index;
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = nearest.dist_sq;
    }
  });
}

static void get_closest_mesh_points(const Mesh &mesh,
                                    const VArray<float3> &positions,
                                    const IndexMask &mask,
                                    const MutableSpan<int> r_point_indices,
                                    const MutableSpan<float> r_distances_sq,
                                    const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.verts_num > 0);
  bke::BVHTreeFromMesh tree_data = mesh.bvh_verts();
  get_closest_in_bvhtree(tree_data, positions, mask, r_point_indices, r_distances_sq, r_positions);
}

static void get_closest_mesh_edges(const Mesh &mesh,
                                   const VArray<float3> &positions,
                                   const IndexMask &mask,
                                   const MutableSpan<int> r_edge_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.edges_num > 0);
  bke::BVHTreeFromMesh tree_data = mesh.bvh_edges();
  get_closest_in_bvhtree(tree_data, positions, mask, r_edge_indices, r_distances_sq, r_positions);
}

static void get_closest_mesh_tris(const Mesh &mesh,
                                  const VArray<float3> &positions,
                                  const IndexMask &mask,
                                  const MutableSpan<int> r_tri_indices,
                                  const MutableSpan<float> r_distances_sq,
                                  const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.faces_num > 0);
  bke::BVHTreeFromMesh tree_data = mesh.bvh_corner_tris();
  get_closest_in_bvhtree(tree_data, positions, mask, r_tri_indices, r_distances_sq, r_positions);
}

static void get_closest_mesh_faces(const Mesh &mesh,
                                   const VArray<float3> &positions,
                                   const IndexMask &mask,
                                   const MutableSpan<int> r_face_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.faces_num > 0);

  Array<int> tri_indices(positions.size());
  get_closest_mesh_tris(mesh, positions, mask, tri_indices, r_distances_sq, r_positions);

  const Span<int> tri_faces = mesh.corner_tri_faces();

  mask.foreach_index([&](const int i) { r_face_indices[i] = tri_faces[tri_indices[i]]; });
}

/* The closest corner is defined to be the closest corner on the closest face. */
static void get_closest_mesh_corners(const Mesh &mesh,
                                     const VArray<float3> &positions,
                                     const IndexMask &mask,
                                     const MutableSpan<int> r_corner_indices,
                                     const MutableSpan<float> r_distances_sq,
                                     const MutableSpan<float3> r_positions)
{
  const Span<float3> vert_positions = mesh.vert_positions();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  BLI_assert(mesh.corners_num > 0);
  Array<int> face_indices(positions.size());
  get_closest_mesh_faces(mesh, positions, mask, face_indices, {}, {});

  mask.foreach_index([&](const int i) {
    const float3 position = positions[i];
    const int face_index = face_indices[i];

    /* Find the closest vertex in the face. */
    float min_distance_sq = FLT_MAX;
    int closest_vert = 0;
    int closest_corner = 0;
    for (const int corner : faces[face_index]) {
      const int vert = corner_verts[corner];
      const float distance_sq = math::distance_squared(position, vert_positions[vert]);
      if (distance_sq < min_distance_sq) {
        min_distance_sq = distance_sq;
        closest_corner = corner;
        closest_vert = vert;
      }
    }
    if (!r_corner_indices.is_empty()) {
      r_corner_indices[i] = closest_corner;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = vert_positions[closest_vert];
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = min_distance_sq;
    }
  });
}

static bool component_is_available(const GeometrySet &geometry,
                                   const GeometryComponent::Type type,
                                   const AttrDomain domain)
{
  if (!geometry.has(type)) {
    return false;
  }
  const GeometryComponent &component = *geometry.get_component(type);
  return component.attribute_domain_size(domain) != 0;
}

static const GeometryComponent *find_source_component(const GeometrySet &geometry,
                                                      const AttrDomain domain)
{
  /* Choose the other component based on a consistent order, rather than some more complicated
   * heuristic. This is the same order visible in the spreadsheet and used in the ray-cast node. */
  static const Array<GeometryComponent::Type> supported_types = {
      GeometryComponent::Type::Mesh, GeometryComponent::Type::PointCloud};
  for (const GeometryComponent::Type src_type : supported_types) {
    if (component_is_available(geometry, src_type, domain)) {
      return geometry.get_component(src_type);
    }
  }

  return nullptr;
}

class SampleNearestFunction : public mf::MultiFunction {
  GeometrySet source_;
  AttrDomain domain_;

  const GeometryComponent *src_component_;

  /* Point clouds do not cache BVH trees currently; avoid rebuilding it on every call. */
  bke::BVHTreeFromPointCloud pointcloud_bvh = {};

  mf::Signature signature_;

 public:
  SampleNearestFunction(GeometrySet geometry, AttrDomain domain)
      : source_(std::move(geometry)), domain_(domain)
  {
    source_.ensure_owns_direct_data();
    this->src_component_ = find_source_component(source_, domain_);
    if (src_component_ && src_component_->type() == bke::GeometryComponent::Type::PointCloud) {
      const PointCloudComponent &component = *static_cast<const PointCloudComponent *>(
          src_component_);
      const PointCloud &points = *component.get();
      pointcloud_bvh = bke::bvhtree_from_pointcloud_get(points, IndexMask(points.totpoint));
    }

    mf::SignatureBuilder builder{"Sample Nearest", signature_};
    builder.single_input<float3>("Position");
    builder.single_output<int>("Index");
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    MutableSpan<int> indices = params.uninitialized_single_output<int>(1, "Index");
    if (!src_component_) {
      index_mask::masked_fill(indices, 0, mask);
      return;
    }

    switch (src_component_->type()) {
      case GeometryComponent::Type::Mesh: {
        const MeshComponent &component = *static_cast<const MeshComponent *>(src_component_);
        const Mesh &mesh = *component.get();
        switch (domain_) {
          case AttrDomain::Point:
            get_closest_mesh_points(mesh, positions, mask, indices, {}, {});
            break;
          case AttrDomain::Edge:
            get_closest_mesh_edges(mesh, positions, mask, indices, {}, {});
            break;
          case AttrDomain::Face:
            get_closest_mesh_faces(mesh, positions, mask, indices, {}, {});
            break;
          case AttrDomain::Corner:
            get_closest_mesh_corners(mesh, positions, mask, indices, {}, {});
            break;
          default:
            break;
        }
        break;
      }
      case GeometryComponent::Type::PointCloud: {
        get_closest_pointcloud_points(pointcloud_bvh, positions, mask, indices, {});
        break;
      }
      default:
        break;
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  const AttrDomain domain = AttrDomain(params.node().custom2);
  if (geometry.has_curves() && !geometry.has_mesh() && !geometry.has_pointcloud()) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("The source geometry must contain a mesh or a point cloud"));
    params.set_default_remaining_outputs();
    return;
  }

  auto sample_position = params.extract_input<bke::SocketValueVariant>("Sample Position");

  std::string error_message;
  bke::SocketValueVariant index;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<SampleNearestFunction>(std::move(geometry), domain),
          {&sample_position},
          {&index},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Index", std::move(index));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_only_mesh_items,
                    NOD_inline_enum_accessors(custom2),
                    int(AttrDomain::Point));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleNearest", GEO_NODE_SAMPLE_NEAREST);
  ntype.ui_name = "Sample Nearest";
  ntype.ui_description =
      "Find the element of a geometry closest to a position. Similar to the \"Index of Nearest\" "
      "node";
  ntype.enum_name_legacy = "SAMPLE_NEAREST";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_nearest_cc
