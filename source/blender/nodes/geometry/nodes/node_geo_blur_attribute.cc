/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_generic_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_blur_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value", "Value_Float")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Int>("Value", "Value_Int")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Vector>("Value", "Value_Vector")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Color>("Value", "Value_Color")
      .supports_field()
      .hide_value()
      .is_default_link_socket();

  b.add_input<decl::Int>("Iterations")
      .default_value(1)
      .min(0)
      .description("How many times to blur the values for all elements");
  b.add_input<decl::Float>("Weight")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .supports_field()
      .description("Relative mix weight of neighboring elements");

  b.add_output<decl::Float>("Value", "Value_Float").field_source_reference_all().dependent_field();
  b.add_output<decl::Int>("Value", "Value_Int").field_source_reference_all().dependent_field();
  b.add_output<decl::Vector>("Value", "Value_Vector")
      .field_source_reference_all()
      .dependent_field();
  b.add_output<decl::Color>("Value", "Value_Color").field_source_reference_all().dependent_field();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeType &node_type = params.node_type();
  const NodeDeclaration &declaration = *node_type.fixed_declaration;

  /* Weight and Iterations inputs don't change based on the data type. */
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(2));

  const eNodeSocketDatatype other_socket_type = static_cast<eNodeSocketDatatype>(
      params.other_socket().type);
  const std::optional<eCustomDataType> new_node_type = node_data_type_to_custom_data_type(
      other_socket_type);
  if (!new_node_type.has_value()) {
    return;
  }
  eCustomDataType fixed_data_type = *new_node_type;
  if (fixed_data_type == CD_PROP_STRING) {
    return;
  }
  if (fixed_data_type == CD_PROP_BOOL) {
    /* This node does not support boolean sockets, use integer instead. */
    fixed_data_type = CD_PROP_INT32;
  }
  params.add_item(IFACE_("Value"), [node_type, fixed_data_type](LinkSearchOpParams &params) {
    bNode &node = params.add_node(node_type);
    node.custom1 = fixed_data_type;
    params.update_and_connect_available_socket(node, "Value");
  });
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = static_cast<eCustomDataType>(node->custom1);

  bNodeSocket *socket_value_float = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_value_int32 = socket_value_float->next;
  bNodeSocket *socket_value_vector = socket_value_int32->next;
  bNodeSocket *socket_value_color4f = socket_value_vector->next;

  bke::nodeSetSocketAvailability(ntree, socket_value_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_value_int32, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, socket_value_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_value_color4f, data_type == CD_PROP_COLOR);

  bNodeSocket *out_socket_value_float = (bNodeSocket *)node->outputs.first;
  bNodeSocket *out_socket_value_int32 = out_socket_value_float->next;
  bNodeSocket *out_socket_value_vector = out_socket_value_int32->next;
  bNodeSocket *out_socket_value_color4f = out_socket_value_vector->next;

  bke::nodeSetSocketAvailability(ntree, out_socket_value_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_int32, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_color4f, data_type == CD_PROP_COLOR);
}

static void build_vert_to_vert_by_edge_map(const Span<int2> edges,
                                           const int verts_num,
                                           Array<int> &r_offsets,
                                           Array<int> &r_indices)
{
  bke::mesh::build_vert_to_edge_map(edges, verts_num, r_offsets, r_indices);
  const OffsetIndices<int> offsets(r_offsets);
  threading::parallel_for(IndexRange(verts_num), 2048, [&](const IndexRange range) {
    for (const int vert : range) {
      MutableSpan<int> neighbors = r_indices.as_mutable_span().slice(offsets[vert]);
      for (const int i : neighbors.index_range()) {
        neighbors[i] = bke::mesh::edge_other_vert(edges[neighbors[i]], vert);
      }
    }
  });
}

