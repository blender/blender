/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_string_utf8.h"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "NOD_geo_foreach_geometry_element.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_library.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"

#include "fmt/core.h"

namespace blender::nodes::node_geo_foreach_geometry_element_cc {

/** Shared between zone input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node_id) {
    return;
  }
  const bool is_zone_input_node = current_node->type_legacy ==
                                  GEO_NODE_FOREACH_GEOMETRY_ELEMENT_INPUT;
  bNode &output_node = const_cast<bNode &>(*zone->output_node());
  PointerRNA output_node_ptr = RNA_pointer_create_discrete(
      current_node_ptr->owner_id, &RNA_Node, &output_node);
  auto &storage = *static_cast<NodeGeometryForeachGeometryElementOutput *>(output_node.storage);

  if (is_zone_input_node) {
    if (uiLayout *panel = layout->panel(C, "input", false, IFACE_("Input Fields"))) {
      socket_items::ui::draw_items_list_with_operators<ForeachGeometryElementInputItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ForeachGeometryElementInputItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            panel->use_property_split_set(true);
            panel->use_property_decorate_set(false);
            panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          });
    }
  }
  else {
    if (uiLayout *panel = layout->panel(C, "main_items", false, IFACE_("Main Geometry"))) {
      socket_items::ui::draw_items_list_with_operators<ForeachGeometryElementMainItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ForeachGeometryElementMainItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            panel->use_property_split_set(true);
            panel->use_property_decorate_set(false);
            panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          });
    }
    if (uiLayout *panel = layout->panel(
            C, "generation_items", false, IFACE_("Generated Geometry")))
    {
      socket_items::ui::draw_items_list_with_operators<
          ForeachGeometryElementGenerationItemsAccessor>(C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ForeachGeometryElementGenerationItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            NodeForeachGeometryElementGenerationItem &active_item =
                storage.generation_items.items[storage.generation_items.active_index];
            panel->use_property_split_set(true);
            panel->use_property_decorate_set(false);
            panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
            if (active_item.socket_type != SOCK_GEOMETRY) {
              panel->prop(item_ptr, "domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
            }
          });
    }
  }

  layout->prop(&output_node_ptr, "inspection_index", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

namespace input_node {

NODE_STORAGE_FUNCS(NodeGeometryForeachGeometryElementInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();

  b.add_default_layout();

  if (!node || !tree) {
    return;
  }

  const NodeGeometryForeachGeometryElementInput &storage = node_storage(*node);
  const bNode *output_node = tree->node_by_id(storage.output_node_id);
  const auto &output_storage = output_node ?
                                   static_cast<const NodeGeometryForeachGeometryElementOutput *>(
                                       output_node->storage) :
                                   nullptr;

  b.add_output<decl::Int>("Index").description(
      "Index of the element in the source geometry. Note that the same index can occur more than "
      "once when iterating over multiple components at once");

  b.add_output<decl::Geometry>("Element")
      .description(
          "Single-element geometry for the current iteration. Note that it can be quite "
          "inefficient to split up large geometries into many small geometries")
      .propagate_all()
      .available(output_storage && AttrDomain(output_storage->domain) != AttrDomain::Corner);

  b.add_input<decl::Geometry>("Geometry").description("Geometry whose elements are iterated over");

  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("Selection on the iteration domain");

  if (output_storage) {
    for (const int i : IndexRange(output_storage->input_items.items_num)) {
      const NodeForeachGeometryElementInputItem &item = output_storage->input_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier =
          ForeachGeometryElementInputItemsAccessor::socket_identifier_for_item(item);
      b.add_input(socket_type, name, identifier)
          .socket_name_ptr(
              &tree->id, ForeachGeometryElementInputItemsAccessor::item_srna, &item, "name")
          .description("Field that is evaluated on the iteration domain")
          .field_on_all();
      b.add_output(socket_type, name, identifier)
          .align_with_previous()
          .description("Evaluated field value for the current element");
    }
  }

  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Dynamic);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);
  const NodeGeometryForeachGeometryElementInput &storage = node_storage(node);
  bNode *output_node = tree.node_by_id(storage.output_node_id);

  PointerRNA output_node_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Node, output_node);
  layout->prop(&output_node_ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryForeachGeometryElementInput *data =
      MEM_callocN<NodeGeometryForeachGeometryElementInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(
      label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "For Each Element"), label_maxncpy);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  bNode *output_node = params.ntree.node_by_id(node_storage(params.node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<
      ForeachGeometryElementInputItemsAccessor>(
      params.ntree, params.node, *output_node, params.link);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeForeachGeometryElementInput", GEO_NODE_FOREACH_GEOMETRY_ELEMENT_INPUT);
  ntype.ui_name = "For Each Geometry Element Input";
  ntype.enum_name_legacy = "FOREACH_GEOMETRY_ELEMENT_INPUT";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.labelfunc = node_label;
  ntype.insert_link = node_insert_link;
  ntype.gather_link_search_ops = nullptr;
  ntype.no_muting = true;
  blender::bke::node_type_storage(ntype,
                                  "NodeGeometryForeachGeometryElementInput",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace input_node

namespace output_node {

NODE_STORAGE_FUNCS(NodeGeometryForeachGeometryElementOutput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_output<decl::Geometry>("Geometry")
      .description(
          "The original input geometry with potentially new attributes that are output by the "
          "zone");

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeGeometryForeachGeometryElementOutput &storage = node_storage(*node);
    for (const int i : IndexRange(storage.main_items.items_num)) {
      const NodeForeachGeometryElementMainItem &item = storage.main_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      std::string identifier = ForeachGeometryElementMainItemsAccessor::socket_identifier_for_item(
          item);
      b.add_input(socket_type, name, identifier)
          .socket_name_ptr(
              &tree->id, ForeachGeometryElementMainItemsAccessor::item_srna, &item, "name")
          .description(
              "Attribute value that will be stored for the current element on the main geometry");
      b.add_output(socket_type, name, identifier)
          .align_with_previous()
          .field_on({0})
          .description("Attribute on the geometry above");
    }
    b.add_input<decl::Extend>("", "__extend__main");
    b.add_output<decl::Extend>("", "__extend__main").align_with_previous();

    auto &panel = b.add_panel("Generated");

    int previous_output_geometry_index = -1;
    int previous_input_geometry_index = -1;
    for (const int i : IndexRange(storage.generation_items.items_num)) {
      const NodeForeachGeometryElementGenerationItem &item = storage.generation_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      if (socket_type == SOCK_GEOMETRY && i > 0) {
        panel.add_separator();
      }
      const StringRef name = item.name ? item.name : "";
      std::string identifier =
          ForeachGeometryElementGenerationItemsAccessor::socket_identifier_for_item(item);
      auto &input_decl = panel.add_input(socket_type, name, identifier)
                             .socket_name_ptr(
                                 &tree->id,
                                 ForeachGeometryElementGenerationItemsAccessor::item_srna,
                                 &item,
                                 "name");
      auto &output_decl = panel.add_output(socket_type, name, identifier).align_with_previous();
      if (socket_type == SOCK_GEOMETRY) {
        previous_input_geometry_index = input_decl.index();
        previous_output_geometry_index = output_decl.index();

        input_decl.description(
            "Geometry generated in the current iteration. Will be joined with geometries from all "
            "other iterations");
        output_decl.description("Result of joining generated geometries from each iteration");
      }
      else {
        if (previous_output_geometry_index > 0) {
          input_decl.description("Field that will be stored as attribute on the geometry above");
          input_decl.field_on({previous_input_geometry_index});
          output_decl.field_on({previous_output_geometry_index});
        }
        output_decl.description("Attribute on the geometry above");
      }
    }
    panel.add_input<decl::Extend>("", "__extend__generation");
    panel.add_output<decl::Extend>("", "__extend__generation").align_with_previous();
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryForeachGeometryElementOutput *data =
      MEM_callocN<NodeGeometryForeachGeometryElementOutput>(__func__);

  data->generation_items.items = MEM_calloc_arrayN<NodeForeachGeometryElementGenerationItem>(
      1, __func__);
  NodeForeachGeometryElementGenerationItem &item = data->generation_items.items[0];
  item.name = BLI_strdup(DATA_("Geometry"));
  item.socket_type = SOCK_GEOMETRY;
  item.identifier = data->generation_items.next_identifier++;
  data->generation_items.items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ForeachGeometryElementInputItemsAccessor>(*node);
  socket_items::destruct_array<ForeachGeometryElementGenerationItemsAccessor>(*node);
  socket_items::destruct_array<ForeachGeometryElementMainItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryForeachGeometryElementOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometryForeachGeometryElementOutput>(__func__,
                                                                              src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ForeachGeometryElementInputItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<ForeachGeometryElementGenerationItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<ForeachGeometryElementMainItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (!socket_items::try_add_item_via_any_extend_socket<ForeachGeometryElementMainItemsAccessor>(
          params.ntree, params.node, params.node, params.link, "__extend__main"))
  {
    return false;
  }
  return socket_items::try_add_item_via_any_extend_socket<
      ForeachGeometryElementGenerationItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ForeachGeometryElementInputItemsAccessor>();
  socket_items::ops::make_common_operators<ForeachGeometryElementMainItemsAccessor>();
  socket_items::ops::make_common_operators<ForeachGeometryElementGenerationItemsAccessor>();
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  const NodeGeometryForeachGeometryElementOutput &storage = node_storage(params.node);
  if (storage.generation_items.items_num > 0) {
    if (storage.generation_items.items[0].socket_type != SOCK_GEOMETRY) {
      NodeExtraInfoRow row;
      row.text = RPT_("Missing Geometry");
      row.tooltip = TIP_("Each output field has to correspond to a geometry that is above it");
      row.icon = ICON_ERROR;
      params.rows.append(std::move(row));
    }
  }
}

static std::pair<bNode *, bNode *> add_foreach_zone(LinkSearchOpParams &params)
{
  bNode &input_node = params.add_node("GeometryNodeForeachGeometryElementInput");
  bNode &output_node = params.add_node("GeometryNodeForeachGeometryElementOutput");
  output_node.location[0] = 300;

  auto &input_storage = *static_cast<NodeGeometryForeachGeometryElementInput *>(
      input_node.storage);
  input_storage.output_node_id = output_node.identifier;

  return {&input_node, &output_node};
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  const eNodeSocketDatatype type = eNodeSocketDatatype(other_socket.type);
  if (type != SOCK_GEOMETRY) {
    return;
  }
  if (other_socket.in_out == SOCK_OUT) {
    params.add_item_full_name(IFACE_("For Each Element"), [](LinkSearchOpParams &params) {
      const auto [input_node, output_node] = add_foreach_zone(params);
      params.update_and_connect_available_socket(*input_node, "Geometry");
    });
  }
  else {
    params.add_item_full_name(
        fmt::format(IFACE_("For Each Element {} Main"), UI_MENU_ARROW_SEP),
        [](LinkSearchOpParams &params) {
          const auto [input_node, output_node] = add_foreach_zone(params);
          socket_items::clear<ForeachGeometryElementGenerationItemsAccessor>(*output_node);
          params.update_and_connect_available_socket(*output_node, "Geometry");
        });

    params.add_item_full_name(
        fmt::format(IFACE_("For Each Element {} Generated"), UI_MENU_ARROW_SEP),
        [](LinkSearchOpParams &params) {
          const auto [input_node, output_node] = add_foreach_zone(params);
          params.node_tree.ensure_topology_cache();
          bke::node_add_link(params.node_tree,
                             *output_node,
                             output_node->output_socket(2),
                             params.node,
                             params.socket);
        });
  }
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<ForeachGeometryElementInputItemsAccessor>(&writer, node);
  socket_items::blend_write<ForeachGeometryElementGenerationItemsAccessor>(&writer, node);
  socket_items::blend_write<ForeachGeometryElementMainItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<ForeachGeometryElementInputItemsAccessor>(&reader, node);
  socket_items::blend_read_data<ForeachGeometryElementMainItemsAccessor>(&reader, node);
  socket_items::blend_read_data<ForeachGeometryElementGenerationItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype,
                     "GeometryNodeForeachGeometryElementOutput",
                     GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT);
  ntype.ui_name = "For Each Geometry Element Output";
  ntype.enum_name_legacy = "FOREACH_GEOMETRY_ELEMENT_OUTPUT";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = input_node::node_label;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.get_extra_info = node_extra_info;
  ntype.no_muting = true;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryForeachGeometryElementOutput", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace output_node

}  // namespace blender::nodes::node_geo_foreach_geometry_element_cc

namespace blender::nodes {

StructRNA *ForeachGeometryElementInputItemsAccessor::item_srna =
    &RNA_ForeachGeometryElementInputItem;

void ForeachGeometryElementInputItemsAccessor::blend_write_item(BlendWriter *writer,
                                                                const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ForeachGeometryElementInputItemsAccessor::blend_read_data_item(BlendDataReader *reader,
                                                                    ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *ForeachGeometryElementMainItemsAccessor::item_srna =
    &RNA_ForeachGeometryElementMainItem;

void ForeachGeometryElementMainItemsAccessor::blend_write_item(BlendWriter *writer,
                                                               const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ForeachGeometryElementMainItemsAccessor::blend_read_data_item(BlendDataReader *reader,
                                                                   ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *ForeachGeometryElementGenerationItemsAccessor::item_srna =
    &RNA_ForeachGeometryElementGenerationItem;

void ForeachGeometryElementGenerationItemsAccessor::blend_write_item(BlendWriter *writer,
                                                                     const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ForeachGeometryElementGenerationItemsAccessor::blend_read_data_item(BlendDataReader *reader,
                                                                         ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
