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

#include "BLI_kdopbvh.h"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"

#include "FN_generic_array.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

using namespace blender::bke::mesh_surface_sample;
using blender::fn::GArray;

namespace blender::nodes {

static void geo_node_transfer_attribute_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Target");

  b.add_input<decl::Vector>("Attribute").hide_value().supports_field();
  b.add_input<decl::Float>("Attribute", "Attribute_001").hide_value().supports_field();
  b.add_input<decl::Color>("Attribute", "Attribute_002").hide_value().supports_field();
  b.add_input<decl::Bool>("Attribute", "Attribute_003").hide_value().supports_field();
  b.add_input<decl::Int>("Attribute", "Attribute_004").hide_value().supports_field();

  b.add_input<decl::Vector>("Source Position").implicit_field();
  b.add_input<decl::Int>("Index").implicit_field();

  b.add_output<decl::Vector>("Attribute").dependent_field({6, 7});
  b.add_output<decl::Float>("Attribute", "Attribute_001").dependent_field({6, 7});
  b.add_output<decl::Color>("Attribute", "Attribute_002").dependent_field({6, 7});
  b.add_output<decl::Bool>("Attribute", "Attribute_003").dependent_field({6, 7});
  b.add_output<decl::Int>("Attribute", "Attribute_004").dependent_field({6, 7});
}

static void geo_node_transfer_attribute_layout(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  const NodeGeometryTransferAttribute &data = *static_cast<const NodeGeometryTransferAttribute *>(
      node.storage);
  const GeometryNodeAttributeTransferMode mapping = (GeometryNodeAttributeTransferMode)data.mode;

  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "mapping", 0, "", ICON_NONE);
  if (mapping != GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED) {
    uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
  }
}

static void geo_node_transfer_attribute_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryTransferAttribute *data = (NodeGeometryTransferAttribute *)MEM_callocN(
      sizeof(NodeGeometryTransferAttribute), __func__);
  data->data_type = CD_PROP_FLOAT;
  data->mode = GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED;
  node->storage = data;
}