static void build_edge_to_edge_by_vert_map(const Span<int2> edges,
                                           const int verts_num,
                                           Array<int> &r_offsets,
                                           Array<int> &r_indices)
{
  Array<int> vert_to_edge_offset_data;
  Array<int> vert_to_edge_indices;
  const GroupedSpan<int> vert_to_edge = bke::mesh::build_vert_to_edge_map(
      edges, verts_num, vert_to_edge_offset_data, vert_to_edge_indices);
  const OffsetIndices<int> vert_to_edge_offsets(vert_to_edge_offset_data);

  r_offsets = Array<int>(edges.size() + 1, 0);
  threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int edge_i : range) {
      const int2 edge = edges[edge_i];
      r_offsets[edge_i] = vert_to_edge_offsets[edge[0]].size() - 1 +
                          vert_to_edge_offsets[edge[1]].size() - 1;
    }
  });
  const OffsetIndices offsets = offset_indices::accumulate_counts_to_offsets(r_offsets);
  r_indices.reinitialize(offsets.total_size());

  threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int edge_i : range) {
      const int2 edge = edges[edge_i];
      MutableSpan<int> neighbors = r_indices.as_mutable_span().slice(offsets[edge_i]);
      int count = 0;
      for (const Span<int> neighbor_edges : {vert_to_edge[edge[0]], vert_to_edge[edge[1]]}) {
        for (const int neighbor_edge : neighbor_edges) {
          if (neighbor_edge != edge_i) {
            neighbors[count] = neighbor_edge;
            count++;
          }
        }
      }
    }
  });
}

static void build_face_to_face_by_edge_map(const OffsetIndices<int> polys,
                                           const Span<int> corner_edges,
                                           const int edges_num,
                                           Array<int> &r_offsets,
                                           Array<int> &r_indices)
{
  Array<int> edge_to_poly_offsets;
  Array<int> edge_to_poly_indices;
  const GroupedSpan<int> edge_to_poly_map = bke::mesh::build_edge_to_poly_map(
      polys, corner_edges, edges_num, edge_to_poly_offsets, edge_to_poly_indices);

  r_offsets = Array<int>(polys.size() + 1, 0);
  for (const int poly_i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[poly_i])) {
      for (const int neighbor : edge_to_poly_map[edge]) {
        if (neighbor != poly_i) {
          r_offsets[poly_i]++;
        }
      }
    }
  }
  const OffsetIndices offsets = offset_indices::accumulate_counts_to_offsets(r_offsets);
  r_indices.reinitialize(offsets.total_size());

  threading::parallel_for(polys.index_range(), 1024, [&](IndexRange range) {
    for (const int poly_i : range) {
      MutableSpan<int> neighbors = r_indices.as_mutable_span().slice(offsets[poly_i]);
      int count = 0;
      for (const int edge : corner_edges.slice(polys[poly_i])) {
        for (const int neighbor : edge_to_poly_map[edge]) {
          if (neighbor != poly_i) {
            neighbors[count] = neighbor;
            count++;
          }
        }
      }
    }
  });
}

static GroupedSpan<int> create_mesh_map(const Mesh &mesh,
                                        const eAttrDomain domain,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      build_vert_to_vert_by_edge_map(mesh.edges(), mesh.totvert, r_offsets, r_indices);
      break;
    case ATTR_DOMAIN_EDGE:
      build_edge_to_edge_by_vert_map(mesh.edges(), mesh.totvert, r_offsets, r_indices);
      break;
    case ATTR_DOMAIN_FACE:
      build_face_to_face_by_edge_map(
          mesh.polys(), mesh.corner_edges(), mesh.totedge, r_offsets, r_indices);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

template<typename T>
static Span<T> blur_on_mesh_exec(const Span<float> neighbor_weights,
                                 const GroupedSpan<int> neighbors_map,
                                 const int iterations,
                                 const MutableSpan<T> buffer_a,
                                 const MutableSpan<T> buffer_b)
{
  /* Source is set to buffer_b even though it is actually in buffer_a because the loop below starts
   * with swapping both. */
  MutableSpan<T> src = buffer_b;
  MutableSpan<T> dst = buffer_a;

  for ([[maybe_unused]] const int64_t iteration : IndexRange(iterations)) {
    std::swap(src, dst);
    bke::attribute_math::DefaultMixer<T> mixer{dst, IndexMask(0)};
    threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t index : range) {
        const Span<int> neighbors = neighbors_map[index];
        const float neighbor_weight = neighbor_weights[index];
        mixer.set(index, src[index], 1.0f);
        for (const int neighbor : neighbors) {
          mixer.mix_in(index, src[neighbor], neighbor_weight);
        }
      }
      mixer.finalize(range);
    });
  }

  return dst;
}

