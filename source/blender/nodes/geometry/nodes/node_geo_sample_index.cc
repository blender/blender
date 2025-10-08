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

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "clamp", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySampleIndex *data = MEM_callocN<NodeGeometrySampleIndex>(__func__);
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
      GeometryComponent::Type::Mesh,
      GeometryComponent::Type::PointCloud,
      GeometryComponent::Type::Curve,
      GeometryComponent::Type::Instance,
      GeometryComponent::Type::GreasePencil};
  for (const GeometryComponent::Type src_type : supported_types) {
    if (component_is_available(geometry, src_type, domain)) {
      return geometry.get_component(src_type);
    }
  }

  return nullptr;
}

template<typename T>
void copy_with_clamped_indices(const VArray<T> &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               MutableSpan<T> dst)
{
  const int last_index = src.index_range().last();
  devirtualize_varray2(src, indices, [&](const auto src, const auto indices) {
    mask.foreach_index(GrainSize(4096), [&](const int i) {
      const int index = indices[i];
      dst[i] = src[std::clamp(index, 0, last_index)];
    });
  });
}

/**
 * The index-based transfer theoretically does not need realized data when there is only one
 * instance geometry set in the source. A future optimization could be removing that limitation
 * internally.
 */
class SampleIndexFunction : public mf::MultiFunction {
  GeometrySet src_geometry_;
  GField src_field_;
  AttrDomain domain_;
  bool clamp_;

  mf::Signature signature_;

  std::optional<bke::GeometryFieldContext> geometry_context_;
  std::unique_ptr<FieldEvaluator> evaluator_;
  const GVArray *src_data_ = nullptr;

 public:
  SampleIndexFunction(GeometrySet geometry,
                      GField src_field,
                      const AttrDomain domain,
                      const bool clamp)
      : src_geometry_(std::move(geometry)),
        src_field_(std::move(src_field)),
        domain_(domain),
        clamp_(clamp)
  {
    src_geometry_.ensure_owns_direct_data();

    mf::SignatureBuilder builder{"Sample Index", signature_};
    builder.single_input<int>("Index");
    builder.single_output("Value", src_field_.cpp_type());
    this->set_signature(&signature_);

    this->evaluate_field();
  }

  void evaluate_field()
  {
    const GeometryComponent *component = find_source_component(src_geometry_, domain_);
    if (component == nullptr) {
      return;
    }
    const int domain_num = component->attribute_domain_size(domain_);
    geometry_context_.emplace(bke::GeometryFieldContext(*component, domain_));
    evaluator_ = std::make_unique<FieldEvaluator>(*geometry_context_, domain_num);
    evaluator_->add(src_field_);
    evaluator_->evaluate();
    src_data_ = &evaluator_->get_evaluated(0);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<int> &indices = params.readonly_single_input<int>(0, "Index");
    GMutableSpan dst = params.uninitialized_single_output(1, "Value");

    const CPPType &type = dst.type();
    if (src_data_ == nullptr) {
      type.value_initialize_indices(dst.data(), mask);
      return;
    }

    if (clamp_) {
      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        copy_with_clamped_indices(src_data_->typed<T>(), indices, mask, dst.typed<T>());
      });
    }
    else {
      bke::copy_with_checked_indices(*src_data_, indices, mask, dst);
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  const NodeGeometrySampleIndex &storage = node_storage(params.node());
  const AttrDomain domain = AttrDomain(storage.domain);
  const bool use_clamp = bool(storage.clamp);

  GField value_field = params.extract_input<GField>("Value");
  SocketValueVariant index_value_variant = params.extract_input<SocketValueVariant>("Index");
  const CPPType &cpp_type = value_field.cpp_type();

  if (index_value_variant.is_single()) {
    const GeometryComponent *component = find_source_component(geometry, domain);
    if (!component) {
      params.set_default_remaining_outputs();
      return;
    }
    /* Optimization for the case when the index is a single value. Here only that one index has to
     * be evaluated. */
    const int domain_size = component->attribute_domain_size(domain);
    int index = index_value_variant.extract<int>();
    if (use_clamp) {
      index = std::clamp(index, 0, domain_size - 1);
    }
    if (index >= 0 && index < domain_size) {
      const IndexMask mask = IndexRange(index, 1);
      const bke::GeometryFieldContext geometry_context(*component, domain);
      FieldEvaluator evaluator(geometry_context, &mask);
      evaluator.add(value_field);
      evaluator.evaluate();
      const GVArray &data = evaluator.get_evaluated(0);
      BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, buffer);
      data.get_to_uninitialized(index, buffer);
      params.set_output("Value", fn::make_constant_field(cpp_type, buffer));
      cpp_type.destruct(buffer);
    }
    else {
      params.set_output("Value", fn::make_constant_field(cpp_type, cpp_type.default_value()));
    }
    return;
  }

  bke::SocketValueVariant output_value;
  std::string error_message;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<SampleIndexFunction>(
              std::move(geometry), std::move(value_field), domain, use_clamp),
          {&index_value_variant},
          {&output_value},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Value", std::move(output_value));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleIndex", GEO_NODE_SAMPLE_INDEX);
  ntype.ui_name = "Sample Index";
  ntype.ui_description = "Retrieve values from specific geometry elements";
  ntype.enum_name_legacy = "SAMPLE_INDEX";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  blender::bke::node_type_storage(
      ntype, "NodeGeometrySampleIndex", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_index_cc