static void geo_node_transfer_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryTransferAttribute &data = *(const NodeGeometryTransferAttribute *)
                                                   node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(data.data_type);
  const GeometryNodeAttributeTransferMode mapping = (GeometryNodeAttributeTransferMode)data.mode;

  bNodeSocket *socket_geometry = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_vector = socket_geometry->next;
  bNodeSocket *socket_float = socket_vector->next;
  bNodeSocket *socket_color4f = socket_float->next;
  bNodeSocket *socket_boolean = socket_color4f->next;
  bNodeSocket *socket_int32 = socket_boolean->next;

  bNodeSocket *socket_positions = socket_int32->next;
  bNodeSocket *socket_indices = socket_positions->next;

  nodeSetSocketAvailability(socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(socket_int32, data_type == CD_PROP_INT32);

  nodeSetSocketAvailability(socket_positions, mapping != GEO_NODE_ATTRIBUTE_TRANSFER_INDEX);
  nodeSetSocketAvailability(socket_indices, mapping == GEO_NODE_ATTRIBUTE_TRANSFER_INDEX);

  bNodeSocket *out_socket_vector = (bNodeSocket *)node->outputs.first;
  bNodeSocket *out_socket_float = out_socket_vector->next;
  bNodeSocket *out_socket_color4f = out_socket_float->next;
  bNodeSocket *out_socket_boolean = out_socket_color4f->next;
  bNodeSocket *out_socket_int32 = out_socket_boolean->next;

  nodeSetSocketAvailability(out_socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(out_socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(out_socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(out_socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(out_socket_int32, data_type == CD_PROP_INT32);
}

static void get_closest_in_bvhtree(BVHTreeFromMesh &tree_data,
                                   const VArray<float3> &positions,
                                   const IndexMask mask,
                                   const MutableSpan<int> r_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(positions.size() == r_indices.size() || r_indices.is_empty());
  BLI_assert(positions.size() == r_distances_sq.size() || r_distances_sq.is_empty());
  BLI_assert(positions.size() == r_positions.size() || r_positions.is_empty());

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

static void get_closest_pointcloud_points(const PointCloud &pointcloud,
                                          const VArray<float3> &positions,
                                          const IndexMask mask,
                                          const MutableSpan<int> r_indices,
                                          const MutableSpan<float> r_distances_sq)
{
  BLI_assert(positions.size() == r_indices.size());
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

static void get_closest_mesh_polygons(const Mesh &mesh,
                                      const VArray<float3> &positions,
                                      const IndexMask mask,
                                      const MutableSpan<int> r_poly_indices,
                                      const MutableSpan<float> r_distances_sq,
                                      const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);

  Array<int> looptri_indices(positions.size());
  get_closest_mesh_looptris(mesh, positions, mask, looptri_indices, r_distances_sq, r_positions);

  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

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
  BLI_assert(mesh.totloop > 0);
  Array<int> poly_indices(positions.size());
  get_closest_mesh_polygons(mesh, positions, mask, poly_indices, {}, {});

  for (const int i : mask) {
    const float3 position = positions[i];
    const int poly_index = poly_indices[i];
    const MPoly &poly = mesh.mpoly[poly_index];

    /* Find the closest vertex in the polygon. */
    float min_distance_sq = FLT_MAX;
    const MVert *closest_mvert;
    int closest_loop_index = 0;
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = mesh.mloop[loop_index];
      const int vertex_index = loop.v;
      const MVert &mvert = mesh.mvert[vertex_index];
      const float distance_sq = float3::distance_squared(position, mvert.co);
      if (distance_sq < min_distance_sq) {
        min_distance_sq = distance_sq;
        closest_loop_index = loop_index;
        closest_mvert = &mvert;
      }
    }
    if (!r_corner_indices.is_empty()) {
      r_corner_indices[i] = closest_loop_index;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = closest_mvert->co;
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = min_distance_sq;
    }
  }
}

template<typename T>
void copy_with_indices(const VArray<T> &src,
                       const IndexMask mask,
                       const Span<int> indices,
                       const MutableSpan<T> dst)
{
  if (src.is_empty()) {
    return;
  }
  for (const int i : mask) {
    dst[i] = src[indices[i]];
  }
}

template<typename T>
void copy_with_indices_clamped(const VArray<T> &src,
                               const IndexMask mask,
                               const Span<int> indices,
                               const MutableSpan<T> dst)
{
  if (src.is_empty()) {
    return;
  }
  const int max_index = src.size() - 1;
  threading::parallel_for(mask.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      const int index = mask[i];
      dst[index] = src[std::clamp(indices[index], 0, max_index)];
    }
  });
}

template<typename T>
void copy_with_indices_and_comparison(const VArray<T> &src_1,
                                      const VArray<T> &src_2,
                                      const Span<float> distances_1,
                                      const Span<float> distances_2,
                                      const IndexMask mask,
                                      const Span<int> indices_1,
                                      const Span<int> indices_2,
                                      const MutableSpan<T> dst)
{
  if (src_1.is_empty() || src_2.is_empty()) {
    return;
  }
  for (const int i : mask) {
    if (distances_1[i] < distances_2[i]) {
      dst[i] = src_1[indices_1[i]];
    }
    else {
      dst[i] = src_2[indices_2[i]];
    }
  }
}

static bool component_is_available(const GeometrySet &geometry,
                                   const GeometryComponentType type,
                                   const AttributeDomain domain)
{
  if (!geometry.has(type)) {
    return false;
  }
  const GeometryComponent &component = *geometry.get_component_for_read(type);
  if (component.is_empty()) {
    return false;
  }
  return component.attribute_domain_size(domain) != 0;
}

/**
 * \note Multi-threading for this function is provided by the field evaluator. Since the #call
 * function could be called many times, calculate the data from the target geometry once and store
 * it for later.
 */
class NearestInterpolatedTransferFunction : public fn::MultiFunction {
  GeometrySet target_;
  GField src_field_;

  /**
   * This function is meant to sample the surface of a mesh rather than take the value from
   * individual elements, so use the most complex domain, ensuring no information is lost. In the
   * future, it should be possible to use the most complex domain required by the field inputs, to
   * simplify sampling and avoid domain conversions.
   */
  AttributeDomain domain_ = ATTR_DOMAIN_CORNER;

  fn::MFSignature signature_;

  std::optional<GeometryComponentFieldContext> target_context_;
  std::unique_ptr<FieldEvaluator> target_evaluator_;
  const GVArray *target_data_;

 public:
  NearestInterpolatedTransferFunction(GeometrySet geometry, GField src_field)
      : target_(std::move(geometry)), src_field_(std::move(src_field))
  {
    target_.ensure_owns_direct_data();
    signature_ = this->create_signature();
    this->set_signature(&signature_);
    this->evaluate_target_field();
  }

  fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Attribute Transfer Nearest Interpolated"};
    signature.single_input<float3>("Position");
    signature.single_output("Attribute", src_field_.cpp_type());
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    GMutableSpan dst = params.uninitialized_single_output_if_required(1, "Attribute");

    const MeshComponent &mesh_component = *target_.get_component_for_read<MeshComponent>();
    BLI_assert(mesh_component.has_mesh());
    const Mesh &mesh = *mesh_component.get_for_read();
    BLI_assert(mesh.totpoly > 0);

    /* Find closest points on the mesh surface. */
    Array<int> looptri_indices(mask.min_array_size());
    Array<float3> sampled_positions(mask.min_array_size());
    get_closest_mesh_looptris(mesh, positions, mask, looptri_indices, {}, sampled_positions);

    MeshAttributeInterpolator interp(&mesh, mask, sampled_positions, looptri_indices);
    interp.sample_data(*target_data_, domain_, eAttributeMapMode::INTERPOLATED, dst);
  }

 private:
  void evaluate_target_field()
  {
    const MeshComponent &mesh_component = *target_.get_component_for_read<MeshComponent>();
    target_context_.emplace(GeometryComponentFieldContext{mesh_component, domain_});
    const int domain_size = mesh_component.attribute_domain_size(domain_);
    target_evaluator_ = std::make_unique<FieldEvaluator>(*target_context_, domain_size);
    target_evaluator_->add(src_field_);
    target_evaluator_->evaluate();
    target_data_ = &target_evaluator_->get_evaluated(0);
  }
};

/**
 * \note Multi-threading for this function is provided by the field evaluator. Since the #call
 * function could be called many times, calculate the data from the target geometry once and store
 * it for later.
 */
class NearestTransferFunction : public fn::MultiFunction {
  GeometrySet target_;
  GField src_field_;
  AttributeDomain domain_;

  fn::MFSignature signature_;

  bool use_mesh_;
  bool use_points_;

  /* Store data from the target as a virtual array, since we may only access a few indices. */
  std::optional<GeometryComponentFieldContext> mesh_context_;
  std::unique_ptr<FieldEvaluator> mesh_evaluator_;
  const GVArray *mesh_data_;

  std::optional<GeometryComponentFieldContext> point_context_;
  std::unique_ptr<FieldEvaluator> point_evaluator_;
  const GVArray *point_data_;

 public:
  NearestTransferFunction(GeometrySet geometry, GField src_field, AttributeDomain domain)
      : target_(std::move(geometry)), src_field_(std::move(src_field)), domain_(domain)
  {
    target_.ensure_owns_direct_data();
    signature_ = this->create_signature();
    this->set_signature(&signature_);

    this->use_mesh_ = component_is_available(target_, GEO_COMPONENT_TYPE_MESH, domain_);
    this->use_points_ = component_is_available(target_, GEO_COMPONENT_TYPE_POINT_CLOUD, domain_);

    this->evaluate_target_field();
  }

  fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Attribute Transfer Nearest"};
    signature.single_input<float3>("Position");
    signature.single_output("Attribute", src_field_.cpp_type());
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    GMutableSpan dst = params.uninitialized_single_output_if_required(1, "Attribute");

    if (!use_mesh_ && !use_points_) {
      dst.type().fill_construct_indices(dst.type().default_value(), dst.data(), mask);
      return;
    }

    const Mesh *mesh = use_mesh_ ? target_.get_mesh_for_read() : nullptr;
    const PointCloud *pointcloud = use_points_ ? target_.get_pointcloud_for_read() : nullptr;

    const int tot_samples = mask.min_array_size();

    Array<int> point_indices;
    Array<float> point_distances;

    /* Depending on where what domain the source attribute lives, these indices are either vertex,
     * corner, edge or polygon indices. */
    Array<int> mesh_indices;
    Array<float> mesh_distances;

    /* If there is a point-cloud, find the closest points. */
    if (use_points_) {
      point_indices.reinitialize(tot_samples);
      if (use_mesh_) {
        point_distances.reinitialize(tot_samples);
      }
      get_closest_pointcloud_points(*pointcloud, positions, mask, point_indices, point_distances);
    }

    /* If there is a mesh, find the closest mesh elements. */
    if (use_mesh_) {
      mesh_indices.reinitialize(tot_samples);
      if (use_points_) {
        mesh_distances.reinitialize(tot_samples);
      }
      switch (domain_) {
        case ATTR_DOMAIN_POINT: {
          get_closest_mesh_points(*mesh, positions, mask, mesh_indices, mesh_distances, {});
          break;
        }
        case ATTR_DOMAIN_EDGE: {
          get_closest_mesh_edges(*mesh, positions, mask, mesh_indices, mesh_distances, {});
          break;
        }
        case ATTR_DOMAIN_FACE: {
          get_closest_mesh_polygons(*mesh, positions, mask, mesh_indices, mesh_distances, {});
          break;
        }
        case ATTR_DOMAIN_CORNER: {
          get_closest_mesh_corners(*mesh, positions, mask, mesh_indices, mesh_distances, {});
          break;
        }
        default: {
          break;
        }
      }
    }

    attribute_math::convert_to_static_type(dst.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if (use_mesh_ && use_points_) {
        GVArray_Typed<T> src_mesh{*mesh_data_};
        GVArray_Typed<T> src_point{*point_data_};
        copy_with_indices_and_comparison(*src_mesh,
                                         *src_point,
                                         mesh_distances,
                                         point_distances,
                                         mask,
                                         mesh_indices,
                                         point_indices,
                                         dst.typed<T>());
      }
      else if (use_points_) {
        GVArray_Typed<T> src_point{*point_data_};
        copy_with_indices(*src_point, mask, point_indices, dst.typed<T>());
      }
      else if (use_mesh_) {
        GVArray_Typed<T> src_mesh{*mesh_data_};
        copy_with_indices(*src_mesh, mask, mesh_indices, dst.typed<T>());
      }
    });
  }

 private:
  void evaluate_target_field()
  {
    if (use_mesh_) {
      const MeshComponent &mesh = *target_.get_component_for_read<MeshComponent>();
      const int domain_size = mesh.attribute_domain_size(domain_);
      mesh_context_.emplace(GeometryComponentFieldContext(mesh, domain_));
      mesh_evaluator_ = std::make_unique<FieldEvaluator>(*mesh_context_, domain_size);
      mesh_evaluator_->add(src_field_);
      mesh_evaluator_->evaluate();
      mesh_data_ = &mesh_evaluator_->get_evaluated(0);
    }

    if (use_points_) {
      const PointCloudComponent &points = *target_.get_component_for_read<PointCloudComponent>();
      const int domain_size = points.attribute_domain_size(domain_);
      point_context_.emplace(GeometryComponentFieldContext(points, domain_));
      point_evaluator_ = std::make_unique<FieldEvaluator>(*point_context_, domain_size);
      point_evaluator_->add(src_field_);
      point_evaluator_->evaluate();
      point_data_ = &point_evaluator_->get_evaluated(0);
    }
  }
};

