/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

template<typename T>
void copy_with_checked_indices(const VArray<T> &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               MutableSpan<T> dst)
{
  const IndexRange src_range = src.index_range();
  devirtualize_varray2(src, indices, [&](const auto src, const auto indices) {
    mask.foreach_index(GrainSize(4096), [&](const int i) {
      const int index = indices[i];
      if (src_range.contains(index)) {
        dst[i] = src[index];
      }
      else {
        dst[i] = {};
      }
    });
  });
}

void copy_with_checked_indices(const GVArray &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_with_checked_indices(src.typed<T>(), indices, mask, dst.typed<T>());
  });
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_sample_index_cc {

NODE_STORAGE_FUNCS(NodeGeometrySampleIndex);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh,
                       GeometryComponent::Type::PointCloud,
                       GeometryComponent::Type::Curve,
                       GeometryComponent::Type::Instance});

  b.add_input<decl::Float>("Value", "Value_Float").hide_value().field_on_all();
  b.add_input<decl::Int>("Value", "Value_Int").hide_value().field_on_all();
  b.add_input<decl::Vector>("Value", "Value_Vector").hide_value().field_on_all();
  b.add_input<decl::Color>("Value", "Value_Color").hide_value().field_on_all();
  b.add_input<decl::Bool>("Value", "Value_Bool").hide_value().field_on_all();
  b.add_input<decl::Rotation>("Value", "Value_Rotation").hide_value().field_on_all();
  b.add_input<decl::Int>("Index").supports_field().description(
      "Which element to retrieve a value from on the geometry");

  b.add_output<decl::Float>("Value", "Value_Float").dependent_field({7});
  b.add_output<decl::Int>("Value", "Value_Int").dependent_field({7});
  b.add_output<decl::Vector>("Value", "Value_Vector").dependent_field({7});
  b.add_output<decl::Color>("Value", "Value_Color").dependent_field({7});
  b.add_output<decl::Bool>("Value", "Value_Bool").dependent_field({7});
  b.add_output<decl::Rotation>("Value", "Value_Rotation").dependent_field({7});
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "clamp", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySampleIndex *data = MEM_cnew<NodeGeometrySampleIndex>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;
  data->clamp = 0;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);

  bNodeSocket *in_socket_geometry = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *in_socket_float = in_socket_geometry->next;
  bNodeSocket *in_socket_int32 = in_socket_float->next;
  bNodeSocket *in_socket_vector = in_socket_int32->next;
  bNodeSocket *in_socket_color4f = in_socket_vector->next;
  bNodeSocket *in_socket_bool = in_socket_color4f->next;
  bNodeSocket *in_socket_quat = in_socket_bool->next;

  bke::nodeSetSocketAvailability(ntree, in_socket_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, in_socket_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, in_socket_color4f, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, in_socket_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, in_socket_int32, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, in_socket_quat, data_type == CD_PROP_QUATERNION);

  bNodeSocket *out_socket_float = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *out_socket_int32 = out_socket_float->next;
  bNodeSocket *out_socket_vector = out_socket_int32->next;
  bNodeSocket *out_socket_color4f = out_socket_vector->next;
  bNodeSocket *out_socket_bool = out_socket_color4f->next;
  bNodeSocket *out_socket_quat = out_socket_bool->next;

  bke::nodeSetSocketAvailability(ntree, out_socket_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, out_socket_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, out_socket_color4f, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, out_socket_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, out_socket_int32, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, out_socket_quat, data_type == CD_PROP_QUATERNION);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(1));
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(1));

  const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
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
                                   const eAttrDomain domain)
{
  if (!geometry.has(type)) {
    return false;
  }
  const GeometryComponent &component = *geometry.get_component(type);
  return component.attribute_domain_size(domain) != 0;
}

