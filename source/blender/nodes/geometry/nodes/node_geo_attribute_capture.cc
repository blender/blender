/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_attribute_math.hh"

#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_capture_cc {

NODE_STORAGE_FUNCS(NodeGeometryAttributeCapture)

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  b.add_input<decl::Geometry>("Geometry");
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    b.add_input(data_type, "Value").field_on_all();
  }

  b.add_output<decl::Geometry>("Geometry").propagate_all();
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    b.add_output(data_type, "Attribute").field_on_all();
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryAttributeCapture *data = MEM_cnew<NodeGeometryAttributeCapture>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = int8_t(AttrDomain::Point);

  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);
  search_link_ops_for_declarations(params, declaration.outputs);

  const bNodeType &node_type = params.node_type();
  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    if (params.in_out() == SOCK_OUT) {
      params.add_item(IFACE_("Attribute"), [node_type, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Attribute");
      });
    }
    else {
      params.add_item(IFACE_("Value"), [node_type, type](LinkSearchOpParams &params) {
        bNode &node = params.add_node(node_type);
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
  }
}

static void clean_unused_attributes(const AnonymousAttributePropagationInfo &propagation_info,
                                    const Set<AttributeIDRef> &skip,
                                    GeometryComponent &component)
{
  std::optional<MutableAttributeAccessor> attributes = component.attributes_for_write();
  if (!attributes.has_value()) {
    return;
  }

  Vector<std::string> unused_ids;
  attributes->for_all([&](const AttributeIDRef &id, const AttributeMetaData /*meta_data*/) {
    if (!id.is_anonymous()) {
      return true;
    }
    if (skip.contains(id)) {
      return true;
    }
    if (propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    unused_ids.append(id.name());
    return true;
  });

  for (const std::string &unused_id : unused_ids) {
    attributes->remove(unused_id);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (!params.output_is_required("Geometry")) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("The attribute output cannot be used without the geometry output"));
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryAttributeCapture &storage = node_storage(params.node());
  const AttrDomain domain = AttrDomain(storage.domain);

  AnonymousAttributeIDPtr attribute_id = params.get_output_anonymous_attribute_id_if_needed(
      "Attribute");
  if (!attribute_id) {
    params.set_output("Geometry", geometry_set);
    params.set_default_remaining_outputs();
    return;
  }

  const GField field = params.extract_input<GField>("Value");

  const auto capture_on = [&](GeometryComponent &component) {
    bke::try_capture_field_on_geometry(component, *attribute_id, domain, field);
    /* Changing of the anonymous attributes may require removing attributes that are no longer
     * needed. */
    clean_unused_attributes(
        params.get_output_propagation_info("Geometry"), {*attribute_id}, component);
  };

  /* Run on the instances component separately to only affect the top level of instances. */
  if (domain == AttrDomain::Instance) {
    if (geometry_set.has_instances()) {
      capture_on(geometry_set.get_component_for_write(GeometryComponent::Type::Instance));
    }
  }
  else {
    static const Array<GeometryComponent::Type> types = {GeometryComponent::Type::Mesh,
                                                         GeometryComponent::Type::PointCloud,
                                                         GeometryComponent::Type::Curve,
                                                         GeometryComponent::Type::GreasePencil};

    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      for (const GeometryComponent::Type type : types) {
        if (geometry_set.has(type)) {
          capture_on(geometry_set.get_component_for_write(type));
        }
      }
    });
  }

  params.set_output("Geometry", geometry_set);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Type of data stored in attribute",
                    rna_enum_attribute_type_items,
                    NOD_storage_enum_accessors(data_type),
                    CD_PROP_FLOAT,
                    enums::attribute_type_type_with_socket_fn);

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Which domain to store the data in",
                    rna_enum_attribute_domain_items,
                    NOD_storage_enum_accessors(domain),
                    int8_t(AttrDomain::Point),
                    enums::domain_experimental_grease_pencil_version3_fn,
                    true);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_CAPTURE_ATTRIBUTE, "Capture Attribute", NODE_CLASS_ATTRIBUTE);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeCapture",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_attribute_capture_cc
