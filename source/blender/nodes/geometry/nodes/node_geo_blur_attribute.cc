/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_generic_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_blur_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_input(data_type, "Value").supports_field().hide_value().is_default_link_socket();
  }
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

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_output(data_type, "Value").field_source_reference_all().dependent_field();
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const blender::bke::bNodeType &node_type = params.node_type();
  const NodeDeclaration &declaration = *node_type.static_declaration;

  /* Weight and Iterations inputs don't change based on the data type. */
  search_link_ops_for_declarations(params, declaration.inputs);

  const std::optional<eCustomDataType> new_node_type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (!new_node_type.has_value()) {
    return;
  }
  eCustomDataType fixed_data_type = *new_node_type;
  if (fixed_data_type == CD_PROP_STRING) {
    return;
  }
  if (fixed_data_type == CD_PROP_QUATERNION) {
    fixed_data_type = CD_PROP_FLOAT3;
  }
  if (fixed_data_type == CD_PROP_FLOAT4X4) {
    /* Don't implement matrix blurring for now. */
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

static void build_face_to_face_by_edge_map(const OffsetIndices<int> faces,
                                           const Span<int> corner_edges,
                                           const int edges_num,
                                           Array<int> &r_offsets,
                                           Array<int> &r_indices)
{
  Array<int> edge_to_face_offset_data;
  Array<int> edge_to_face_indices;
  const GroupedSpan<int> edge_to_face_map = bke::mesh::build_edge_to_face_map(
      faces, corner_edges, edges_num, edge_to_face_offset_data, edge_to_face_indices);
  const OffsetIndices<int> edge_to_face_offsets(edge_to_face_offset_data);

  r_offsets = Array<int>(faces.size() + 1, 0);
  threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
    for (const int face_i : range) {
      for (const int edge : corner_edges.slice(faces[face_i])) {
        /* Subtract face itself from the number of faces connected to the edge. */
        r_offsets[face_i] += edge_to_face_offsets[edge].size() - 1;
      }
    }
  });
  const OffsetIndices<int> offsets = offset_indices::accumulate_counts_to_offsets(r_offsets);
  r_indices.reinitialize(offsets.total_size());

  threading::parallel_for(faces.index_range(), 1024, [&](IndexRange range) {
    for (const int face_i : range) {
      MutableSpan<int> neighbors = r_indices.as_mutable_span().slice(offsets[face_i]);
      if (neighbors.is_empty()) {
        continue;
      }
      int count = 0;
      for (const int edge : corner_edges.slice(faces[face_i])) {
        for (const int neighbor : edge_to_face_map[edge]) {
          if (neighbor != face_i) {
            neighbors[count] = neighbor;
            count++;
          }
        }
      }
    }
  });
}

static GroupedSpan<int> create_mesh_map(const Mesh &mesh,
                                        const AttrDomain domain,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  switch (domain) {
    case AttrDomain::Point:
      build_vert_to_vert_by_edge_map(mesh.edges(), mesh.verts_num, r_offsets, r_indices);
      break;
    case AttrDomain::Edge:
      build_edge_to_edge_by_vert_map(mesh.edges(), mesh.verts_num, r_offsets, r_indices);
      break;
    case AttrDomain::Face:
      build_face_to_face_by_edge_map(
          mesh.faces(), mesh.corner_edges(), mesh.edges_num, r_offsets, r_indices);
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

template<typename Func> static void to_static_type_for_blur(const CPPType &type, const Func &func)
{
  type.to_static_type_tag<int, float, float3, ColorGeometry4f>([&](auto type_tag) {
    using T = typename decltype(type_tag)::type;
    if constexpr (!std::is_same_v<T, void>) {
      func(T());
    }
    else {
      BLI_assert_unreachable();
    }
  });
}

static GSpan blur_on_mesh(const Mesh &mesh,
                          const AttrDomain domain,
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
  to_static_type_for_blur(buffer_a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    result_buffer = blur_on_mesh_exec<T>(
        neighbor_weights, neighbors_map, iterations, buffer_a.typed<T>(), buffer_b.typed<T>());
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
  to_static_type_for_blur(buffer_a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    result_buffer = blur_on_curve_exec<T>(
        curves, neighbor_weights, iterations, buffer_a.typed<T>(), buffer_b.typed<T>());
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
                                 const IndexMask & /*mask*/) const final
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

    GSpan result_buffer = buffer_a.as_span();
    switch (context.type()) {
      case GeometryComponent::Type::Mesh:
        if (ELEM(context.domain(), AttrDomain::Point, AttrDomain::Edge, AttrDomain::Face)) {
          if (const Mesh *mesh = context.mesh()) {
            result_buffer = blur_on_mesh(
                *mesh, context.domain(), iterations_, neighbor_weights, buffer_a, buffer_b);
          }
        }
        break;
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::GreasePencil:
        if (context.domain() == AttrDomain::Point) {
          if (const bke::CurvesGeometry *curves = context.curves_or_strokes()) {
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
    return get_default_hash(iterations_, weight_field_, value_field_);
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

  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override
  {
    const std::optional<AttrDomain> domain = bke::try_detect_field_domain(component, value_field_);
    if (domain.has_value() && *domain == AttrDomain::Corner) {
      return AttrDomain::Point;
    }
    return domain;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const int iterations = params.extract_input<int>("Iterations");
  Field<float> weight_field = params.extract_input<Field<float>>("Weight");

  GField value_field = params.extract_input<GField>("Value");
  GField output_field{std::make_shared<BlurAttributeFieldInput>(
      std::move(weight_field), std::move(value_field), iterations)};
  params.set_output<GField>("Value", std::move(output_field));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "data_type",
      "Data Type",
      "",
      rna_enum_attribute_type_items,
      NOD_inline_enum_accessors(custom1),
      CD_PROP_FLOAT,
      [](bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        return enum_items_filter(rna_enum_attribute_type_items, [](const EnumPropertyItem &item) {
          return ELEM(item.value, CD_PROP_FLOAT, CD_PROP_FLOAT3, CD_PROP_COLOR, CD_PROP_INT32);
        });
      });
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_BLUR_ATTRIBUTE, "Blur Attribute", NODE_CLASS_ATTRIBUTE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_blur_attribute_cc
