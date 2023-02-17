/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_bvhutils.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

void get_closest_in_bvhtree(BVHTreeFromMesh &tree_data,
                            const VArray<float3> &positions,
                            const IndexMask mask,
                            const MutableSpan<int> r_indices,
                            const MutableSpan<float> r_distances_sq,
                            const MutableSpan<float3> r_positions)
{
  BLI_assert(positions.size() >= r_indices.size());
  BLI_assert(positions.size() >= r_distances_sq.size());
  BLI_assert(positions.size() >= r_positions.size());

  for (const int i : mask) {
    BVHTreeNearest nearest;
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
  }
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_sample_nearest_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"))
      .supported_type({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD});
  b.add_input<decl::Vector>(N_("Sample Position")).implicit_field(implicit_field_inputs::position);
  b.add_output<decl::Int>(N_("Index")).dependent_field({1});
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
  node->custom2 = ATTR_DOMAIN_POINT;
}

static void get_closest_pointcloud_points(const PointCloud &pointcloud,
                                          const VArray<float3> &positions,
                                          const IndexMask mask,
                                          const MutableSpan<int> r_indices,
                                          const MutableSpan<float> r_distances_sq)
{
  BLI_assert(positions.size() >= r_indices.size());
  BLI_assert(pointcloud.totpoint > 0);

  BVHTreeFromPointCloud tree_data;
  BKE_bvhtree_from_pointcloud_get(&tree_data, &pointcloud, 2);

  for (const int i : mask) {
    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    const float3 position = positions[i];
    BLI_bvhtree_find_nearest(
        tree_data.tree, position, &nearest, tree_data.nearest_callback, &tree_data);
    r_indices[i] = nearest.index;
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = nearest.dist_sq;
    }
  }

  free_bvhtree_from_pointcloud(&tree_data);
}

