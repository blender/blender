/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BKE_context.hh"
#include "BKE_type_conversions.hh"

#include "BLO_read_write.hh"

#include "DNA_modifier_types.h"

#include "NOD_geo_viewer.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_node.hh"
#include "ED_viewer_path.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_viewer_cc {

NODE_STORAGE_FUNCS(NodeGeometryViewer)

static void draw_float(uiLayout &layout, const float value)
{
  const std::string label = fmt::format("{:.5f}", value);
  layout.label(label, ICON_NONE);
}
static void draw_int(uiLayout &layout, const int value)
{
  const std::string label = fmt::format("{}", value);
  layout.label(label, ICON_NONE);
}
static void draw_bool(uiLayout &layout, const bool value)
{
  layout.label(value ? IFACE_("True") : IFACE_("False"), ICON_NONE);
}
static void draw_vector(uiLayout &layout, const float3 &value)
{
  uiLayout &col = layout.column(true);
  col.label(fmt::format("{}: {:.5f}", IFACE_("X"), value.x), ICON_NONE);
  col.label(fmt::format("{}: {:.5f}", IFACE_("Y"), value.y), ICON_NONE);
  col.label(fmt::format("{}: {:.5f}", IFACE_("Z"), value.z), ICON_NONE);
}
static void draw_color(uiLayout &layout, const ColorGeometry4f &value)
{
  uiLayout &col = layout.column(true);
  col.label(fmt::format("{}: {:.5f}", CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "R"), value.r), ICON_NONE);
  col.label(fmt::format("{}: {:.5f}", CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "G"), value.g), ICON_NONE);
  col.label(fmt::format("{}: {:.5f}", CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "B"), value.b), ICON_NONE);
  col.label(fmt::format("{}: {:.5f}", CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "A"), value.a), ICON_NONE);
}
static void draw_string(uiLayout &layout, const StringRef value)
{
  /* The node doesn't get wider than that anyway. */
  const int max_display_length = 200;
  layout.label(value.substr(0, max_display_length), ICON_NONE);
}
static bool draw_from_viewer_log_value(CustomSocketDrawParams &params,
                                       geo_eval_log::GeoTreeLog &tree_log)
{
  tree_log.ensure_viewer_node_logs();
  geo_eval_log::ViewerNodeLog *viewer_log = tree_log.viewer_node_logs.lookup_default(
      params.node.identifier, nullptr);
  if (!viewer_log) {
    return false;
  }
  const int socket_index = params.socket.index();
  const auto &storage = *static_cast<NodeGeometryViewer *>(params.node.storage);
  const NodeGeometryViewerItem &viewer_item = storage.items[socket_index];
  const geo_eval_log::ViewerNodeLog::Item *item_log = viewer_log->items.lookup_key_ptr_as(
      viewer_item.identifier);
  if (!item_log) {
    return false;
  }
  const bke::SocketValueVariant &value = item_log->value;
  if (!value.is_single()) {
    return false;
  }
  const GPointer single_value = value.get_single_ptr();
  if (single_value.is_type<float>()) {
    draw_float(params.layout, *single_value.get<float>());
    return true;
  }
  if (single_value.is_type<float3>()) {
    draw_vector(params.layout, *single_value.get<float3>());
    return true;
  }
  if (single_value.is_type<int>()) {
    draw_int(params.layout, *single_value.get<int>());
    return true;
  }
  if (single_value.is_type<bool>()) {
    draw_bool(params.layout, *single_value.get<bool>());
    return true;
  }
  if (single_value.is_type<std::string>()) {
    draw_string(params.layout, *single_value.get<std::string>());
    return true;
  }
  if (single_value.is_type<ColorGeometry4f>()) {
    draw_color(params.layout, *single_value.get<ColorGeometry4f>());
    return true;
  }
  return false;
}
static bool draw_generic_value_log(CustomSocketDrawParams &params, const GPointer &value)
{
  const CPPType &value_type = *params.socket.typeinfo->base_cpp_type;
  const CPPType &socket_base_cpp_type = *params.socket.typeinfo->base_cpp_type;
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  if (value_type != socket_base_cpp_type) {
    if (!conversions.is_convertible(value_type, socket_base_cpp_type)) {
      return false;
    }
  }
  BUFFER_FOR_CPP_TYPE_VALUE(socket_base_cpp_type, socket_value);
  conversions.convert_to_uninitialized(
      value_type, socket_base_cpp_type, value.get(), socket_value);
  BLI_SCOPED_DEFER([&]() { socket_base_cpp_type.destruct(socket_value); });
  switch (params.socket.type) {
    case SOCK_INT:
      draw_int(params.layout, *static_cast<int *>(socket_value));
      return true;
    case SOCK_FLOAT:
      draw_float(params.layout, *static_cast<float *>(socket_value));
      return true;
    case SOCK_VECTOR:
      draw_vector(params.layout, *static_cast<float3 *>(socket_value));
      return true;
    case SOCK_RGBA:
      draw_color(params.layout, *static_cast<ColorGeometry4f *>(socket_value));
      return true;
    case SOCK_BOOLEAN:
      draw_bool(params.layout, *static_cast<bool *>(socket_value));
      return true;
    default:
      return false;
  }
  return false;
}
static bool draw_from_socket_log_value(CustomSocketDrawParams &params,
                                       geo_eval_log::GeoTreeLog &tree_log)
{
  tree_log.ensure_socket_values();
  geo_eval_log::ValueLog *value_log = tree_log.find_socket_value_log(params.socket);
  if (!value_log) {
    return false;
  }
  if (const auto *generic_value_log = dynamic_cast<const geo_eval_log::GenericValueLog *>(
          value_log))
  {
    return draw_generic_value_log(params, generic_value_log->value);
  }
  if (const auto *string_value_log = dynamic_cast<const geo_eval_log::StringLog *>(value_log)) {
    draw_string(params.layout, string_value_log->value);
    return true;
  }
  return false;
}
static void draw_input_socket(CustomSocketDrawParams &params)
{
  SpaceNode *snode = CTX_wm_space_node(&params.C);
  if (!snode) {
    params.draw_standard(params.layout);
    return;
  }
  snode->edittree->ensure_topology_cache();
  const bNodeSocket &socket = params.socket;
  if (!socket.is_directly_linked()) {
    params.draw_standard(params.layout);
    return;
  }
  const geo_eval_log::ContextualGeoTreeLogs geo_tree_logs =
      geo_eval_log::GeoNodesLog::get_contextual_tree_logs(*snode);
  geo_eval_log::GeoTreeLog *tree_log = geo_tree_logs.get_main_tree_log(params.node);
  if (!tree_log) {
    params.draw_standard(params.layout);
    return;
  }
  if (draw_from_viewer_log_value(params, *tree_log)) {
    return;
  }
  if (draw_from_socket_log_value(params, *tree_log)) {
    return;
  }
  params.draw_standard(params.layout);
}

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();

  if (!node || !tree) {
    return;
  }

  b.add_default_layout();

  const NodeGeometryViewer &storage = node_storage(*node);
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryViewerItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name ? item.name : "";
    const std::string identifier = GeoViewerItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &tree->id, GeoViewerItemsAccessor::item_srna, &item, "name");
    if (socket_type_supports_fields(socket_type)) {
      input_decl.field_on_all();
    }
    input_decl.structure_type(StructureType::Dynamic);
    input_decl.custom_draw([](CustomSocketDrawParams &params) { draw_input_socket(params); });
  }

  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Dynamic);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryViewer *data = MEM_callocN<NodeGeometryViewer>(__func__);
  data->data_type_legacy = CD_PROP_FLOAT;
  data->domain = int8_t(AttrDomain::Auto);
  node->storage = data;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode &node = *ptr->data_as<bNode>();
  const NodeGeometryViewer &storage = node_storage(node);

  bool has_geometry_input = false;
  bool has_potential_field_input = false;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryViewerItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    if (socket_type == SOCK_GEOMETRY) {
      has_geometry_input = true;
    }
    else if (socket_type_supports_fields(socket_type)) {
      has_potential_field_input = true;
    }
  }

  if (has_geometry_input && has_potential_field_input) {
    layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode &node = *ptr->data_as<bNode>();
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);

  if (uiLayout *panel = layout->panel(C, "viewer_items", false, IFACE_("Viewer Items"))) {
    socket_items::ui::draw_items_list_with_operators<GeoViewerItemsAccessor>(
        C, panel, ntree, node);
    socket_items::ui::draw_active_item_props<GeoViewerItemsAccessor>(
        ntree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          panel->prop(item_ptr, "auto_remove", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.in_out == SOCK_OUT) {
    params.add_item(IFACE_("Value"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeViewer");
      const auto *item = socket_items::add_item_with_socket_type_and_name<GeoViewerItemsAccessor>(
          params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
      params.update_and_connect_available_socket(node, item->name);
      SpaceNode *snode = CTX_wm_space_node(&params.C);
      Main *bmain = CTX_data_main(&params.C);
      ed::viewer_path::activate_geometry_node(*bmain, *snode, node);
    });
    return;
  }
}

/**
 * Evaluates the first field after for each geometry as ".viewer" attribute. This attribute is used
 * by drawing code.
 */
static void log_viewer_attribute(const bNode &node, geo_eval_log::ViewerNodeLog &r_log)
{
  const auto &storage = *static_cast<NodeGeometryViewer *>(node.storage);
  const StringRef viewer_attribute_name = ".viewer";
  std::optional<int> last_geometry_identifier;
  for (const int i : IndexRange(storage.items_num)) {
    const bNodeSocket &bsocket = node.input_socket(i);
    const NodeGeometryViewerItem &item = storage.items[i];
    const bke::bNodeSocketType &type = *bsocket.typeinfo;

    if (type.type == SOCK_GEOMETRY) {
      last_geometry_identifier = item.identifier;
      continue;
    }
    if (!last_geometry_identifier) {
      continue;
    }
    if (!socket_type_supports_fields(type.type)) {
      continue;
    }
    /* Changing the `value` field doesn't change the hash or equality of the item. */
    GMutablePointer geometry_ptr = const_cast<bke::SocketValueVariant &>(
                                       r_log.items.lookup_key_as(*last_geometry_identifier).value)
                                       .get_single_ptr();
    GeometrySet &geometry = *geometry_ptr.get<GeometrySet>();
    const bke::SocketValueVariant &value = r_log.items.lookup_key_as(item.identifier).value;
    if (!(value.is_single() || value.is_context_dependent_field())) {
      continue;
    }
    const GField field = value.get<GField>();
    const AttrDomain domain_or_auto = AttrDomain(storage.domain);
    if (domain_or_auto == AttrDomain::Instance) {
      if (geometry.has_instances()) {
        bke::GeometryComponent &component =
            geometry.get_component_for_write<bke::InstancesComponent>();
        bke::try_capture_field_on_geometry(
            component, viewer_attribute_name, AttrDomain::Instance, field);
      }
    }
    else {
      geometry::foreach_real_geometry(geometry, [&](GeometrySet &geometry) {
        for (const bke::GeometryComponent::Type type :
             {bke::GeometryComponent::Type::Mesh,
              bke::GeometryComponent::Type::PointCloud,
              bke::GeometryComponent::Type::Curve,
              bke::GeometryComponent::Type::GreasePencil})
        {
          if (!geometry.has(type)) {
            continue;
          }
          bke::GeometryComponent &component = geometry.get_component_for_write(type);
          AttrDomain used_domain = domain_or_auto;
          if (domain_or_auto == AttrDomain::Auto) {
            if (const std::optional<AttrDomain> domain = bke::try_detect_field_domain(component,
                                                                                      field))
            {
              used_domain = *domain;
            }
            else {
              used_domain = AttrDomain::Point;
            }
          }
          bke::try_capture_field_on_geometry(component, viewer_attribute_name, used_domain, field);
        }
      });
    }
    /* Avoid overriding the viewer attribute with other fields.*/
    last_geometry_identifier.reset();
  }
}

static void geo_viewer_node_log_impl(const bNode &node,
                                     const Span<bke::SocketValueVariant *> input_values,
                                     geo_eval_log::ViewerNodeLog &r_log)
{
  const auto &storage = *static_cast<NodeGeometryViewer *>(node.storage);
  for (const int i : IndexRange(storage.items_num)) {
    void *src_value = input_values[i];
    if (!src_value) {
      continue;
    }
    const NodeGeometryViewerItem &item = storage.items[i];

    bke::SocketValueVariant &value = *input_values[i];
    if (value.is_single() && value.get_single_ptr().is_type<bke::GeometrySet>()) {
      value.get_single_ptr().get<bke::GeometrySet>()->ensure_owns_direct_data();
    }
    r_log.items.add_new({item.identifier, item.name, std::move(value)});
  }
  log_viewer_attribute(node, r_log);
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  SpaceNode *snode = CTX_wm_space_node(&params.C);
  if (snode) {
    if (std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
            ed::space_node::get_modifier_for_node_editor(*snode))
    {
      const NodesModifierData &nmd = *object_and_modifier->nmd;
      nmd.node_group->ensure_topology_cache();
      if (!(nmd.modifier.mode & eModifierMode_Realtime)) {
        NodeExtraInfoRow row;
        row.icon = ICON_ERROR;
        row.text = TIP_("Modifier disabled");
        row.tooltip = TIP_("The viewer does not work because the modifier is disabled");
        params.rows.append(std::move(row));
      }
      else if (!nmd.node_group->group_output_node()) {
        NodeExtraInfoRow row;
        row.icon = ICON_ERROR;
        row.text = TIP_("Missing output");
        row.tooltip = TIP_(
            "The viewer does not work because the node group used by the modifier has no output");
        params.rows.append(std::move(row));
      }
    }
  }
  const auto data_type = eCustomDataType(node_storage(params.node).data_type_legacy);
  if (ELEM(data_type, CD_PROP_QUATERNION, CD_PROP_FLOAT4X4)) {
    NodeExtraInfoRow row;
    row.icon = ICON_INFO;
    row.text = TIP_("No color overlay");
    row.tooltip = TIP_(
        "Rotation values can only be displayed with the text overlay in the 3D view");
    params.rows.append(std::move(row));
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<GeoViewerItemsAccessor>();
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<GeoViewerItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryViewer &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_mallocN<NodeGeometryViewer>(__func__);
  *dst_storage = src_storage;
  dst_node->storage = dst_storage;

  socket_items::copy_array<GeoViewerItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  NodeGeometryViewerItem *new_item = nullptr;
  const bool keep_link = socket_items::try_add_item_via_any_extend_socket<GeoViewerItemsAccessor>(
      params.ntree, params.node, params.node, params.link, std::nullopt, &new_item);
  if (new_item) {
    new_item->flag |= NODE_GEO_VIEWER_ITEM_FLAG_AUTO_REMOVE;
  }
  return keep_link;
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<GeoViewerItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<GeoViewerItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeViewer", GEO_NODE_VIEWER);
  ntype.ui_name = "Viewer";
  ntype.ui_description = "Display the input data in the Spreadsheet Editor";
  ntype.enum_name_legacy = "VIEWER";
  ntype.nclass = NODE_CLASS_OUTPUT;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryViewer", node_free_storage, node_copy_storage);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.insert_link = node_insert_link;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.no_muting = true;
  ntype.register_operators = node_operators;
  ntype.get_extra_info = node_extra_info;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_viewer_cc

namespace blender::nodes {

StructRNA *GeoViewerItemsAccessor::item_srna = &RNA_NodeGeometryViewerItem;

void GeoViewerItemsAccessor::blend_write_item(BlendWriter *writer,
                                              const NodeGeometryViewerItem &item)
{
  BLO_write_string(writer, item.name);
}

void GeoViewerItemsAccessor::blend_read_data_item(BlendDataReader *reader,
                                                  NodeGeometryViewerItem &item)
{
  BLO_read_string(reader, &item.name);
}

void geo_viewer_node_log(const bNode &node,
                         const Span<bke::SocketValueVariant *> input_values,
                         geo_eval_log::ViewerNodeLog &r_log)
{
  node_geo_viewer_cc::geo_viewer_node_log_impl(node, input_values, r_log);
}

}  // namespace blender::nodes
