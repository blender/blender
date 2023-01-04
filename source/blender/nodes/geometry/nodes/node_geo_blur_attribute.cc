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
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_blur_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Value"), "Value_Float")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Int>(N_("Value"), "Value_Int")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Vector>(N_("Value"), "Value_Vector")
      .supports_field()
      .hide_value()
      .is_default_link_socket();
  b.add_input<decl::Color>(N_("Value"), "Value_Color")
      .supports_field()
      .hide_value()
      .is_default_link_socket();

  b.add_input<decl::Int>("Iterations")
      .default_value(1)
      .min(0)
      .description(N_("How many times to blur the values for all elements"));
  b.add_input<decl::Float>("Weight")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .supports_field()
      .description(N_("Relative mix weight of neighboring elements"));

  b.add_output<decl::Float>(N_("Value"), "Value_Float")
      .field_source_reference_all()
      .dependent_field();
  b.add_output<decl::Int>(N_("Value"), "Value_Int").field_source_reference_all().dependent_field();
  b.add_output<decl::Vector>(N_("Value"), "Value_Vector")
      .field_source_reference_all()
      .dependent_field();
  b.add_output<decl::Color>(N_("Value"), "Value_Color")
      .field_source_reference_all()
      .dependent_field();
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
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(2));

  const bNodeType &node_type = params.node_type();
  const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
      (eNodeSocketDatatype)params.other_socket().type);
  if (type && *type != CD_PROP_STRING) {
    params.add_item(IFACE_("Value"), [node_type, type](LinkSearchOpParams &params) {
      bNode &node = params.add_node(node_type);
      node.custom1 = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = static_cast<eCustomDataType>(node->custom1);

  bNodeSocket *socket_value_float = (bNodeSocket *)node->inputs.first;
  bNodeSocket *socket_value_int32 = socket_value_float->next;
  bNodeSocket *socket_value_vector = socket_value_int32->next;
  bNodeSocket *socket_value_color4f = socket_value_vector->next;

  nodeSetSocketAvailability(ntree, socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, socket_value_int32, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(ntree, socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, socket_value_color4f, data_type == CD_PROP_COLOR);

  bNodeSocket *out_socket_value_float = (bNodeSocket *)node->outputs.first;
  bNodeSocket *out_socket_value_int32 = out_socket_value_float->next;
  bNodeSocket *out_socket_value_vector = out_socket_value_int32->next;
  bNodeSocket *out_socket_value_color4f = out_socket_value_vector->next;

  nodeSetSocketAvailability(ntree, out_socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, out_socket_value_int32, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(ntree, out_socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, out_socket_value_color4f, data_type == CD_PROP_COLOR);
}

static Array<Vector<int>> build_vert_to_vert_by_edge_map(const Span<MEdge> edges,
                                                         const int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const MEdge &edge : edges) {
    map[edge.v1].append(edge.v2);
    map[edge.v2].append(edge.v1);
  }
  return map;
}

static Array<Vector<int>> build_edge_to_edge_by_vert_map(const Span<MEdge> edges,
                                                         const int verts_num,
                                                         const IndexMask edge_mask)
{
  Array<Vector<int>> map(edges.size());
  Array<Vector<int>> vert_to_edge_map = bke::mesh_topology::build_vert_to_edge_map(edges,
                                                                                   verts_num);

  threading::parallel_for(edge_mask.index_range(), 1024, [&](IndexRange range) {
    for (const int edge_i : edge_mask.slice(range)) {

      Vector<int> &self_edges = map[edge_i];
      const Span<int> vert_1_edges = vert_to_edge_map[edges[edge_i].v1];
      const Span<int> vert_2_edges = vert_to_edge_map[edges[edge_i].v2];

      self_edges.reserve(vert_1_edges.size() - 1 + vert_2_edges.size() - 1);

      for (const int i : vert_1_edges) {
        if (i != edge_i) {
          self_edges.append(i);
        }
      }
      for (const int i : vert_2_edges) {
        if (i != edge_i) {
          self_edges.append(i);
        }
      }
    }
  });
  return map;
}

static Array<Vector<int>> build_face_to_edge_by_loop_map(const Span<MPoly> polys,
                                                         const Span<MLoop> loops,
                                                         const int edges_num)
{
  Array<Vector<int>> map(edges_num);
  for (const int i : polys.index_range()) {
    const MPoly &poly = polys[i];
    for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
      map[loop.e].append(i);
    }
  }
  return map;
}

static Array<Vector<int>> build_face_to_face_by_edge_map(const Span<MPoly> polys,
                                                         const Span<MLoop> loops,
                                                         const int edges_num,
                                                         const IndexMask poly_mask)
{
  Array<Vector<int>> map(polys.size());
  Array<Vector<int>> faces_by_edge = build_face_to_edge_by_loop_map(polys, loops, edges_num);

  threading::parallel_for(poly_mask.index_range(), 1024, [&](IndexRange range) {
    for (const int poly_i : poly_mask.slice(range)) {
      const MPoly &poly = polys[poly_i];
      for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
        const int edge_i = loop.e;
        if (faces_by_edge[edge_i].size() > 1) {
          for (const int neighbor : faces_by_edge[edge_i]) {
            if (neighbor != poly_i) {
              map[poly_i].append(neighbor);
            }
          }
        }
      }
    }
  });
  return map;
}

static Array<Vector<int>> create_mesh_map(const Mesh &mesh,
                                          const eAttrDomain domain,
                                          const IndexMask mask)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT: {
      const Span<MEdge> edges = mesh.edges();
      const int verts_num = mesh.totvert;
      return build_vert_to_vert_by_edge_map(edges, verts_num);
    }
    case ATTR_DOMAIN_EDGE: {
      const Span<MEdge> edges = mesh.edges();
      const int verts_num = mesh.totvert;
      return build_edge_to_edge_by_vert_map(edges, verts_num, mask);
    }
    case ATTR_DOMAIN_FACE: {
      const Span<MPoly> polys = mesh.polys();
      const Span<MLoop> loops = mesh.loops();
      const int edges_num = mesh.totedge;
      return build_face_to_face_by_edge_map(polys, loops, edges_num, mask);
    }
    case ATTR_DOMAIN_CORNER: {
      return {};
    }
    default:
      BLI_assert_unreachable();
      return {};
  }
}