static GSpan blur_on_mesh(const Mesh &mesh,
                          const eAttrDomain domain,
                          const int iterations,
                          const Span<float> neighbor_weights,
                          const GMutableSpan buffer_a,
                          const GMutableSpan buffer_b)
{
  Array<int> neighbor_offsets;
  Array<int> neighbor_indices;
  const GroupedSpan<int> neighbors_map = create_mesh_map(
      mesh, domain, neighbor_offsets, neighbor_indices);

  GSpan result_buffer;
  bke::attribute_math::convert_to_static_type(buffer_a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_same_v<T, bool>) {
      result_buffer = blur_on_mesh_exec<T>(
          neighbor_weights, neighbors_map, iterations, buffer_a.typed<T>(), buffer_b.typed<T>());
    }
  });
  return result_buffer;
}

template<typename T>
static Span<T> blur_on_curve_exec(const bke::CurvesGeometry &curves,
                                  const Span<float> neighbor_weights,
                                  const int iterations,
                                  const MutableSpan<T> buffer_a,
                                  const MutableSpan<T> buffer_b)
{
  MutableSpan<T> src = buffer_b;
  MutableSpan<T> dst = buffer_a;

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  for ([[maybe_unused]] const int iteration : IndexRange(iterations)) {
    std::swap(src, dst);
    bke::attribute_math::DefaultMixer<T> mixer{dst, IndexMask(0)};
    threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() == 1) {
          /* No mixing possible. */
          const int point_i = points[0];
          mixer.set(point_i, src[point_i], 1.0f);
          continue;
        }
        /* Inner points. */
        for (const int point_i : points.drop_front(1).drop_back(1)) {
          const float neighbor_weight = neighbor_weights[point_i];
          mixer.set(point_i, src[point_i], 1.0f);
          mixer.mix_in(point_i, src[point_i - 1], neighbor_weight);
          mixer.mix_in(point_i, src[point_i + 1], neighbor_weight);
        }
        const int first_i = points[0];
        const float first_neighbor_weight = neighbor_weights[first_i];
        const int last_i = points.last();
        const float last_neighbor_weight = neighbor_weights[last_i];

        /* First point. */
        mixer.set(first_i, src[first_i], 1.0f);
        mixer.mix_in(first_i, src[first_i + 1], first_neighbor_weight);
        /* Last point. */
        mixer.set(last_i, src[last_i], 1.0f);
        mixer.mix_in(last_i, src[last_i - 1], last_neighbor_weight);

        if (cyclic[curve_i]) {
          /* First point. */
          mixer.mix_in(first_i, src[last_i], first_neighbor_weight);
          /* Last point. */
          mixer.mix_in(last_i, src[first_i], last_neighbor_weight);
        }
      }
      mixer.finalize(points_by_curve[range]);
    });
  }

  return dst;
}

static GSpan blur_on_curves(const bke::CurvesGeometry &curves,
                            const int iterations,
                            const Span<float> neighbor_weights,
                            const GMutableSpan buffer_a,
                            const GMutableSpan buffer_b)
{
  GSpan result_buffer;
  bke::attribute_math::convert_to_static_type(buffer_a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_same_v<T, bool>) {
      result_buffer = blur_on_curve_exec<T>(
          curves, neighbor_weights, iterations, buffer_a.typed<T>(), buffer_b.typed<T>());
    }
  });
  return result_buffer;
}