static const GeometryComponent *find_source_component(const GeometrySet &geometry,
                                                      const eAttrDomain domain)
{
  /* Choose the other component based on a consistent order, rather than some more complicated
   * heuristic. This is the same order visible in the spreadsheet and used in the ray-cast node. */
  static const Array<GeometryComponent::Type> supported_types = {
      GeometryComponent::Type::Mesh,
      GeometryComponent::Type::PointCloud,
      GeometryComponent::Type::Curve,
      GeometryComponent::Type::Instance};
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
  eAttrDomain domain_;
  bool clamp_;

  mf::Signature signature_;

  std::optional<bke::GeometryFieldContext> geometry_context_;
  std::unique_ptr<FieldEvaluator> evaluator_;
  const GVArray *src_data_ = nullptr;

 public:
  SampleIndexFunction(GeometrySet geometry,
                      GField src_field,
                      const eAttrDomain domain,
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
      copy_with_checked_indices(*src_data_, indices, mask, dst);
    }
  }
};

static GField get_input_attribute_field(GeoNodeExecParams &params, const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return params.extract_input<Field<float>>("Value_Float");
    case CD_PROP_FLOAT3:
      return params.extract_input<Field<float3>>("Value_Vector");
    case CD_PROP_COLOR:
      return params.extract_input<Field<ColorGeometry4f>>("Value_Color");
    case CD_PROP_BOOL:
      return params.extract_input<Field<bool>>("Value_Bool");
    case CD_PROP_INT32:
      return params.extract_input<Field<int>>("Value_Int");
    case CD_PROP_QUATERNION:
      return params.extract_input<Field<math::Quaternion>>("Value_Rotation");
    default:
      BLI_assert_unreachable();
  }
  return {};
}

static void output_attribute_field(GeoNodeExecParams &params, GField field)
{
  switch (bke::cpp_type_to_custom_data_type(field.cpp_type())) {
    case CD_PROP_FLOAT: {
      params.set_output("Value_Float", Field<float>(field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output("Value_Vector", Field<float3>(field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output("Value_Color", Field<ColorGeometry4f>(field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output("Value_Bool", Field<bool>(field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output("Value_Int", Field<int>(field));
      break;
    }
    case CD_PROP_QUATERNION: {
      params.set_output("Value_Rotation", Field<math::Quaternion>(field));
      break;
    }
    default:
      break;
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  const NodeGeometrySampleIndex &storage = node_storage(params.node());
  const eCustomDataType data_type = eCustomDataType(storage.data_type);
  const eAttrDomain domain = eAttrDomain(storage.domain);
  const bool use_clamp = bool(storage.clamp);

  GField value_field = get_input_attribute_field(params, data_type);
  ValueOrField<int> index_value_or_field = params.extract_input<ValueOrField<int>>("Index");
  const CPPType &cpp_type = value_field.cpp_type();

  GField output_field;
  if (index_value_or_field.is_field()) {
    /* If the index is a field, the output has to be a field that still depends on the input. */
    auto fn = std::make_shared<SampleIndexFunction>(
        std::move(geometry), std::move(value_field), domain, use_clamp);
    auto op = FieldOperation::Create(std::move(fn), {index_value_or_field.as_field()});
    output_field = GField(std::move(op));
  }
  else if (const GeometryComponent *component = find_source_component(geometry, domain)) {
    /* Optimization for the case when the index is a single value. Here only that one index has to
     * be evaluated. */
    const int domain_size = component->attribute_domain_size(domain);
    int index = index_value_or_field.as_value();
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
      output_field = fn::make_constant_field(cpp_type, buffer);
      cpp_type.destruct(buffer);
    }
    else {
      output_field = fn::make_constant_field(cpp_type, cpp_type.default_value());
    }
  }
  else {
    /* Output default value if there is no geometry. */
    output_field = fn::make_constant_field(cpp_type, cpp_type.default_value());
  }

  output_attribute_field(params, std::move(output_field));
}

}  // namespace blender::nodes::node_geo_sample_index_cc

void register_node_type_geo_sample_index()
{
  namespace file_ns = blender::nodes::node_geo_sample_index_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SAMPLE_INDEX, "Sample Index", NODE_CLASS_GEOMETRY);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  node_type_storage(
      &ntype, "NodeGeometrySampleIndex", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