template<typename T>
static void blur_on_mesh_exec(const Span<float> neighbor_weights,
                              const Span<Vector<int>> neighbors_map,
                              const int iterations,
                              MutableSpan<T> main_buffer,
                              MutableSpan<T> tmp_buffer)
{
  MutableSpan<T> src = main_buffer;
  MutableSpan<T> dst = tmp_buffer;

  for ([[maybe_unused]] const int64_t iteration : IndexRange(iterations)) {
    attribute_math::DefaultMixer<T> mixer{dst, IndexMask(0)};
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
    std::swap(src, dst);
  }

  /* The last computed values are in #src now. If the main buffer is #dst, the values have to be
   * copied once more. */
  if (dst.data() == main_buffer.data()) {
    threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
      initialized_copy_n(
          src.data() + range.start(), range.size(), main_buffer.data() + range.start());
    });
  }
}

static void blur_on_mesh(const Mesh &mesh,
                         const eAttrDomain domain,
                         const int iterations,
                         const Span<float> neighbor_weights,
                         GMutableSpan main_buffer,
                         GMutableSpan tmp_buffer)
{
  Array<Vector<int>> neighbors_map = create_mesh_map(mesh, domain, neighbor_weights.index_range());
  if (neighbors_map.is_empty()) {
    return;
  }
  attribute_math::convert_to_static_type(main_buffer.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_same_v<T, bool>) {
      blur_on_mesh_exec<T>(neighbor_weights,
                           neighbors_map,
                           iterations,
                           main_buffer.typed<T>(),
                           tmp_buffer.typed<T>());
    }
  });
}

