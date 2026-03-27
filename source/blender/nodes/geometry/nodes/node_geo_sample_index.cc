/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_index_cc {

NODE_STORAGE_FUNCS(NodeGeometrySampleIndex);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh,
                       GeometryComponent::Type::PointCloud,
                       GeometryComponent::Type::Curve,
                       GeometryComponent::Type::Instance,
                       GeometryComponent::Type::GreasePencil})
      .description("Geometry to sample a value on");
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    b.add_input(data_type, "Value").hide_value().field_on_all();
  }
  b.add_input<decl::Int>("Index")
      .supports_field()
      .description("Which element to retrieve a value from on the geometry")
      .structure_type(StructureType::Dynamic);

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    b.add_output(data_type, "Value").dependent_field({2});
  }
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "clamp", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySampleIndex *data = MEM_new<NodeGeometrySampleIndex>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = int8_t(AttrDomain::Point);
  data->clamp = 0;
  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);

  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    /* The input and output sockets have the same name. */
    params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleIndex");
      node_storage(node).data_type = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry"_ustr);
  const NodeGeometrySampleIndex &storage = node_storage(params.node());
  const AttrDomain domain = AttrDomain(storage.domain);
  const bool use_clamp = bool(storage.clamp);

  GField value_field = params.extract_input<GField>("Value"_ustr);
  SocketValueVariant index_value_variant = params.extract_input<SocketValueVariant>("Index"_ustr);
  const CPPType &cpp_type = value_field.cpp_type();

  const GeometryComponent *component = bke::SampleIndexFunction::find_source_component(geometry,
                                                                                       domain);
  if (!component) {
    params.set_default_remaining_outputs();
    return;
  }
  if (index_value_variant.is_single()) {
    /* Optimization for the case when the index is a single value. Here only that one index has to
     * be evaluated. */
    const int domain_size = component->attribute_domain_size(domain);
    int index = index_value_variant.extract<int>();
    if (use_clamp) {
      index = std::clamp(index, 0, domain_size - 1);
    }
    const eNodeSocketDatatype socket_type = params.node().output_socket(0).typeinfo->type;
    SocketValueVariant output_value;
    void *buffer = output_value.allocate_single(socket_type);
    if (index >= 0 && index < domain_size) {
      const IndexMask mask = IndexRange(index, 1);
      const bke::GeometryFieldContext geometry_context(*component, domain);
      FieldEvaluator evaluator(geometry_context, &mask);
      evaluator.add(value_field);
      evaluator.evaluate();
      const GVArray &data = evaluator.get_evaluated(0);
      data.get_to_uninitialized(index, buffer);
    }
    else {
      cpp_type.copy_construct(cpp_type.default_value(), buffer);
    }
    params.set_output("Value"_ustr, std::move(output_value));
    return;
  }

  std::string error_message;

  if (use_clamp) {
    bke::SocketValueVariant index_value_variant_copy = index_value_variant;
    static auto clamp_fn = mf::build::SI3_SO<int, int, int, int>(
        "Clamp",
        [](int value, int min, int max) { return std::clamp(value, min, max); },
        mf::build::exec_presets::SomeSpanOrSingle<0>());
    const int domain_size = component->attribute_domain_size(domain);
    bke::SocketValueVariant min_value = bke::SocketValueVariant::From(0);
    bke::SocketValueVariant max_value = bke::SocketValueVariant::From(domain_size - 1);
    if (!execute_multi_function_on_value_variant(
            clamp_fn,
            {&index_value_variant_copy, &min_value, &max_value},
            {&index_value_variant},
            params.user_data(),
            error_message))
    {
      params.set_default_remaining_outputs();
      params.error_message_add(NodeWarningType::Error, std::move(error_message));
      return;
    }
  }

  bke::SocketValueVariant output_value;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<bke::SampleIndexFunction>(
              std::move(geometry), std::move(value_field), domain),
          {&index_value_variant},
          {&output_value},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Value"_ustr, std::move(output_value));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleIndex", GEO_NODE_SAMPLE_INDEX);
  ntype.ui_name = "Sample Index";
  ntype.ui_description = "Retrieve values from specific geometry elements";
  ntype.enum_name_legacy = "SAMPLE_INDEX";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  bke::node_type_storage(
      ntype, "NodeGeometrySampleIndex", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_index_cc
