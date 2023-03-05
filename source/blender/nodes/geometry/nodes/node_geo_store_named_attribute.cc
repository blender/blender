/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_enum_types.h"

#include "NOD_socket_search_link.hh"

#include "BKE_type_conversions.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_store_named_attribute_cc {

NODE_STORAGE_FUNCS(NodeGeometryStoreNamedAttribute)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().field_on_all();
  b.add_input<decl::String>(N_("Name")).is_attribute_name();
  b.add_input<decl::Vector>(N_("Value"), "Value_Vector").field_on_all();
  b.add_input<decl::Float>(N_("Value"), "Value_Float").field_on_all();
  b.add_input<decl::Color>(N_("Value"), "Value_Color").field_on_all();
  b.add_input<decl::Bool>(N_("Value"), "Value_Bool").field_on_all();
  b.add_input<decl::Int>(N_("Value"), "Value_Int").field_on_all();

  b.add_output<decl::Geometry>(N_("Geometry")).propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryStoreNamedAttribute *data = MEM_cnew<NodeGeometryStoreNamedAttribute>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryStoreNamedAttribute &storage = node_storage(*node);
  const eCustomDataType data_type = eCustomDataType(storage.data_type);

  bNodeSocket *socket_geometry = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *socket_name = socket_geometry->next->next;
  bNodeSocket *socket_vector = socket_name->next;
  bNodeSocket *socket_float = socket_vector->next;
  bNodeSocket *socket_color4f = socket_float->next;
  bNodeSocket *socket_boolean = socket_color4f->next;
  bNodeSocket *socket_int32 = socket_boolean->next;

  nodeSetSocketAvailability(ntree, socket_vector, ELEM(data_type, CD_PROP_FLOAT2, CD_PROP_FLOAT3));
  nodeSetSocketAvailability(ntree, socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(
      ntree, socket_color4f, ELEM(data_type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR));
  nodeSetSocketAvailability(ntree, socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, socket_int32, data_type == CD_PROP_INT32);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(2));
  search_link_ops_for_declarations(params, declaration.outputs.as_span().take_front(1));

  if (params.in_out() == SOCK_IN) {
    const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
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
  std::string name = params.extract_input<std::string>("Name");

  if (name.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }
  if (!bke::allow_procedural_attribute_access(name)) {
    params.error_message_add(NodeWarningType::Info, TIP_(bke::no_procedural_access_message));
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  params.used_named_attribute(name, NamedAttributeUsage::Write);

  const NodeGeometryStoreNamedAttribute &storage = node_storage(params.node());
  const eCustomDataType data_type = eCustomDataType(storage.data_type);
  const eAttrDomain domain = eAttrDomain(storage.domain);

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  GField field;
  switch (data_type) {
    case CD_PROP_FLOAT:
      field = params.get_input<Field<float>>("Value_Float");
      break;
    case CD_PROP_FLOAT2: {
      field = params.get_input<Field<float3>>("Value_Vector");
      field = bke::get_implicit_type_conversions().try_convert(field, CPPType::get<float2>());
      break;
    }
    case CD_PROP_FLOAT3:
      field = params.get_input<Field<float3>>("Value_Vector");
      break;
    case CD_PROP_COLOR:
      field = params.get_input<Field<ColorGeometry4f>>("Value_Color");
      break;
    case CD_PROP_BYTE_COLOR: {
      field = params.get_input<Field<ColorGeometry4f>>("Value_Color");
      field = bke::get_implicit_type_conversions().try_convert(field,
                                                               CPPType::get<ColorGeometry4b>());
      break;
    }
    case CD_PROP_BOOL:
      field = params.get_input<Field<bool>>("Value_Bool");
      break;
    case CD_PROP_INT32:
      field = params.get_input<Field<int>>("Value_Int");
      break;
    default:
      break;
  }

  std::atomic<bool> failure = false;

  /* Run on the instances component separately to only affect the top level of instances. */
  if (domain == ATTR_DOMAIN_INSTANCE) {
    if (geometry_set.has_instances()) {
      GeometryComponent &component = geometry_set.get_component_for_write(
          GEO_COMPONENT_TYPE_INSTANCES);
      if (!bke::try_capture_field_on_geometry(component, name, domain, selection, field)) {
        if (component.attribute_domain_size(domain) != 0) {
          failure.store(true);
        }
      }
    }
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      for (const GeometryComponentType type :
           {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE}) {
        if (geometry_set.has(type)) {
          GeometryComponent &component = geometry_set.get_component_for_write(type);
          if (!bke::try_capture_field_on_geometry(component, name, domain, selection, field)) {
            if (component.attribute_domain_size(domain) != 0) {
              failure.store(true);
            }
          }
        }
      }
    });
  }

  if (failure) {
    const char *domain_name = nullptr;
    RNA_enum_name_from_value(rna_enum_attribute_domain_items, domain, &domain_name);
    const char *type_name = nullptr;
    RNA_enum_name_from_value(rna_enum_attribute_type_items, data_type, &type_name);
    char *message = BLI_sprintfN(
        TIP_("Failed to write to attribute \"%s\" with domain \"%s\" and type \"%s\""),
        name.c_str(),
        TIP_(domain_name),
        TIP_(type_name));
    params.error_message_add(NodeWarningType::Warning, message);
    MEM_freeN(message);
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_store_named_attribute_cc

void register_node_type_geo_store_named_attribute()
{
  namespace file_ns = blender::nodes::node_geo_store_named_attribute_cc;
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_STORE_NAMED_ATTRIBUTE, "Store Named Attribute", NODE_CLASS_ATTRIBUTE);
  node_type_storage(&ntype,
                    "NodeGeometryStoreNamedAttribute",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  node_type_size(&ntype, 140, 100, 700);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