template<typename T>
static void blur_on_curve_exec(const bke::CurvesGeometry &curves,
                               const Span<float> neighbor_weights,
                               const int iterations,
                               MutableSpan<T> main_buffer,
                               MutableSpan<T> tmp_buffer)
{
  MutableSpan<T> src = main_buffer;
  MutableSpan<T> dst = tmp_buffer;

  const VArray<bool> cyclic = curves.cyclic();

  for ([[maybe_unused]] const int iteration : IndexRange(iterations)) {
    attribute_math::DefaultMixer<T> mixer{dst, IndexMask(0)};
    threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = curves.points_for_curve(curve_i);
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
        if (cyclic[curve_i]) {
          /* First point. */
          mixer.set(first_i, src[first_i], 1.0f);
          mixer.mix_in(first_i, src[first_i + 1], first_neighbor_weight);
          mixer.mix_in(first_i, src[last_i], first_neighbor_weight);
          /* Last point. */
          mixer.set(last_i, src[last_i], 1.0f);
          mixer.mix_in(last_i, src[last_i - 1], last_neighbor_weight);
          mixer.mix_in(last_i, src[first_i], last_neighbor_weight);
        }
        else {
          /* First point. */
          mixer.set(first_i, src[first_i], 1.0f);
          mixer.mix_in(first_i, src[first_i + 1], first_neighbor_weight);
          /* Last point. */
          mixer.set(last_i, src[last_i], 1.0f);
          mixer.mix_in(last_i, src[last_i - 1], last_neighbor_weight);
        }
      }
      mixer.finalize(curves.points_for_curves(range));
    });
    std::swap(src, dst);
  }

  /* The last computed values are in #src now. If the main buffer is #dst, the values have to be
   * copied once more. */
  if (dst.data() == main_buffer.data()) {
    threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
      initialized_copy_n(
          src.data() + range.start(), range.size(), main_buffer.data() + range.start());
    });
  }
}

static void blur_on_curves(const bke::CurvesGeometry &curves,
                           const int iterations,
                           const Span<float> neighbor_weights,
                           GMutableSpan main_buffer,
                           GMutableSpan tmp_buffer)
{
  attribute_math::convert_to_static_type(main_buffer.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_same_v<T, bool>) {
      blur_on_curve_exec<T>(
          curves, neighbor_weights, iterations, main_buffer.typed<T>(), tmp_buffer.typed<T>());
    }
  });
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

    GArray<> main_buffer(*type_, domain_size);

    FieldEvaluator evaluator(context, domain_size);

    evaluator.add_with_destination(value_field_, main_buffer.as_mutable_span());
    evaluator.add(weight_field_);
    evaluator.evaluate();

    /* Blurring does not make sense with a less than 2 elements. */
    if (domain_size <= 1) {
      return GVArray::ForGArray(std::move(main_buffer));
    }

    if (iterations_ <= 0) {
      return GVArray::ForGArray(std::move(main_buffer));
    }

    VArraySpan<float> neighbor_weights = evaluator.get_evaluated<float>(1);
    GArray<> tmp_buffer(*type_, domain_size);

    switch (context.type()) {
      case GEO_COMPONENT_TYPE_MESH:
        if (ELEM(context.domain(), ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE)) {
          if (const Mesh *mesh = context.mesh()) {
            blur_on_mesh(
                *mesh, context.domain(), iterations_, neighbor_weights, main_buffer, tmp_buffer);
          }
        }
        break;
      case GEO_COMPONENT_TYPE_CURVE:
        if (context.domain() == ATTR_DOMAIN_POINT) {
          if (const bke::CurvesGeometry *curves = context.curves()) {
            blur_on_curves(*curves, iterations_, neighbor_weights, main_buffer, tmp_buffer);
          }
        }
        break;
      default:
        break;
    }

    return GVArray::ForGArray(std::move(main_buffer));
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
            &other)) {
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

  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
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