static const GeometryComponent *find_best_match_component(const GeometrySet &geometry,
                                                          const GeometryComponentType type,
                                                          const AttributeDomain domain)
{
  /* Prefer transferring from the same component type, if it exists. */
  if (component_is_available(geometry, type, domain)) {
    return geometry.get_component_for_read(type);
  }

  /* If there is no component of the same type, choose the other component based on a consistent
   * order, rather than some more complicated heuristic. This is the same order visible in the
   * spreadsheet and used in the ray-cast node. */
  static const Array<GeometryComponentType> supported_types = {
      GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE};
  for (const GeometryComponentType src_type : supported_types) {
    if (component_is_available(geometry, src_type, domain)) {
      return geometry.get_component_for_read(src_type);
    }
  }

  return nullptr;
}

/**
 * Use a #FieldInput because it's necessary to know the field context in order to choose the
 * corresponding component type from the input geometry, and only a #FieldInput receives the
 * evaluation context to provide its data.
 *
 * The index-based transfer theoretically does not need realized data when there is only one
 * instance geometry set in the target. A future optimization could be removing that limitation
 * internally.
 */
class IndexTransferFieldInput : public FieldInput {
  GeometrySet src_geometry_;
  GField src_field_;
  Field<int> index_field_;
  AttributeDomain domain_;