static void get_closest_mesh_points(const Mesh &mesh,
                                    const VArray<float3> &positions,
                                    const IndexMask mask,
                                    const MutableSpan<int> r_point_indices,
                                    const MutableSpan<float> r_distances_sq,
                                    const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totvert > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_VERTS, 2);
  get_closest_in_bvhtree(tree_data, positions, mask, r_point_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_edges(const Mesh &mesh,
                                   const VArray<float3> &positions,
                                   const IndexMask mask,
                                   const MutableSpan<int> r_edge_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totedge > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_EDGES, 2);
  get_closest_in_bvhtree(tree_data, positions, mask, r_edge_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_looptris(const Mesh &mesh,
                                      const VArray<float3> &positions,
                                      const IndexMask mask,
                                      const MutableSpan<int> r_looptri_indices,
                                      const MutableSpan<float> r_distances_sq,
                                      const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
  get_closest_in_bvhtree(
      tree_data, positions, mask, r_looptri_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_polys(const Mesh &mesh,
                                   const VArray<float3> &positions,
                                   const IndexMask mask,
                                   const MutableSpan<int> r_poly_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);

  Array<int> looptri_indices(positions.size());
  get_closest_mesh_looptris(mesh, positions, mask, looptri_indices, r_distances_sq, r_positions);

  const Span<MLoopTri> looptris = mesh.looptris();

  for (const int i : mask) {
    const MLoopTri &looptri = looptris[looptri_indices[i]];
    r_poly_indices[i] = looptri.poly;
  }
}

/* The closest corner is defined to be the closest corner on the closest face. */
static void get_closest_mesh_corners(const Mesh &mesh,
                                     const VArray<float3> &positions,
                                     const IndexMask mask,
                                     const MutableSpan<int> r_corner_indices,
                                     const MutableSpan<float> r_distances_sq,
                                     const MutableSpan<float3> r_positions)
{
  const Span<float3> vert_positions = mesh.vert_positions();
  const Span<MPoly> polys = mesh.polys();
  const Span<MLoop> loops = mesh.loops();

  BLI_assert(mesh.totloop > 0);
  Array<int> poly_indices(positions.size());
  get_closest_mesh_polys(mesh, positions, mask, poly_indices, {}, {});

  for (const int i : mask) {
    const float3 position = positions[i];
    const int poly_index = poly_indices[i];
    const MPoly &poly = polys[poly_index];

    /* Find the closest vertex in the polygon. */
    float min_distance_sq = FLT_MAX;
    int closest_vert_index = 0;
    int closest_loop_index = 0;
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = loops[loop_index];
      const int vertex_index = loop.v;
      const float distance_sq = math::distance_squared(position, vert_positions[vertex_index]);
      if (distance_sq < min_distance_sq) {
        min_distance_sq = distance_sq;
        closest_loop_index = loop_index;
        closest_vert_index = vertex_index;
      }
    }
    if (!r_corner_indices.is_empty()) {
      r_corner_indices[i] = closest_loop_index;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = vert_positions[closest_vert_index];
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = min_distance_sq;
    }
  }
}

static bool component_is_available(const GeometrySet &geometry,
                                   const GeometryComponentType type,
                                   const eAttrDomain domain)
{
  if (!geometry.has(type)) {
    return false;
  }
  const GeometryComponent &component = *geometry.get_component_for_read(type);
  return component.attribute_domain_size(domain) != 0;
}

static const GeometryComponent *find_source_component(const GeometrySet &geometry,
                                                      const eAttrDomain domain)
{
  /* Choose the other component based on a consistent order, rather than some more complicated
   * heuristic. This is the same order visible in the spreadsheet and used in the ray-cast node. */
  static const Array<GeometryComponentType> supported_types = {GEO_COMPONENT_TYPE_MESH,
                                                               GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                               GEO_COMPONENT_TYPE_CURVE,
                                                               GEO_COMPONENT_TYPE_INSTANCES};
  for (const GeometryComponentType src_type : supported_types) {
    if (component_is_available(geometry, src_type, domain)) {
      return geometry.get_component_for_read(src_type);
    }
  }

  return nullptr;
}

class SampleNearestFunction : public mf::MultiFunction {
  GeometrySet source_;
  eAttrDomain domain_;

  const GeometryComponent *src_component_;

  mf::Signature signature_;

 public:
  SampleNearestFunction(GeometrySet geometry, eAttrDomain domain)
      : source_(std::move(geometry)), domain_(domain)
  {
    source_.ensure_owns_direct_data();
    this->src_component_ = find_source_component(source_, domain_);

    mf::SignatureBuilder builder{"Sample Nearest", signature_};
    builder.single_input<float3>("Position");
    builder.single_output<int>("Index");
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    MutableSpan<int> indices = params.uninitialized_single_output<int>(1, "Index");
    if (!src_component_) {
      indices.fill_indices(mask, 0);
      return;
    }

    switch (src_component_->type()) {
      case GEO_COMPONENT_TYPE_MESH: {
        const MeshComponent &component = *static_cast<const MeshComponent *>(src_component_);
        const Mesh &mesh = *component.get_for_read();
        Array<float> distances(mask.min_array_size());
        switch (domain_) {
          case ATTR_DOMAIN_POINT:
            get_closest_mesh_points(mesh, positions, mask, indices, distances, {});
            break;
          case ATTR_DOMAIN_EDGE:
            get_closest_mesh_edges(mesh, positions, mask, indices, distances, {});
            break;
          case ATTR_DOMAIN_FACE:
            get_closest_mesh_polys(mesh, positions, mask, indices, distances, {});
            break;
          case ATTR_DOMAIN_CORNER:
            get_closest_mesh_corners(mesh, positions, mask, indices, distances, {});
            break;
          default:
            break;
        }
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        const PointCloudComponent &component = *static_cast<const PointCloudComponent *>(
            src_component_);
        const PointCloud &points = *component.get_for_read();
        Array<float> distances(mask.min_array_size());
        get_closest_pointcloud_points(points, positions, mask, indices, distances);
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
  const eAttrDomain domain = eAttrDomain(params.node().custom2);
  if (geometry.has_curves() && !geometry.has_mesh() && !geometry.has_pointcloud()) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("The source geometry must contain a mesh or a point cloud"));
    params.set_default_remaining_outputs();
    return;
  }

  Field<float3> positions = params.extract_input<Field<float3>>("Sample Position");
  auto fn = std::make_shared<SampleNearestFunction>(std::move(geometry), domain);
  auto op = FieldOperation::Create(std::move(fn), {std::move(positions)});
  params.set_output<Field<int>>("Index", Field<int>(std::move(op)));
}

}  // namespace blender::nodes::node_geo_sample_nearest_cc

void register_node_type_geo_sample_nearest()
{
  namespace file_ns = blender::nodes::node_geo_sample_nearest_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SAMPLE_NEAREST, "Sample Nearest", NODE_CLASS_GEOMETRY);
  ntype.initfunc = file_ns::node_init;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