class BlurAttributeFieldInput final : public bke::GeometryFieldInput {
 private:
  const Field<float> weight_field_;
  const GField value_field_;
  const int iterations_;

 public:
  BlurAttributeFieldInput(Field<float> weight_field, GField value_field, const int iterations)
      : bke::GeometryFieldInput(value_field.cpp_type(), "Blur Attribute"),
        weight_field_(std::move(weight_field)),
        value_field_(std::move(value_field)),
        iterations_(iterations)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask /*mask*/) const final
  {
    const int64_t domain_size = context.attributes()->domain_size(context.domain());

    GArray<> buffer_a(*type_, domain_size);

    FieldEvaluator evaluator(context, domain_size);

    evaluator.add_with_destination(value_field_, buffer_a.as_mutable_span());
    evaluator.add(weight_field_);
    evaluator.evaluate();

    /* Blurring does not make sense with a less than 2 elements. */
    if (domain_size <= 1) {
      return GVArray::ForGArray(std::move(buffer_a));
    }

    if (iterations_ <= 0) {
      return GVArray::ForGArray(std::move(buffer_a));
    }

    VArraySpan<float> neighbor_weights = evaluator.get_evaluated<float>(1);
    GArray<> buffer_b(*type_, domain_size);

    GSpan result_buffer;
    switch (context.type()) {
      case GEO_COMPONENT_TYPE_MESH:
        if (ELEM(context.domain(), ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE)) {
          if (const Mesh *mesh = context.mesh()) {
            result_buffer = blur_on_mesh(
                *mesh, context.domain(), iterations_, neighbor_weights, buffer_a, buffer_b);
          }
        }
        break;
      case GEO_COMPONENT_TYPE_CURVE:
        if (context.domain() == ATTR_DOMAIN_POINT) {
          if (const bke::CurvesGeometry *curves = context.curves()) {
            result_buffer = blur_on_curves(
                *curves, iterations_, neighbor_weights, buffer_a, buffer_b);
          }
        }
        break;
      default:
        break;
    }

    BLI_assert(ELEM(result_buffer.data(), buffer_a.data(), buffer_b.data()));
    if (result_buffer.data() == buffer_a.data()) {
      return GVArray::ForGArray(std::move(buffer_a));
    }
    return GVArray::ForGArray(std::move(buffer_b));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    weight_field_.node().for_each_field_input_recursive(fn);
    value_field_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash_3(iterations_, weight_field_, value_field_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const BlurAttributeFieldInput *other_blur = dynamic_cast<const BlurAttributeFieldInput *>(
            &other))
    {
      return weight_field_ == other_blur->weight_field_ &&
             value_field_ == other_blur->value_field_ && iterations_ == other_blur->iterations_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const override
  {
    return bke::try_detect_field_domain(component, value_field_);
  }
};

static StringRefNull identifier_suffix(eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return "Float";
    case CD_PROP_INT32:
      return "Int";
    case CD_PROP_COLOR:
      return "Color";
    case CD_PROP_FLOAT3:
      return "Vector";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const eCustomDataType data_type = static_cast<eCustomDataType>(params.node().custom1);

  const int iterations = params.extract_input<int>("Iterations");
  Field<float> weight_field = params.extract_input<Field<float>>("Weight");

  bke::attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    static const std::string identifier = "Value_" + identifier_suffix(data_type);
    Field<T> value_field = params.extract_input<Field<T>>(identifier);
    Field<T> output_field{std::make_shared<BlurAttributeFieldInput>(
        std::move(weight_field), std::move(value_field), iterations)};
    params.set_output(identifier, std::move(output_field));
  });
}

}  // namespace blender::nodes::node_geo_blur_attribute_cc

void register_node_type_geo_blur_attribute()
{
  namespace file_ns = blender::nodes::node_geo_blur_attribute_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_BLUR_ATTRIBUTE, "Blur Attribute", NODE_CLASS_ATTRIBUTE);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