 public:
  IndexTransferFieldInput(GeometrySet geometry,
                          GField src_field,
                          Field<int> index_field,
                          const AttributeDomain domain)
      : FieldInput(src_field.cpp_type(), "Attribute Transfer Index"),
        src_geometry_(std::move(geometry)),
        src_field_(std::move(src_field)),
        index_field_(std::move(index_field)),
        domain_(domain)
  {
    src_geometry_.ensure_owns_direct_data();
  }

  const GVArray *get_varray_for_context(const FieldContext &context,
                                        const IndexMask mask,
                                        ResourceScope &scope) const final
  {
    const GeometryComponentFieldContext *geometry_context =
        dynamic_cast<const GeometryComponentFieldContext *>(&context);
    if (geometry_context == nullptr) {
      return nullptr;
    }

    FieldEvaluator index_evaluator{*geometry_context, &mask};
    index_evaluator.add(index_field_);
    index_evaluator.evaluate();
    const VArray<int> &index_varray = index_evaluator.get_evaluated<int>(0);
    /* The index virtual array is expected to be a span, since transferring the same index for
     * every element is not very useful. */
    VArray_Span<int> indices{index_varray};

    const GeometryComponent *component = find_best_match_component(
        src_geometry_, geometry_context->geometry_component().type(), domain_);
    if (component == nullptr) {
      return nullptr;
    }

    GeometryComponentFieldContext target_context{*component, domain_};
    /* A potential improvement is to only copy the necessary values
     * based on the indices retrieved from the index input field. */
    FieldEvaluator target_evaluator{target_context, component->attribute_domain_size(domain_)};
    target_evaluator.add(src_field_);
    target_evaluator.evaluate();
    const GVArray &src_data = target_evaluator.get_evaluated(0);

    GArray dst(src_field_.cpp_type(), mask.min_array_size());

    attribute_math::convert_to_static_type(src_data.type(), [&](auto dummy) {
      using T = decltype(dummy);
      GVArray_Typed<T> src{src_data};
      copy_with_indices_clamped(*src, mask, indices, dst.as_mutable_span().typed<T>());
    });

    return &scope.construct<fn::GVArray_For_GArray>(std::move(dst));
  }
};

