/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "BKE_attribute_legacy_convert.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_type_conversions.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_store_named_attribute_cc {

NODE_STORAGE_FUNCS(NodeGeometryStoreNamedAttribute)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  const bNode *node = b.node_or_null();

  b.add_input<decl::Geometry>("Geometry")
      .description("Geometry to store a new attribute with the given name on");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::String>("Name").is_attribute_name().optional_label();

  if (node != nullptr) {
    const NodeGeometryStoreNamedAttribute &storage = node_storage(*node);
    const eCustomDataType data_type = eCustomDataType(storage.data_type);
    b.add_input(data_type, "Value").field_on_all();
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryStoreNamedAttribute *data = MEM_callocN<NodeGeometryStoreNamedAttribute>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = int8_t(AttrDomain::Point);
  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);
  search_link_ops_for_declarations(params, declaration.outputs);

  if (params.in_out() == SOCK_IN) {
    const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
        eNodeSocketDatatype(params.other_socket().type));
    if (type && *type != CD_PROP_STRING) {
      /* The input and output sockets have the same name. */
      params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeStoreNamedAttribute");
        node_storage(node).data_type = *type;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string name = params.extract_input<std::string>("Name");

  if (name.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }
  if (!bke::allow_procedural_attribute_access(name)) {
    params.error_message_add(NodeWarningType::Info, TIP_(bke::no_procedural_access_message));
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }
  if (bke::attribute_name_is_anonymous(name)) {
    params.error_message_add(NodeWarningType::Info,
                             TIP_("Anonymous attributes cannot be created here"));
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  params.used_named_attribute(name, NamedAttributeUsage::Write);

  const NodeGeometryStoreNamedAttribute &storage = node_storage(params.node());
  const eCustomDataType cd_type = eCustomDataType(storage.data_type);
  const bke::AttrType data_type = *bke::custom_data_type_to_attr_type(cd_type);
  const AttrDomain domain = AttrDomain(storage.domain);

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  GField field = params.extract_input<GField>("Value");
  if (ELEM(data_type, bke::AttrType::Float2, bke::AttrType::ColorByte, bke::AttrType::Int8)) {
    field = bke::get_implicit_type_conversions().try_convert(
        std::move(field), bke::attribute_type_to_cpp_type(data_type));
  }

  std::atomic<bool> failure = false;

  /* Run on the instances component separately to only affect the top level of instances. */
  if (domain == AttrDomain::Instance) {
    if (geometry_set.has_instances()) {
      GeometryComponent &component = geometry_set.get_component_for_write(
          GeometryComponent::Type::Instance);

      if (name == "position" && data_type == bke::AttrType::Float3) {
        /* Special case for "position" which is no longer an attribute on instances. */
        bke::Instances &instances = *geometry_set.get_instances_for_write();
        bke::InstancesFieldContext context(instances);
        fn::FieldEvaluator evaluator{context, instances.instances_num()};
        evaluator.set_selection(selection);
        evaluator.add_with_destination(field, bke::instance_position_varray_for_write(instances));
        evaluator.evaluate();
      }
      else {
        if (!bke::try_capture_field_on_geometry(component, name, domain, selection, field)) {
          if (component.attribute_domain_size(domain) != 0) {
            failure.store(true);
          }
        }
      }
    }
  }
  else {
    geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
      for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                                 GeometryComponent::Type::PointCloud,
                                                 GeometryComponent::Type::Curve,
                                                 GeometryComponent::Type::GreasePencil})
      {
        if (geometry_set.has(type)) {
          GeometryComponent &component = geometry_set.get_component_for_write(type);
          if (bke::try_capture_field_on_geometry(component, name, domain, selection, field)) {
            if (component.type() == GeometryComponent::Type::Mesh) {
              Mesh &mesh = *geometry_set.get_mesh_for_write();
              bke::mesh_ensure_default_color_attribute_on_add(mesh, name, domain, data_type);
            }
          }
          else if (component.attribute_domain_size(domain) != 0) {
            failure.store(true);
          }
        }
      }
    });
  }

  if (failure) {
    const char *domain_name = nullptr;
    RNA_enum_name_from_value(rna_enum_attribute_domain_items, int(domain), &domain_name);
    const char *type_name = nullptr;
    RNA_enum_name_from_value(rna_enum_attribute_type_items, cd_type, &type_name);
    const std::string message = fmt::format(
        fmt::runtime(
            TIP_("Failed to write to attribute \"{}\" with domain \"{}\" and type \"{}\"")),
        name,
        TIP_(domain_name),
        TIP_(type_name));
    params.error_message_add(NodeWarningType::Warning, message);
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "data_type",
      "Data Type",
      "Type of data stored in attribute",
      rna_enum_attribute_type_items,
      NOD_storage_enum_accessors(data_type),
      CD_PROP_FLOAT,
      [](bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        return enum_items_filter(rna_enum_attribute_type_items,
                                 enums::generic_attribute_type_supported);
      });

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Which domain to store the data in",
                    rna_enum_attribute_domain_items,
                    NOD_storage_enum_accessors(domain),
                    int(AttrDomain::Point));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeStoreNamedAttribute", GEO_NODE_STORE_NAMED_ATTRIBUTE);
  ntype.ui_name = "Store Named Attribute";
  ntype.ui_description =
      "Store the result of a field on a geometry as an attribute with the specified name";
  ntype.enum_name_legacy = "STORE_NAMED_ATTRIBUTE";
  ntype.nclass = NODE_CLASS_ATTRIBUTE;
  blender::bke::node_type_storage(ntype,
                                  "NodeGeometryStoreNamedAttribute",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  blender::bke::node_type_size(ntype, 140, 100, 700);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_store_named_attribute_cc