static GField get_input_attribute_field(GeoNodeExecParams &params, const CustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return params.extract_input<Field<float>>("Attribute_001");
    case CD_PROP_FLOAT3:
      return params.extract_input<Field<float3>>("Attribute");
    case CD_PROP_COLOR:
      return params.extract_input<Field<ColorGeometry4f>>("Attribute_002");
    case CD_PROP_BOOL:
      return params.extract_input<Field<bool>>("Attribute_003");
    case CD_PROP_INT32:
      return params.extract_input<Field<int>>("Attribute_004");
    default:
      BLI_assert_unreachable();
  }
  return {};
}

static void output_attribute_field(GeoNodeExecParams &params, GField field)
{
  switch (bke::cpp_type_to_custom_data_type(field.cpp_type())) {
    case CD_PROP_FLOAT: {
      params.set_output("Attribute_001", Field<float>(field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output("Attribute", Field<float3>(field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output("Attribute_002", Field<ColorGeometry4f>(field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output("Attribute_003", Field<bool>(field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output("Attribute_004", Field<int>(field));
      break;
    }
    default:
      break;
  }
}

static void geo_node_transfer_attribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Target");
  const bNode &node = params.node();
  const NodeGeometryTransferAttribute &data = *(const NodeGeometryTransferAttribute *)node.storage;
  const GeometryNodeAttributeTransferMode mapping = (GeometryNodeAttributeTransferMode)data.mode;
  const CustomDataType data_type = static_cast<CustomDataType>(data.data_type);
  const AttributeDomain domain = static_cast<AttributeDomain>(data.domain);

  GField field = get_input_attribute_field(params, data_type);

  auto return_default = [&]() {
    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      output_attribute_field(params, fn::make_constant_field<T>(T()));
    });
  };

  if (geometry.has_instances()) {
    if (geometry.has_realized_data()) {
      params.error_message_add(
          NodeWarningType::Info,
          TIP_("Only realized geometry is supported, instances will not be used"));
    }
    else {
      params.error_message_add(NodeWarningType::Error,
                               TIP_("Target geometry must contain realized data"));
      return return_default();
    }
    /* Since the instances are not used, there is no point in keeping
     * a reference to them while the field is passed around. */
    geometry.remove(GEO_COMPONENT_TYPE_INSTANCES);
  }

  GField output_field;
  switch (mapping) {
    case GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED: {
      const Mesh *mesh = geometry.get_mesh_for_read();
      if (mesh == nullptr) {
        if (!geometry.is_empty()) {
          params.error_message_add(NodeWarningType::Error,
                                   TIP_("The target geometry must contain a mesh"));
        }
        return return_default();
      }
      if (mesh->totpoly == 0) {
        /* Don't add a warning for empty meshes. */
        if (mesh->totvert != 0) {
          params.error_message_add(NodeWarningType::Error,
                                   TIP_("The target mesh must have faces"));
        }
        return return_default();
      }
      auto fn = std::make_unique<NearestInterpolatedTransferFunction>(std::move(geometry),
                                                                      std::move(field));
      auto op = std::make_shared<FieldOperation>(
          FieldOperation(std::move(fn), {params.extract_input<Field<float3>>("Source Position")}));
      output_field = GField(std::move(op));
      break;
    }
    case GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST: {
      if (geometry.has_curve() && !geometry.has_mesh() && !geometry.has_pointcloud()) {
        params.error_message_add(NodeWarningType::Warning,
                                 TIP_("Curve targets are not currently supported"));
        return return_default();
      }
      auto fn = std::make_unique<NearestTransferFunction>(
          std::move(geometry), std::move(field), domain);
      auto op = std::make_shared<FieldOperation>(
          FieldOperation(std::move(fn), {params.extract_input<Field<float3>>("Source Position")}));
      output_field = GField(std::move(op));
      break;
    }
    case GEO_NODE_ATTRIBUTE_TRANSFER_INDEX: {
      Field<int> indices = params.extract_input<Field<int>>("Index");
      std::shared_ptr<FieldInput> input = std::make_shared<IndexTransferFieldInput>(
          std::move(geometry), std::move(field), std::move(indices), domain);
      output_field = GField(std::move(input));
      break;
    }
  }

  output_attribute_field(params, std::move(output_field));
}

}  // namespace blender::nodes

void register_node_type_geo_transfer_attribute()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_TRANSFER_ATTRIBUTE, "Transfer Attribute", NODE_CLASS_ATTRIBUTE, 0);
  node_type_init(&ntype, blender::nodes::geo_node_transfer_attribute_init);
  node_type_update(&ntype, blender::nodes::geo_node_transfer_attribute_update);
  node_type_storage(&ntype,
                    "NodeGeometryTransferAttribute",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_transfer_attribute_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_transfer_attribute_exec;
  ntype.draw_buttons = blender::nodes::geo_node_transfer_attribute_layout;
  nodeRegisterType(&ntype);
}
